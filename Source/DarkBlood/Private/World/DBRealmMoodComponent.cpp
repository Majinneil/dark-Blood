#include "World/DBRealmMoodComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "DarkBlood.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "World/DBRealmLayout.h"

namespace
{
	using FMood = UDBRealmMoodComponent::FMood;

	/** Seconds for a mood change to mostly complete (exponential blend). */
	constexpr float BlendSeconds = 2.5f;

	FMood MakeMood(float Fog, const FLinearColor& FogColor, float Sun, const FLinearColor& SunColor, float Saturation, const FLinearColor& Gain,
		float Vignette = 0.4f)
	{
		FMood Mood;
		Mood.FogDensity = Fog;
		Mood.FogColor = FogColor;
		Mood.SunIntensity = Sun;
		Mood.SunColor = SunColor;
		Mood.Saturation = Saturation;
		Mood.Gain = Gain;
		Mood.Vignette = Vignette;
		return Mood;
	}

	/** Mood of each region, after the region sheet (docs/VisualPack/Reference/DarkBlood_Regionen.png). */
	FMood GetBiomeMood(EDBRealmBiome Biome)
	{
		const FLinearColor White = FLinearColor::White;
		switch (Biome)
		{
		case EDBRealmBiome::CherryValley:   return MakeMood(1.3f, FLinearColor(1.f, 0.82f, 0.9f), 1.f, FLinearColor(1.f, 0.95f, 0.95f), 1.1f, FLinearColor(1.02f, 0.98f, 1.f));
		case EDBRealmBiome::BambooForest:   return MakeMood(1.6f, FLinearColor(0.8f, 1.f, 0.82f), 0.9f, FLinearColor(0.95f, 1.f, 0.9f), 1.05f, FLinearColor(0.98f, 1.02f, 0.97f));
		case EDBRealmBiome::MistMountains:  return MakeMood(4.f, FLinearColor(0.85f, 0.9f, 1.f), 0.85f, FLinearColor(0.92f, 0.95f, 1.f), 0.85f, FLinearColor(0.97f, 0.99f, 1.03f));
		case EDBRealmBiome::Coast:          return MakeMood(0.8f, FLinearColor(0.85f, 0.95f, 1.f), 1.05f, White, 1.05f, White);
		case EDBRealmBiome::RiceFields:     return MakeMood(1.2f, FLinearColor(1.f, 0.95f, 0.78f), 1.05f, FLinearColor(1.f, 0.95f, 0.85f), 1.1f, FLinearColor(1.03f, 1.01f, 0.96f));
		case EDBRealmBiome::SpiritForest:   return MakeMood(5.f, FLinearColor(0.55f, 0.75f, 0.8f), 0.5f, FLinearColor(0.8f, 0.9f, 1.f), 0.7f, FLinearColor(0.93f, 1.f, 1.04f), 0.7f);
		case EDBRealmBiome::FireMountains:  return MakeMood(4.f, FLinearColor(1.f, 0.42f, 0.22f), 0.7f, FLinearColor(1.f, 0.68f, 0.48f), 1.15f, FLinearColor(1.08f, 0.96f, 0.9f), 0.6f);
		case EDBRealmBiome::IceWaste:       return MakeMood(2.2f, FLinearColor(0.8f, 0.9f, 1.f), 1.f, FLinearColor(0.85f, 0.92f, 1.f), 0.8f, FLinearColor(0.96f, 0.99f, 1.05f));
		case EDBRealmBiome::Desert:         return MakeMood(1.6f, FLinearColor(1.f, 0.84f, 0.62f), 1.15f, FLinearColor(1.f, 0.93f, 0.8f), 1.05f, FLinearColor(1.04f, 1.f, 0.94f));
		case EDBRealmBiome::SkyTemple:      return MakeMood(2.f, White, 1.1f, FLinearColor(1.f, 0.98f, 0.92f), 0.95f, FLinearColor(1.02f, 1.02f, 1.02f), 0.25f);
		case EDBRealmBiome::DemonWaste:     return MakeMood(4.f, FLinearColor(0.6f, 0.1f, 0.1f), 0.4f, FLinearColor(1.f, 0.5f, 0.45f), 0.8f, FLinearColor(1.05f, 0.9f, 0.9f), 0.75f);
		case EDBRealmBiome::VassalFortress: return MakeMood(2.5f, FLinearColor(0.55f, 0.6f, 0.7f), 0.6f, FLinearColor(0.9f, 0.92f, 1.f), 0.75f, FLinearColor(0.97f, 0.98f, 1.02f), 0.6f);
		case EDBRealmBiome::GreatCity:      return MakeMood(1.2f, FLinearColor(1.f, 0.95f, 0.88f), 1.f, White, 1.f, White);
		case EDBRealmBiome::Riverlands:     return MakeMood(1.8f, FLinearColor(0.8f, 0.95f, 1.f), 1.f, White, 1.05f, FLinearColor(0.98f, 1.01f, 1.02f));
		case EDBRealmBiome::TheEnd:         return MakeMood(6.f, FLinearColor(0.8f, 0.05f, 0.1f), 0.3f, FLinearColor(1.f, 0.4f, 0.4f), 0.9f, FLinearColor(1.1f, 0.88f, 0.88f), 0.85f);
		case EDBRealmBiome::Capital:
		default:                            return MakeMood(1.f, White, 1.f, White, 1.f, White);
		}
	}

