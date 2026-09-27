#include "Art/DBVisualSliceDirector.h"

#include "Art/DBModularBuilding.h"
#include "Art/DBSetDressing.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SplineComponent.h"
#include "DarkBlood.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Everything the slice spawns, in the slice's local frame (X = forward from the player start). */
	struct FSliceBuilder
	{
		UWorld& World;
		FTransform Origin;
		TArray<TObjectPtr<AActor>>& Out;

		FTransform At(const FVector& Local, float Yaw = 0.f) const
		{
			return FTransform(FRotator(0.f, Yaw, 0.f), Local) * Origin;
		}

		template <typename T>
		T* Begin(const FVector& Local, float Yaw = 0.f)
		{
			FActorSpawnParameters Params;
			Params.bDeferConstruction = true;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			T* Actor = World.SpawnActor<T>(T::StaticClass(), At(Local, Yaw), Params);
			if (Actor)
			{
				Actor->SetReplicates(false);
				Out.Add(Actor);
			}
			return Actor;
		}

		template <typename T>
		T* Finish(T* Actor, const FVector& Local, float Yaw = 0.f)
		{
			if (Actor)
			{
				Actor->FinishSpawning(At(Local, Yaw));
			}
			return Actor;
		}

		ADBModularBuilding* Building(const FVector& Local, float Yaw, EDBBuildingType Type, int32 Seed, FName Region, float Wealth, int32 X, int32 Y,
			int32 Floors = 1, float Damage = 0.f)
		{
			ADBModularBuilding* Actor = Begin<ADBModularBuilding>(Local, Yaw);
			if (Actor)
			{
				Actor->Configure(Type, Seed, Region, Wealth, X, Y, Floors, Damage);
			}
			return Finish(Actor, Local, Yaw);
		}

		ADBGroundPatch* Ground(const FVector& Local, const FVector2D& Size, EDBArtMaterial Material, float Layer, float TileSize = 0.f, bool bRound = false,
			float Yaw = 0.f)
		{
			ADBGroundPatch* Actor = Begin<ADBGroundPatch>(Local, Yaw);
			if (Actor)
			{
				Actor->Size = Size;
				Actor->Material = Material;
				Actor->Layer = Layer;
				Actor->TileSize = TileSize;
				Actor->bRound = bRound;
				Actor->Seed = FMath::RoundToInt(Local.X + Local.Y);
			}
			return Finish(Actor, Local, Yaw);
		}

		ADBSplineDressing* Spline(EDBSplineDressing Type, const TArray<FVector>& Points, float Width, int32 Seed)
		{
			ADBSplineDressing* Actor = Begin<ADBSplineDressing>(Points[0]);
			if (Actor)
			{
				Actor->Type = Type;
				Actor->Width = Width;
				Actor->Seed = Seed;
				TArray<FVector> Relative;
				for (const FVector& Point : Points)
				{
					Relative.Add(Point - Points[0]);
				}
				Actor->GetSpline()->SetSplinePoints(Relative, ESplineCoordinateSpace::Local, true);
			}
			return Finish(Actor, Points[0]);
		}

		ADBLantern* Lantern(const FVector& Local, EDBLanternStyle Style, float Yaw = 0.f)
		{
			ADBLantern* Actor = Begin<ADBLantern>(Local, Yaw);
			if (Actor)
			{
				Actor->Style = Style;
				Actor->StoneMaterial = Style == EDBLanternStyle::Stone ? EDBArtMaterial::StoneTemple : EDBArtMaterial::StoneMossy;
			}
			return Finish(Actor, Local, Yaw);
		}

		ADBScatterVolume* Scatter(const FVector& Local, const FVector& Extent, EDBBiome Biome, float Density, float Undergrowth, int32 Seed)
		{
			ADBScatterVolume* Actor = Begin<ADBScatterVolume>(Local);
			if (Actor)
			{
				Actor->Biome = Biome;
				Actor->Density = Density;
				Actor->UndergrowthDensity = Undergrowth;
				Actor->Seed = Seed;
				Actor->GetArea()->SetBoxExtent(Extent);
			}
			return Finish(Actor, Local);
		}
	};

	constexpr float SliceGroundProbe = 3000.f;
}

