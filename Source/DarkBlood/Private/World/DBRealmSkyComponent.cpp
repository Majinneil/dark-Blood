#include "World/DBRealmSkyComponent.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "DarkBlood.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "World/DBRealmMoodComponent.h"
#include "World/DBWorldStateComponent.h"

namespace
{
	using FSkyKey = UDBRealmSkyComponent::FSkyKey;

	// The day in keys (hour, sun elevation, sun lux, sun kelvin, sky light, fog, dusk weight, moon). Dusk peaks at
	// 18:20: the sun 2 degrees above the horizon - its light crosses the whole atmosphere (red, grazing, long shadows),
	// the sky burns red-violet, the fog thickens in the valleys. Lux values are the project's exposure scale (noon 10).
	const FSkyKey Keys[] = {
		{0.f, -40.f, 0.f, 4000.f, 0.5f, 1.2f, 0.f, 1.f},
		{4.8f, -12.f, 0.f, 4000.f, 0.55f, 1.3f, 0.f, 1.f},
		{5.8f, -2.f, 3.f, 2400.f, 0.8f, 1.7f, 0.5f, 0.6f},
		{6.7f, 4.f, 5.f, 3000.f, 0.8f, 1.5f, 0.35f, 0.f},
		{8.f, 18.f, 8.f, 4300.f, 1.f, 1.15f, 0.05f, 0.f},
		{12.f, 58.f, 10.f, 5800.f, 1.f, 1.f, 0.f, 0.f},
		{16.f, 26.f, 9.f, 5000.f, 1.f, 1.f, 0.f, 0.f},
		{17.4f, 7.f, 8.f, 3300.f, 1.f, 1.25f, 0.45f, 0.2f},
		{18.33f, -1.5f, 7.f, 2900.f, 1.25f, 1.7f, 1.f, 0.85f},
		{19.2f, -4.f, 1.5f, 2000.f, 0.8f, 1.6f, 0.75f, 1.f},
		{20.5f, -15.f, 0.f, 4000.f, 0.55f, 1.3f, 0.15f, 1.f},
		{24.f, -40.f, 0.f, 4000.f, 0.5f, 1.2f, 0.f, 1.f},
	};

	FSkyKey Lerp(const FSkyKey& A, const FSkyKey& B, float Alpha)
	{
		FSkyKey Out;
		Out.Hour = FMath::Lerp(A.Hour, B.Hour, Alpha);
		Out.SunElevation = FMath::Lerp(A.SunElevation, B.SunElevation, Alpha);
		Out.SunLux = FMath::Lerp(A.SunLux, B.SunLux, Alpha);
		Out.SunKelvin = FMath::Lerp(A.SunKelvin, B.SunKelvin, Alpha);
		Out.SkyLight = FMath::Lerp(A.SkyLight, B.SkyLight, Alpha);
		Out.Fog = FMath::Lerp(A.Fog, B.Fog, Alpha);
		Out.Dusk = FMath::Lerp(A.Dusk, B.Dusk, Alpha);
		Out.Moon = FMath::Lerp(A.Moon, B.Moon, Alpha);
		return Out;
	}

	/** Sun path: rises in the east (+Y), sets in the west (-Y), 15 degrees per hour. Returns the light's rotation. */
	FRotator SunRotation(float Hour, float Elevation, float AzimuthOffset = 0.f)
	{
		const float Azimuth = FMath::DegreesToRadians(90.f + (Hour - 6.f) * 15.f + AzimuthOffset);
		const float Height = FMath::DegreesToRadians(Elevation);
		const FVector ToSun(FMath::Cos(Height) * FMath::Cos(Azimuth), FMath::Cos(Height) * FMath::Sin(Azimuth), FMath::Sin(Height));
		return (-ToSun).Rotation();
	}

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float Alpha) { return FMath::Lerp(A, B, Alpha); }
}