	FMood Blend(const FMood& A, const FMood& B, float Alpha)
	{
		FMood Out;
		Out.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, Alpha);
		Out.FogColor = FMath::Lerp(A.FogColor, B.FogColor, Alpha);
		Out.SunIntensity = FMath::Lerp(A.SunIntensity, B.SunIntensity, Alpha);
		Out.SunColor = FMath::Lerp(A.SunColor, B.SunColor, Alpha);
		Out.Saturation = FMath::Lerp(A.Saturation, B.Saturation, Alpha);
		Out.Gain = FMath::Lerp(A.Gain, B.Gain, Alpha);
		Out.Vignette = FMath::Lerp(A.Vignette, B.Vignette, Alpha);
		return Out;
	}
}

UDBRealmMoodComponent::UDBRealmMoodComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDBRealmMoodComponent::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		Fog = It->GetComponent();
		BaseFogDensity = Fog->FogDensity;
		BaseFogColor = Fog->FogInscatteringLuminance;
		break;
	}
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent()); Light && Light->IsUsedAsAtmosphereSunLight())
		{
			Sun = Light;
			BaseSunIntensity = Light->Intensity;
			BaseSunColor = Light->GetLightColor();
			break;
		}
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	PostProcess = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
	if (PostProcess)
	{
		PostProcess->SetReplicates(false);
		PostProcess->bUnbound = true;
		// Below authored volumes (visual slice presets, cinematics).
		PostProcess->Priority = -1.f;
		FPostProcessSettings& S = PostProcess->Settings;
		S.bOverride_ColorSaturation = true;
		S.bOverride_ColorGain = true;
		S.bOverride_VignetteIntensity = true;
	}
}

void UDBRealmMoodComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PostProcess)
	{
		PostProcess->Destroy();
		PostProcess = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UDBRealmMoodComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RegionTimer -= DeltaTime;
	if (RegionTimer <= 0.f)
	{
		RegionTimer = 0.5f;
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector Location = Camera->GetCameraLocation() / 100.0;
			// Outside the landscape (dungeon interiors) the last region's mood stays.
			const int32 Region = DBRealm::IsInside(Location.X, Location.Y) ? DBRealm::FindRegionIndex(Location.X, Location.Y) : TargetRegion;
			const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
			if (Region != TargetRegion && Regions.IsValidIndex(Region))
			{
				const bool bFirst = TargetRegion == INDEX_NONE;
				TargetRegion = Region;
				SecondsSinceChange = 0.f;
				Target = GetBiomeMood(Regions[Region].Biome);
				if (bFirst)
				{
					Current = Target;
				}
				UE_LOG(LogDarkBlood, Log, TEXT("DBREALM mood: %s"), Regions[Region].DisplayName);
			}
		}
	}
	// Settled moods need no updates (each one dirties the fog and light render state).
	SecondsSinceChange += DeltaTime;
	if (TargetRegion != INDEX_NONE && SecondsSinceChange < BlendSeconds * 3.f)
	{
		Current = Blend(Current, Target, 1.f - FMath::Exp(-DeltaTime * 3.f / BlendSeconds));
		Apply(Current);
	}
}

void UDBRealmMoodComponent::Apply(const FMood& Mood)
{
	if (UExponentialHeightFogComponent* FogComponent = Fog.Get())
	{
		FogComponent->SetFogDensity(BaseFogDensity * Mood.FogDensity);
		FogComponent->SetFogInscatteringColor(BaseFogColor * Mood.FogColor);
	}
	if (UDirectionalLightComponent* Light = Sun.Get())
	{
		Light->SetIntensity(BaseSunIntensity * Mood.SunIntensity);
		Light->SetLightColor(BaseSunColor * Mood.SunColor);
	}
	if (PostProcess)
	{
		FPostProcessSettings& S = PostProcess->Settings;
		S.ColorSaturation = FVector4(Mood.Saturation, Mood.Saturation, Mood.Saturation, 1.0);
		S.ColorGain = FVector4(Mood.Gain.R, Mood.Gain.G, Mood.Gain.B, 1.0);
		S.VignetteIntensity = Mood.Vignette;
	}
}