ADBVisualSliceDirector::ADBVisualSliceDirector()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.f);
}

ADBVisualSliceDirector* ADBVisualSliceDirector::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADBVisualSliceDirector> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

ADBVisualSliceDirector* ADBVisualSliceDirector::SpawnFor(UWorld* World, const FTransform& Origin)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	if (ADBVisualSliceDirector* Existing = Get(World))
	{
		return Existing;
	}
	// Anchor on the floor below the origin (the player start / pawn center floats ~90 cm above it).
	FVector Ground = Origin.GetLocation() - FVector(0.f, 0.f, 90.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DBSliceGround), true);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	// Static and dynamic world geometry (the template floor is WorldDynamic), never pawns.
	FCollisionObjectQueryParams GroundTypes(ECC_WorldStatic);
	GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	const FVector Top = Origin.GetLocation() + FVector(0.f, 0.f, 50.f);
	if (World->LineTraceSingleByObjectType(Hit, Top, Top - FVector(0.f, 0.f, SliceGroundProbe), GroundTypes, Params))
	{
		Ground = Hit.ImpactPoint;
	}
	// Same rounding as the replicated FVector_NetQuantize, so server and clients build at identical heights.
	Ground = FVector(FMath::RoundHalfFromZero(Ground.X), FMath::RoundHalfFromZero(Ground.Y), FMath::RoundHalfFromZero(Ground.Z));
	FActorSpawnParameters SpawnParams;
	SpawnParams.bDeferConstruction = true;
	const FTransform DirectorTransform(FRotator(0.f, Origin.Rotator().Yaw, 0.f), Ground);
	ADBVisualSliceDirector* Director = World->SpawnActor<ADBVisualSliceDirector>(ADBVisualSliceDirector::StaticClass(), DirectorTransform, SpawnParams);
	if (Director)
	{
		Director->State.Origin = Ground;
		Director->State.Yaw = Origin.Rotator().Yaw;
		Director->State.bEnabled = !FParse::Param(FCommandLine::Get(), TEXT("DBNoVisualSlice"));
		FString TimeOfDay;
		if (FParse::Value(FCommandLine::Get(), TEXT("DBTimeOfDay="), TimeOfDay))
		{
			const int64 Value = StaticEnum<EDBTimeOfDay>()->GetValueByNameString(TimeOfDay);
			Director->State.TimeOfDay = Value == INDEX_NONE ? EDBTimeOfDay::Dusk : static_cast<EDBTimeOfDay>(Value);
		}
		Director->FinishSpawning(DirectorTransform);
	}
	return Director;
}

void ADBVisualSliceDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBVisualSliceDirector, State);
}

void ADBVisualSliceDirector::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		ApplyLocal();
	}
}

void ADBVisualSliceDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearDressing();
	Super::EndPlay(EndPlayReason);
}

void ADBVisualSliceDirector::OnRep_State()
{
	ApplyLocal();
}

void ADBVisualSliceDirector::SetSliceEnabled(bool bEnabled)
{
	if (HasAuthority() && State.bEnabled != bEnabled)
	{
		State.bEnabled = bEnabled;
		ApplyLocal();
	}
}

void ADBVisualSliceDirector::SetTimeOfDay(EDBTimeOfDay TimeOfDay)
{
	if (HasAuthority())
	{
		State.TimeOfDay = TimeOfDay;
		ApplyLocal();
	}
}

void ADBVisualSliceDirector::ApplyLocal()
{
	if (State.bEnabled && !bBuilt)
	{
		BuildDressing();
	}
	else if (!State.bEnabled && bBuilt)
	{
		ClearDressing();
	}
	ApplyLighting();
}