UDBRealmSkyComponent::UDBRealmSkyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UDBRealmSkyComponent::FSkyKey UDBRealmSkyComponent::Evaluate(float Hour)
{
	Hour = FMath::Fmod(FMath::Fmod(Hour, 24.f) + 24.f, 24.f);
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Keys); ++Index)
	{
		if (Hour <= Keys[Index].Hour)
		{
			const FSkyKey& A = Keys[Index - 1];
			const FSkyKey& B = Keys[Index];
			// Smoothstep between keys: no visible kinks when the clock runs.
			const float Alpha = FMath::SmoothStep(0.f, 1.f, (Hour - A.Hour) / FMath::Max(B.Hour - A.Hour, KINDA_SMALL_NUMBER));
			return Lerp(A, B, Alpha);
		}
	}
	return Keys[0];
}

void UDBRealmSkyComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}
	Configure();
}

void UDBRealmSkyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PostProcess)
	{
		PostProcess->Destroy();
		PostProcess = nullptr;
	}
	if (Moon)
	{
		Moon->Destroy();
		Moon = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UDBRealmSkyComponent::Configure()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (Light && Light->IsUsedAsAtmosphereSunLight() && Light->GetAtmosphereSunLightIndex() == 0)
		{
			Sun = Light;
			// Fully dynamic sun: Lumen GI + virtual shadow maps, a soft disk, light shafts through the fog.
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetUseTemperature(true);
			Light->SetLightSourceAngle(1.2f);
			Light->SetVolumetricScatteringIntensity(1.6f);
			// No light-shaft bloom: it smears a streak across the ground when the sun sits on the horizon.
			Light->bEnableLightShaftBloom = false;
			Light->bCastCloudShadows = true;
			Light->CloudShadowStrength = 0.6f;
			Light->MarkRenderStateDirty();
			break;
		}
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		SkyLight = It->GetLightComponent();
		// Real-time capture of the atmosphere: the ambient light takes the sky's color at every hour (no baked cubemap).
		SkyLight->SetRealTimeCapture(true);
		SkyLight->bLowerHemisphereIsBlack = false;
		SkyLight->SetLowerHemisphereColor(FLinearColor(0.05f, 0.035f, 0.06f));
		SkyLight->SetVolumetricScatteringIntensity(1.f);
		break;
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		UExponentialHeightFogComponent* Component = It->GetComponent();
		Fog = Component;
		// Layer 1: thin haze over the whole land, thinning quickly with height - mountain tops stay clear.
		Component->SetFogHeightFalloff(0.35f);
		Component->SetFogMaxOpacity(0.97f);
		Component->SetStartDistance(1500.f);
		// Layer 2: dense mist lying in the valleys (Atmospheric Perspective). It sits 25 m above sea level and thins
		// out within a few tens of metres: settlements and rivers sink into it, ridges rise out of it.
		Component->SecondFogData.FogDensity = 0.06f;
		Component->SecondFogData.FogHeightFalloff = 0.9f;
		Component->SecondFogData.FogHeightOffset = 2500.f;
		// Sun glow through the fog towards the light (directional inscattering).
		Component->SetDirectionalInscatteringExponent(6.f);
		Component->SetDirectionalInscatteringStartDistance(4000.f);
		// Volumetric fog: forward scattering lets sun shafts and lantern halos glow; limited range for the frame rate.
		Component->SetVolumetricFog(true);
		Component->SetVolumetricFogScatteringDistribution(0.75f);
		Component->SetVolumetricFogAlbedo(FColor(235, 222, 230));
		Component->SetVolumetricFogExtinctionScale(1.f);
		Component->SetVolumetricFogDistance(9000.f);
		Component->SetVolumetricFogStartDistance(300.f);
		Component->MarkRenderStateDirty();
		break;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (USkyAtmosphereComponent* Sky = It->FindComponentByClass<USkyAtmosphereComponent>())
		{
			Atmosphere = Sky;
			break;
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UVolumetricCloudComponent* Cloud = It->FindComponentByClass<UVolumetricCloudComponent>())
		{
			Clouds = Cloud;
			// A cloud deck above the peaks, lit from below by the setting sun; half the samples for the frame rate.
			Cloud->SetLayerBottomAltitude(2.5f);
			Cloud->SetLayerHeight(6.f);
			Cloud->SetViewSampleCountScale(0.6f);
			Cloud->SetReflectionViewSampleCountScale(0.25f);
			Cloud->SetShadowViewSampleCountScale(0.5f);
			Cloud->SetSkyLightCloudBottomOcclusion(0.6f);
			break;
		}
	}
	if (AActor* Owner = GetOwner())
	{
		Mood = Owner->FindComponentByClass<UDBRealmMoodComponent>();
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	// The blood moon: a second atmosphere light (index 1). Its huge red disk is drawn by the sky atmosphere; its light
	// is weak and casts no shadows (cheap), it only tints the night.
	if (ADirectionalLight* MoonLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform::Identity, Params))
	{
		MoonLight->SetReplicates(false);
		UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(MoonLight->GetLightComponent());
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetAtmosphereSunLight(true);
		Light->SetAtmosphereSunLightIndex(1);
		Light->SetLightSourceAngle(7.f);
		Light->SetLightColor(FLinearColor(1.f, 0.16f, 0.12f));
		Light->SetAtmosphereSunDiskColorScale(FLinearColor(0.35f, 0.025f, 0.02f));
		Light->SetCastShadows(false);
		Light->SetIntensity(0.f);
		Moon = MoonLight;
	}
	PostProcess = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
	if (PostProcess)
	{
		PostProcess->SetReplicates(false);
		PostProcess->bUnbound = true;
		// Above the region mood volume (it owns global saturation, gain and vignette), below authored volumes.
		PostProcess->Priority = -0.5f;
		FPostProcessSettings& S = PostProcess->Settings;
		// Lumen everywhere: GI and reflections are traced every frame - nothing is baked, the dusk light is live.
		S.bOverride_DynamicGlobalIlluminationMethod = true;
		S.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		S.bOverride_ReflectionMethod = true;
		S.ReflectionMethod = EReflectionMethod::Lumen;
		S.bOverride_LumenSceneLightingQuality = true;
		S.LumenSceneLightingQuality = 1.f;
		S.bOverride_LumenFinalGatherQuality = true;
		S.LumenFinalGatherQuality = 1.f;
		S.bOverride_LumenMaxTraceDistance = true;
		S.LumenMaxTraceDistance = 20000.f;
		S.bOverride_LumenSkylightLeaking = true;
		S.LumenSkylightLeaking = 0.05f;
		// Tonemapper (ACES film curve): a deeper toe for rich shadows, a soft shoulder that keeps lantern flames from
		// clipping.
		S.bOverride_FilmSlope = true;
		S.FilmSlope = 0.86f;
		S.bOverride_FilmToe = true;
		S.FilmToe = 0.6f;
		S.bOverride_FilmShoulder = true;
		S.FilmShoulder = 0.3f;
		S.bOverride_FilmBlackClip = true;
		S.FilmBlackClip = 0.f;
		S.bOverride_FilmWhiteClip = true;
		S.FilmWhiteClip = 0.04f;
		// Bloom on bright sources only (lanterns, the sun's edge, the moon), wide and soft.
		S.bOverride_BloomIntensity = true;
		S.bOverride_BloomThreshold = true;
		S.BloomThreshold = -1.f;
		S.bOverride_BloomSizeScale = true;
		S.BloomSizeScale = 4.f;
		// Local exposure: lanterns and the bright sky keep detail, shadows are not lifted to grey.
		S.bOverride_LocalExposureHighlightContrastScale = true;
		S.LocalExposureHighlightContrastScale = 0.75f;
		S.bOverride_LocalExposureShadowContrastScale = true;
		S.LocalExposureShadowContrastScale = 0.9f;
		S.bOverride_AutoExposureBias = true;
		S.bOverride_AutoExposureMinBrightness = true;
		S.bOverride_AutoExposureMaxBrightness = true;
		S.bOverride_AutoExposureSpeedUp = true;
		S.AutoExposureSpeedUp = 1.5f;
		S.bOverride_AutoExposureSpeedDown = true;
		S.AutoExposureSpeedDown = 1.f;
		// Split toning: cool violet shadows against warm highlights (the lantern/sky contrast).
		S.bOverride_ColorContrast = true;
		S.bOverride_ColorGainShadows = true;
		S.bOverride_ColorSaturationShadows = true;
		S.bOverride_ColorGainHighlights = true;
		S.bOverride_ColorSaturationHighlights = true;
		S.bOverride_ColorCorrectionShadowsMax = true;
		S.ColorCorrectionShadowsMax = 0.09f;
		S.bOverride_ColorCorrectionHighlightsMin = true;
		S.ColorCorrectionHighlightsMin = 0.45f;
		S.bOverride_FilmGrainIntensity = true;
		S.bOverride_SceneFringeIntensity = true;
		S.SceneFringeIntensity = 0.f;
	}
	Apply(GetHour());
}

float UDBRealmSkyComponent::GetHour() const
{
	if (HourOverride >= 0.f)
	{
		return HourOverride;
	}
	const ADBGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADBGameState>() : nullptr;
	const UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr;
	return WorldState ? WorldState->GetTimeOfDay() : 18.33f;
}

void UDBRealmSkyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	NextUpdate -= DeltaTime;
	if (NextUpdate > 0.f)
	{
		return;
	}
	NextUpdate = 0.25f;
	const float Hour = GetHour();
	// A game minute moves the sun a quarter of a degree: updating more often only re-dirties light and fog state.
	if (FMath::Abs(Hour - AppliedHour) > 0.01f)
	{
		Apply(Hour);
	}
}

void UDBRealmSkyComponent::Apply(float Hour)
{
	AppliedHour = Hour;
	const FSkyKey Key = Evaluate(Hour);
	const float Dusk = Key.Dusk;

	// Fog base values by time: pale haze by day, violet-rose at dusk, deep blue at night. The region mood multiplies.
	const FLinearColor DayFog(0.45f, 0.5f, 0.6f);
	const FLinearColor DuskFog(0.3f, 0.25f, 0.38f);
	const FLinearColor NightFog(0.03f, 0.04f, 0.08f);
	const float Night = FMath::Clamp(-Key.SunElevation / 12.f, 0.f, 1.f);
	const FLinearColor FogColor = Mix(Mix(DayFog, DuskFog, Dusk), NightFog, Night * (1.f - Dusk));
	const float FogDensity = 0.012f * Key.Fog;

	if (UDirectionalLightComponent* Light = Sun.Get())
	{
		// Below the horizon the sun still colors the sky (atmosphere) while its direct light fades out.
		Light->SetWorldRotation(SunRotation(Hour, Key.SunElevation));
		Light->SetTemperature(Key.SunKelvin);
	}
	if (UDBRealmMoodComponent* MoodComponent = Mood.Get())
	{
		MoodComponent->SetSkyBase(FogDensity, FogColor, Key.SunLux, FLinearColor::White);
	}
	else
	{
		if (UDirectionalLightComponent* Light = Sun.Get())
		{
			Light->SetIntensity(Key.SunLux);
		}
		if (UExponentialHeightFogComponent* Component = Fog.Get())
		{
			Component->SetFogDensity(FogDensity);
			Component->SetFogInscatteringColor(FogColor);
		}
	}
	if (UExponentialHeightFogComponent* Component = Fog.Get())
	{
		// The valley mist and the sun glow through it peak at dusk.
		Component->SecondFogData.FogDensity = FMath::Lerp(0.02f, 0.07f, FMath::Max(Dusk, Night * 0.5f));
		// The atmosphere would light the distant fog almost white at the horizon: keep it a dim violet at dusk.
		Component->SetSkyAtmosphereAmbientContributionColorScale(Mix(FLinearColor::White, FLinearColor(0.45f, 0.36f, 0.5f), Dusk));
		Component->SetDirectionalInscatteringColor(Mix(FLinearColor(0.25f, 0.22f, 0.18f), FLinearColor(0.55f, 0.2f, 0.16f), Dusk) * (1.f - Night));
		Component->MarkRenderStateDirty();
	}
	if (USkyLightComponent* Sky = SkyLight.Get())
	{
		Sky->SetIntensity(Key.SkyLight);
	}
	if (USkyAtmosphereComponent* Sky = Atmosphere.Get())
	{
		// Dusk: the sky leans crimson-violet instead of plain orange, and the air between the ridges thickens
		// (aerial perspective) - near hills keep their colour, far ones fade into the sky in layers.
		Sky->SetSkyLuminanceFactor(Mix(FLinearColor::White, FLinearColor(1.05f, 0.78f, 1.15f), Dusk));
		Sky->SetAerialPespectiveViewDistanceScale(FMath::Lerp(1.f, 2.2f, Dusk));
	}
	if (Moon)
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Cast<ADirectionalLight>(Moon)->GetLightComponent()))
		{
			// Low over the western horizon at dusk, climbing through the night.
			const float Elevation = FMath::Lerp(11.f, 35.f, FMath::Clamp((Hour < 12.f ? Hour + 24.f : Hour) - 18.f, 0.f, 8.f) / 8.f);
			Light->SetWorldRotation(SunRotation(Hour, Elevation, -32.f));
			Light->SetIntensity(0.8f * Key.Moon);
			Light->SetAtmosphereSunDiskColorScale(FLinearColor(0.35f, 0.025f, 0.02f) * Key.Moon);
		}
	}
	if (PostProcess)
	{
		FPostProcessSettings& S = PostProcess->Settings;
		// Exposure: the dusk stays dark and moody (lower bias, capped auto exposure) instead of being brightened up.
		S.AutoExposureBias = FMath::Lerp(0.f, -0.3f, Dusk) - 0.3f * Night;
		S.AutoExposureMinBrightness = -2.f;
		S.AutoExposureMaxBrightness = 14.f;
		S.BloomIntensity = FMath::Lerp(0.35f, 0.75f, FMath::Max(Dusk, Night));
		S.ColorContrast = FVector4(1.f, 1.f, 1.f, FMath::Lerp(1.f, 1.1f, Dusk));
		S.ColorGainShadows = FVector4(FMath::Lerp(1.f, 0.9f, Dusk), FMath::Lerp(1.f, 0.93f, Dusk), FMath::Lerp(1.f, 1.14f, Dusk), 1.f);
		S.ColorSaturationShadows = FVector4(1.f, 1.f, 1.f, FMath::Lerp(1.f, 0.85f, Dusk));
		S.ColorGainHighlights = FVector4(FMath::Lerp(1.f, 1.05f, Dusk), FMath::Lerp(1.f, 0.99f, Dusk), FMath::Lerp(1.f, 0.94f, Dusk), 1.f);
		S.ColorSaturationHighlights = FVector4(1.f, 1.f, 1.f, FMath::Lerp(1.f, 1.15f, Dusk));
		S.FilmGrainIntensity = FMath::Lerp(0.f, 0.06f, Dusk);
	}
	UE_LOG(LogDarkBlood, Log, TEXT("DBSKY %.2f h: sun %.1f deg %.1f lux %.0f K, sky %.2f, fog %.4f, dusk %.2f, moon %.2f"), Hour, Key.SunElevation,
		Key.SunLux, Key.SunKelvin, Key.SkyLight, FogDensity, Dusk, Key.Moon);
}