void ADBVisualSliceDirector::ClearDressing()
{
	for (AActor* Actor : LocalActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	LocalActors.Reset();
	if (IsValid(PostProcess))
	{
		PostProcess->Destroy();
	}
	PostProcess = nullptr;
	bBuilt = false;
}

void ADBVisualSliceDirector::BuildDressing()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		// A dedicated server keeps only the collision-relevant pieces in a later pass; for now it renders nothing.
		return;
	}
	const double StartSeconds = FPlatformTime::Seconds();
	FSliceBuilder B{*World, FTransform(FRotator(0.f, State.Yaw, 0.f), State.Origin), LocalActors};
	using M = EDBArtMaterial;

	// ---- Ground layers (visual only, the blockout floor keeps the collision) ------------------------
	// The base layer also carries collision where the blockout floor ends (same height: gameplay unchanged).
	if (ADBGroundPatch* Base = B.Begin<ADBGroundPatch>(FVector(3500.f, 0.f, 0.f)))
	{
		Base->Size = FVector2D(26000.f, 16000.f);
		Base->Material = M::GroundForest;
		Base->Layer = 0.6f;
		Base->bCollision = true;
		B.Finish(Base, FVector(3500.f, 0.f, 0.f));
	}
	B.Ground(FVector(800.f, 150.f, 0.f), FVector2D(3600.f, 3900.f), M::GroundCourtyard, 1.6f, 125.f);
	B.Ground(FVector(3900.f, -2250.f, 0.f), FVector2D(3600.f, 2400.f), M::GroundEarth, 1.1f);
	B.Ground(FVector(8100.f, -700.f, 0.f), FVector2D(1900.f, 1700.f), M::StoneTemple, 1.4f, 0.f, true);

	// ---- 1. Capital: castle courtyard, palace hall, smithy, guardhouse, walls, gate --------------------
	// The palace stands well behind the king: the open courtyard in front of the start stays a combat space.
	B.Building(FVector(1950.f, 0.f, 0.f), 180.f, EDBBuildingType::TempleHall, 11, TEXT("Capital"), 1.f, 4, 4);
	B.Building(FVector(300.f, -1250.f, 0.f), 90.f, EDBBuildingType::Smithy, 23, TEXT("Capital"), 0.5f, 2, 3);
	B.Building(FVector(350.f, 1650.f, 0.f), -90.f, EDBBuildingType::Guardhouse, 31, TEXT("Capital"), 0.6f, 2, 3);
	B.Spline(EDBSplineDressing::CastleWall, {FVector(-1000.f, 2100.f, 0.f), FVector(2600.f, 2100.f, 0.f)}, 60.f, 1);
	B.Spline(EDBSplineDressing::CastleWall, {FVector(-1000.f, -1800.f, 0.f), FVector(-1000.f, 2100.f, 0.f)}, 60.f, 2);
	B.Spline(EDBSplineDressing::CastleWall, {FVector(-1000.f, -1800.f, 0.f), FVector(2600.f, -1800.f, 0.f)}, 60.f, 3);
	B.Spline(EDBSplineDressing::CastleWall, {FVector(2600.f, -1800.f, 0.f), FVector(2600.f, -960.f, 0.f)}, 60.f, 4);
	B.Spline(EDBSplineDressing::CastleWall, {FVector(2600.f, -440.f, 0.f), FVector(2600.f, 2100.f, 0.f)}, 60.f, 5);
	if (ADBGate* Gate = B.Begin<ADBGate>(FVector(2600.f, -700.f, 0.f)))
	{
		Gate->Style = EDBGateStyle::RoofedGate;
		Gate->Width = 440.f;
		Gate->Height = 430.f;
		B.Finish(Gate, FVector(2600.f, -700.f, 0.f));
	}
	for (const float Y : {-330.f, 330.f})
	{
		B.Lantern(FVector(1060.f, Y, 0.f), EDBLanternStyle::Stone);
	}
	B.Lantern(FVector(2450.f, -1030.f, 0.f), EDBLanternStyle::WoodPost);
	B.Lantern(FVector(2450.f, -370.f, 0.f), EDBLanternStyle::WoodPost);
	B.Spline(EDBSplineDressing::WoodFence, {FVector(100.f, 1180.f, 0.f), FVector(720.f, 1180.f, 0.f), FVector(720.f, 430.f, 0.f)}, 0.f, 6);
	B.Scatter(FVector(-620.f, 1650.f, 0.f), FVector(260.f, 260.f, 400.f), EDBBiome::CherryGrove, 9.f, 6.f, 7);

	// ---- 2. Road out of the capital (the first demon encounter happens on it) ---------------------------
	B.Spline(EDBSplineDressing::Road, {FVector(2600.f, -700.f, 0.f), FVector(3500.f, -640.f, 0.f), FVector(5000.f, -780.f, 0.f), FVector(5980.f, -700.f, 0.f)}, 440.f, 8);
	B.Spline(EDBSplineDressing::Road, {FVector(6620.f, -700.f, 0.f), FVector(7050.f, -700.f, 0.f)}, 440.f, 9);
	for (const float X : {3200.f, 4000.f, 4800.f, 5600.f})
	{
		B.Lantern(FVector(X, -1010.f, 0.f), EDBLanternStyle::WoodPost);
	}

	// ---- 3. Village south of the road: tavern, merchant, houses, storehouse, fence ------------------------
	if (ADBModularBuilding* Tavern = B.Begin<ADBModularBuilding>(FVector(3900.f, -1780.f, 0.f), 90.f))
	{
		Tavern->Configure(EDBBuildingType::Tavern, 41, TEXT("Village"), 0.55f, 3, 4, 2);
		B.Finish(Tavern, FVector(3900.f, -1780.f, 0.f), 90.f);
	}
	B.Building(FVector(2950.f, -1750.f, 0.f), 90.f, EDBBuildingType::MerchantHouse, 43, TEXT("Village"), 0.5f, 2, 2, 2);
	B.Building(FVector(4950.f, -1650.f, 0.f), 90.f, EDBBuildingType::SmallHouse, 47, TEXT("Village"), 0.3f, 2, 3);
	B.Building(FVector(5050.f, -2800.f, 0.f), 180.f, EDBBuildingType::SmallHouse, 53, TEXT("Village"), 0.35f, 2, 2);
	B.Building(FVector(3450.f, -2950.f, 0.f), 0.f, EDBBuildingType::Warehouse, 59, TEXT("Village"), 0.5f, 2, 2);
	B.Building(FVector(4300.f, -3050.f, 0.f), 90.f, EDBBuildingType::SmallHouse, 61, TEXT("Village"), 0.25f, 2, 2, 1, 0.35f);
	B.Spline(EDBSplineDressing::WoodFence, {FVector(2800.f, -3550.f, 0.f), FVector(4000.f, -3650.f, 0.f), FVector(5700.f, -3500.f, 0.f)}, 0.f, 10);
	B.Lantern(FVector(4300.f, -2350.f, 0.f), EDBLanternStyle::WoodPost);

	// ---- 4. Forest north of the road, 6. dungeon entrance at its far end ----------------------------------
	B.Spline(EDBSplineDressing::StonePath, {FVector(5000.f, -470.f, 0.f), FVector(5150.f, 900.f, 0.f), FVector(5050.f, 2100.f, 0.f), FVector(5200.f, 2650.f, 0.f)},
		150.f, 11);
	B.Scatter(FVector(4300.f, 1450.f, 0.f), FVector(1500.f, 1050.f, 500.f), EDBBiome::TemperateForest, 5.f, 14.f, 12);
	if (ADBDungeonEntrance* Dungeon = B.Begin<ADBDungeonEntrance>(FVector(5250.f, 3150.f, 0.f), -90.f))
	{
		Dungeon->Corruption = 0.8f;
		Dungeon->Seed = 13;
		B.Finish(Dungeon, FVector(5250.f, 3150.f, 0.f), -90.f);
	}
	B.Scatter(FVector(5250.f, 3500.f, 0.f), FVector(1000.f, 450.f, 500.f), EDBBiome::Corrupted, 2.5f, 6.f, 14);

	// ---- 5. Stream with a lacquered bridge where the road crosses ------------------------------------------
	B.Spline(EDBSplineDressing::Stream,
		{FVector(6350.f, 3600.f, 0.f), FVector(6150.f, 1500.f, 0.f), FVector(6300.f, -700.f, 0.f), FVector(6050.f, -2500.f, 0.f), FVector(6250.f, -3900.f, 0.f)}, 260.f, 15);
	if (ADBBridge* Bridge = B.Begin<ADBBridge>(FVector(6300.f, -700.f, 0.f)))
	{
		Bridge->Length = 660.f;
		Bridge->Width = 380.f;
		B.Finish(Bridge, FVector(6300.f, -700.f, 0.f));
	}
	B.Scatter(FVector(6700.f, 1800.f, 0.f), FVector(500.f, 1300.f, 500.f), EDBBiome::WetForest, 4.f, 16.f, 16);

	// ---- 7. Shrine: torii, stone path, lanterns, cherry trees, bamboo grove ---------------------------------
	if (ADBGate* Torii = B.Begin<ADBGate>(FVector(7100.f, -700.f, 0.f)))
	{
		Torii->Style = EDBGateStyle::Torii;
		B.Finish(Torii, FVector(7100.f, -700.f, 0.f));
	}
	B.Spline(EDBSplineDressing::StonePath, {FVector(7200.f, -700.f, 0.f), FVector(7850.f, -700.f, 0.f)}, 170.f, 17);
	for (const float X : {7450.f, 7750.f})
	{
		B.Lantern(FVector(X, -1020.f, 0.f), EDBLanternStyle::Stone);
		B.Lantern(FVector(X, -380.f, 0.f), EDBLanternStyle::Stone);
	}
	B.Building(FVector(8350.f, -700.f, 0.f), 180.f, EDBBuildingType::Shrine, 71, TEXT("Temple"), 0.8f, 2, 2);
	B.Scatter(FVector(8100.f, -700.f, 0.f), FVector(1100.f, 1100.f, 500.f), EDBBiome::ShrineGarden, 2.5f, 5.f, 18);
	B.Scatter(FVector(8400.f, 1100.f, 0.f), FVector(800.f, 450.f, 500.f), EDBBiome::Bamboo, 7.f, 3.f, 19);
	B.Scatter(FVector(8500.f, -2400.f, 0.f), FVector(900.f, 700.f, 500.f), EDBBiome::MountainForest, 3.f, 8.f, 20);

	// Point lights of lanterns/buildings only switch on at dusk and night (see ApplyLighting).
	bBuilt = true;
	int32 Instances = 0;
	for (const AActor* Actor : LocalActors)
	{
		if (const ADBModularBuilding* Building = Cast<ADBModularBuilding>(Actor))
		{
			Instances += Building->GetInstanceCount();
		}
		else if (const ADBArtActor* Art = Cast<ADBArtActor>(Actor))
		{
			Instances += Art->GetInstanceCount();
		}
	}
	UE_LOG(LogDarkBlood, Log, TEXT("DBVIS visual slice built at %s (yaw %.0f): %d actors, %d instances in %.0f ms"), *FVector(State.Origin).ToString(), State.Yaw,
		LocalActors.Num(), Instances, (FPlatformTime::Seconds() - StartSeconds) * 1000.0);
}

void ADBVisualSliceDirector::ApplyLighting()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	struct FPreset
	{
		float SunPitch, SunYaw, SunLux;
		FLinearColor SunColor;
		float SkyIntensity;
		float FogDensity;
		FLinearColor FogColor;
		float VolumetricExtinction;
		float ExposureBias, Saturation, Contrast;
		FLinearColor Gain;
		bool bLanterns;
		float MinBrightness, MaxBrightness;
	};
	static const FPreset Presets[] = {
		/* Day   */ {-48.f, -35.f, 9.f, FLinearColor(1.f, 0.95f, 0.88f), 1.f, 0.012f, FLinearColor(0.45f, 0.55f, 0.7f), 0.6f, 0.f, 1.f, 1.03f, FLinearColor::White, false, 0.1f, 4.f},
		/* Dusk  */ {-9.f, 150.f, 6.f, FLinearColor(1.f, 0.58f, 0.34f), 1.2f, 0.02f, FLinearColor(0.85f, 0.5f, 0.32f), 1.2f, 0.3f, 1.05f, 1.08f, FLinearColor(1.f, 0.97f, 0.93f), true, 0.08f, 2.f},
		/* Night */ {-40.f, 60.f, 0.35f, FLinearColor(0.55f, 0.65f, 1.f), 0.3f, 0.02f, FLinearColor(0.07f, 0.09f, 0.16f), 1.f, -0.3f, 0.85f, 1.05f, FLinearColor(0.9f, 0.95f, 1.08f), true, 0.6f, 1.2f},
		/* Demon */ {-12.f, 150.f, 1.4f, FLinearColor(0.95f, 0.25f, 0.17f), 0.35f, 0.03f, FLinearColor(0.2f, 0.03f, 0.03f), 1.6f, 0.f, 0.7f, 1.15f, FLinearColor(1.05f, 0.9f, 0.9f), true, 0.4f, 1.5f},
	};
	const FPreset& Preset = Presets[FMath::Clamp(static_cast<int32>(State.TimeOfDay), 0, 3)];
	const bool bActive = State.bEnabled;

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (!Sun || !bActive)
		{
			continue;
		}
		Sun->SetMobility(EComponentMobility::Movable);
		Sun->SetWorldRotation(FRotator(Preset.SunPitch, State.Yaw + Preset.SunYaw, 0.f));
		Sun->SetIntensity(Preset.SunLux);
		Sun->SetLightColor(Preset.SunColor);
		break;
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (USkyLightComponent* Sky = It->GetLightComponent(); Sky && bActive)
		{
			Sky->SetIntensity(Preset.SkyIntensity);
			Sky->RecaptureSky();
		}
		break;
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (UExponentialHeightFogComponent* Fog = It->GetComponent(); Fog && bActive)
		{
			Fog->SetFogDensity(Preset.FogDensity);
			Fog->SetFogInscatteringColor(Preset.FogColor);
			Fog->SetVolumetricFog(true);
			Fog->SetVolumetricFogExtinctionScale(Preset.VolumetricExtinction);
			Fog->SetVolumetricFogScatteringDistribution(0.6f);
		}
		break;
	}

	if (bActive && !IsValid(PostProcess))
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		PostProcess = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
		if (PostProcess)
		{
			PostProcess->SetReplicates(false);
			PostProcess->bUnbound = true;
			PostProcess->Priority = 1.f;
		}
	}
	if (IsValid(PostProcess))
	{
		FPostProcessSettings& S = PostProcess->Settings;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = Preset.ExposureBias;
		S.bOverride_AutoExposureMinBrightness = true;
		S.AutoExposureMinBrightness = Preset.MinBrightness; // night stays night: exposure cannot adapt all the way up
		S.bOverride_AutoExposureMaxBrightness = true;
		S.AutoExposureMaxBrightness = Preset.MaxBrightness;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = 0.45f; // restrained: lanterns glow, no bloom orgy
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = 0.35f;
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(Preset.Saturation, Preset.Saturation, Preset.Saturation, 1.f);
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(Preset.Contrast, Preset.Contrast, Preset.Contrast, 1.f);
		S.bOverride_ColorGain = true;
		S.ColorGain = FVector4(Preset.Gain.R, Preset.Gain.G, Preset.Gain.B, 1.f);
		PostProcess->bEnabled = bActive;
	}

	// Lantern and interior lights only burn at dusk / night (saves light cost at day).
	for (AActor* Actor : LocalActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		TInlineComponentArray<UPointLightComponent*> Lamps(Actor);
		for (UPointLightComponent* Lamp : Lamps)
		{
			Lamp->SetVisibility(Preset.bLanterns);
		}
	}
	UE_LOG(LogDarkBlood, Log, TEXT("DBVIS lighting: %s (slice %s)"), *StaticEnum<EDBTimeOfDay>()->GetNameStringByValue(static_cast<int64>(State.TimeOfDay)),
		bActive ? TEXT("on") : TEXT("off"));
}

FString ADBVisualSliceDirector::DescribeLocalSlice() const
{
	int32 Components = 0;
	int32 Instances = 0;
	int32 Lamps = 0;
	for (const AActor* Actor : LocalActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		TInlineComponentArray<UInstancedStaticMeshComponent*> Batches(Actor);
		for (const UInstancedStaticMeshComponent* Batch : Batches)
		{
			++Components;
			Instances += Batch->GetInstanceCount();
		}
		TInlineComponentArray<UPointLightComponent*> Lights(Actor);
		Lamps += Lights.Num();
	}
	return FString::Printf(TEXT("slice %s, %s, %d actors, %d instanced components, %d instances, %d point lights"), State.bEnabled ? TEXT("on") : TEXT("off"),
		*StaticEnum<EDBTimeOfDay>()->GetNameStringByValue(static_cast<int64>(State.TimeOfDay)), LocalActors.Num(), Components, Instances, Lamps);
}
