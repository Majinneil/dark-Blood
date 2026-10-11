#include "World/DBWaterfall.h"

#include "Art/DBArtMaterials.h"
#include "Components/StaticMeshComponent.h"
#include "DarkBlood.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Audio/DBAudioSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Visual/DBCombatFeedback.h"
#include "World/DBRealmLayout.h"

namespace
{
	/** Sampling of the search (m): a coarse grid, then a walk down the steepest direction. */
	constexpr double GridStep = 150.0;
	constexpr double MinDrop = 32.0;
	/** Water only falls from real cliffs (56 degrees and more); on gentler faces it would read as a slide. */
	constexpr double MinSteepness = 1.5;
	constexpr double MinSpacing = 1100.0;
	constexpr int32 MaxFalls = 16;
	constexpr int32 MaxPerRegion = 3;

	bool CarriesWater(EDBRealmBiome Biome)
	{
		switch (Biome)
		{
		case EDBRealmBiome::Capital:
		case EDBRealmBiome::CherryValley:
		case EDBRealmBiome::BambooForest:
		case EDBRealmBiome::MistMountains:
		case EDBRealmBiome::Coast:
		case EDBRealmBiome::VassalFortress:
		case EDBRealmBiome::DemonWaste:
		case EDBRealmBiome::TheEnd:
			return true;
		default:
			return false;
		}
	}

	/** The demon lands bleed: their falls run with dark blood. */
	bool IsBloodFall(EDBRealmBiome Biome)
	{
		return Biome == EDBRealmBiome::DemonWaste || Biome == EDBRealmBiome::TheEnd;
	}

	bool ClearOfSettlements(const FVector2D& At)
	{
		for (const FDBRealmSettlement& Settlement : DBRealm::GetSettlements())
		{
			if (FVector2D::Distance(At, Settlement.Center) < Settlement.Radius + 120.0)
			{
				return false;
			}
		}
		return true;
	}

	struct FCandidate
	{
		FVector2D Top;
		FVector2D Bottom;
		double TopHeight = 0.0;
		double BottomHeight = 0.0;
		double Score = 0.0;
		int32 Region = 0;
	};

	UStaticMesh* BasicShape(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}
}

const TArray<FDBWaterfallSite>& DBWaterfalls::GetSites()
{
	static TArray<FDBWaterfallSite> Sites;
	static bool bFound = false;
	if (bFound)
	{
		return Sites;
	}
	bFound = true;
	const double StartSeconds = FPlatformTime::Seconds();
	const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
	TArray<FCandidate> Candidates;
	const double Limit = DBRealm::HalfSize - 300.0;
	for (double Y = -Limit; Y <= Limit; Y += GridStep)
	{
		for (double X = -Limit; X <= Limit; X += GridStep)
		{
			const int32 Region = DBRealm::FindRegionIndex(X, Y);
			if (!CarriesWater(Regions[Region].Biome))
			{
				continue;
			}
			const double Height = DBRealm::SampleHeight(X, Y);
			if (Height < 15.0)
			{
				continue;
			}
			const FVector2D Gradient((DBRealm::SampleHeight(X + 12.0, Y) - DBRealm::SampleHeight(X - 12.0, Y)) / 24.0,
				(DBRealm::SampleHeight(X, Y + 12.0) - DBRealm::SampleHeight(X, Y - 12.0)) / 24.0);
			if (Gradient.Size() < 0.75)
			{
				continue;
			}
			// Down the steepest way: the fall ends where the face flattens out.
			const FVector2D Down = -Gradient.GetSafeNormal();
			FCandidate Best;
			for (double Distance = 10.0; Distance <= 130.0; Distance += 10.0)
			{
				const FVector2D Foot = FVector2D(X, Y) + Down * Distance;
				const double FootHeight = FMath::Max(DBRealm::SampleHeight(Foot.X, Foot.Y), 0.0);
				const double Drop = Height - FootHeight;
				if (Drop / Distance < MinSteepness * 0.75)
				{
					break;
				}
				if (Drop >= MinDrop && Drop / Distance >= MinSteepness)
				{
					Best.Top = FVector2D(X, Y);
					Best.Bottom = Foot;
					Best.TopHeight = Height;
					Best.BottomHeight = FootHeight;
					Best.Score = Drop * FMath::Min(Drop / Distance, 3.0);
					Best.Region = Region;
				}
			}
			// Only where the water can land: the sea or level ground (on a slope a fall has nowhere to go).
		const bool bLands = Best.Score > 0.0 && (Best.BottomHeight <= 0.5 || FVector2D((DBRealm::SampleHeight(Best.Bottom.X + 15.0, Best.Bottom.Y)
			- DBRealm::SampleHeight(Best.Bottom.X - 15.0, Best.Bottom.Y)) / 30.0, (DBRealm::SampleHeight(Best.Bottom.X, Best.Bottom.Y + 15.0)
			- DBRealm::SampleHeight(Best.Bottom.X, Best.Bottom.Y - 15.0)) / 30.0).Size() < 0.3);
		if (bLands && ClearOfSettlements(Best.Top) && ClearOfSettlements(Best.Bottom))
			{
				Candidates.Add(Best);
			}
		}
	}
	// The grandest falls first, spread over the realm, a few per region.
	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score > B.Score; });
	TMap<int32, int32> PerRegion;
	for (const FCandidate& Candidate : Candidates)
	{
		if (Sites.Num() >= MaxFalls)
		{
			break;
		}
		int32& Count = PerRegion.FindOrAdd(Candidate.Region);
		if (Count >= MaxPerRegion)
		{
			continue;
		}
		const bool bCrowded = Sites.ContainsByPredicate([&Candidate](const FDBWaterfallSite& Site)
		{
			return FVector2D::Distance(FVector2D(Site.Top) / 100.0, Candidate.Top) < MinSpacing;
		});
		if (bCrowded)
		{
			continue;
		}
		++Count;
		FDBWaterfallSite& Site = Sites.AddDefaulted_GetRef();
		Site.Top = FVector(Candidate.Top.X, Candidate.Top.Y, Candidate.TopHeight) * 100.0;
		Site.Bottom = FVector(Candidate.Bottom.X, Candidate.Bottom.Y, Candidate.BottomHeight) * 100.0;
		const double Drop = Candidate.TopHeight - Candidate.BottomHeight;
		Site.Width = static_cast<float>(FMath::Clamp(Drop * 0.22, 8.0, 22.0) * 100.0);
		Site.RegionId = Regions[Candidate.Region].RegionId;
		// Stand off the face as far as it bulges above the straight line from lip to foot.
		double Bulge = 0.0;
		for (int32 Step = 1; Step < 8; ++Step)
		{
			const double T = Step / 8.0;
			const FVector2D At = FMath::Lerp(Candidate.Top, Candidate.Bottom, T);
			const double Line = FMath::Lerp(Candidate.TopHeight, Candidate.BottomHeight, T);
			Bulge = FMath::Max(Bulge, DBRealm::SampleHeight(At.X, At.Y) - Line);
		}
		Site.Lift = static_cast<float>(150.0 + FMath::Max(0.0, Bulge) * 100.0);
		const FVector2D& Foot = Candidate.Bottom;
		const double FootSlope = FVector2D((DBRealm::SampleHeight(Foot.X + 15.0, Foot.Y) - DBRealm::SampleHeight(Foot.X - 15.0, Foot.Y)) / 30.0,
			(DBRealm::SampleHeight(Foot.X, Foot.Y + 15.0) - DBRealm::SampleHeight(Foot.X, Foot.Y - 15.0)) / 30.0).Size();
		Site.bLevelFoot = Candidate.BottomHeight <= 0.5 || FootSlope < 0.3;
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Waterfalls: %d sites from %d candidates (%.2f s)"), Sites.Num(), Candidates.Num(), FPlatformTime::Seconds() - StartSeconds);
	for (const FDBWaterfallSite& Site : Sites)
	{
		UE_LOG(LogDarkBlood, Log, TEXT("  waterfall %s at (%.0f, %.0f) m: drop %.0f m, width %.0f m, lift %.1f m"), *Site.RegionId.ToString(), Site.Top.X / 100.0,
			Site.Top.Y / 100.0, (Site.Top.Z - Site.Bottom.Z) / 100.0, Site.Width / 100.0, Site.Lift / 100.0);
	}
	return Sites;
}

void DBWaterfalls::SpawnAll(UWorld* World)
{
	if (!World || TActorIterator<ADBWaterfall>(World))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FDBWaterfallSite& Site : GetSites())
	{
		if (ADBWaterfall* Fall = World->SpawnActor<ADBWaterfall>(ADBWaterfall::StaticClass(), Site.Top, FRotator::ZeroRotator, Params))
		{
			Fall->Build(Site);
		}
	}
}

ADBWaterfall::ADBWaterfall()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.3f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

UStaticMeshComponent* ADBWaterfall::AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& Transform)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	Part->SetMobility(EComponentMobility::Static);
	Part->SetupAttachment(Root);
	Part->SetStaticMesh(Mesh);
	Part->SetWorldTransform(Transform);
	Part->SetMaterial(0, Material);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCastShadow(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCachedMaxDrawDistance(160000.f);
	Part->RegisterComponent();
	Parts.Add(Part);
	return Part;
}

void ADBWaterfall::Build(const FDBWaterfallSite& Site)
{
	UStaticMesh* Plane = BasicShape(TEXT("Plane"));
	UStaticMesh* Cylinder = BasicShape(TEXT("Cylinder"));
	UMaterialInterface* Falling = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Master/M_DB_Waterfall.M_DB_Waterfall"), nullptr,
		LOAD_NoWarn | LOAD_Quiet);
	if (!Plane || !Cylinder || !Falling)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("Waterfall: meshes or M_DB_Waterfall missing"));
		return;
	}
	const FDBRealmRegion& Region = DBRealm::GetRegions()[DBRealm::FindRegionIndex(Site.Top.X / 100.0, Site.Top.Y / 100.0)];
	const bool bBlood = IsBloodFall(Region.Biome);
	const FLinearColor WaterColor = bBlood ? FLinearColor(0.22f, 0.01f, 0.015f) : FLinearColor(0.12f, 0.19f, 0.21f);
	const FLinearColor FoamColor = bBlood ? FLinearColor(0.75f, 0.12f, 0.08f) : FLinearColor(0.86f, 0.9f, 0.92f);
	auto MakeWater = [this, Falling, &WaterColor, &FoamColor](float Flow, float Streaks, float Opacity, float Foam, float DownTiles = 1.f)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Falling, this);
		Material->SetVectorParameterValue(TEXT("WaterColor"), WaterColor);
		Material->SetVectorParameterValue(TEXT("FoamColor"), FoamColor);
		Material->SetScalarParameterValue(TEXT("FlowSpeed"), Flow);
		Material->SetScalarParameterValue(TEXT("Streaks"), Streaks);
		Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		Material->SetScalarParameterValue(TEXT("FoamAmount"), Foam);
		Material->SetScalarParameterValue(TEXT("DownTiles"), DownTiles);
		Materials.Add(Material);
		return Material;
	};

	// Frame of the sheet: X down the face, Y across it, Z (its normal) out of the face.
	const FVector Down = (Site.Bottom - Site.Top).GetSafeNormal();
	const FVector DownFlat = FVector(Down.X, Down.Y, 0.f).GetSafeNormal();
	FVector Across = FVector::CrossProduct(FVector::UpVector, DownFlat).GetSafeNormal();
	if (FVector::DotProduct(FVector::CrossProduct(Down, Across), DownFlat) < 0.f)
	{
		Across = -Across;
	}
	const FVector Out = FVector::CrossProduct(Down, Across).GetSafeNormal();
	const FRotator Facing = FRotationMatrix::MakeFromXY(Down, Across).Rotator();
	const float Length = FVector::Dist(Site.Top, Site.Bottom) + 400.f;
	const FVector Middle = (Site.Top + Site.Bottom) * 0.5f;

	// Two sheets: the body of the fall and a thinner, faster veil in front of it.
	// Texture tiles stay square: as many down the sheet as its length holds.
	AddPart(Plane, MakeWater(1.f, Site.Width / 450.f, 0.42f, 1.f, Length / 450.f), FTransform(Facing, Middle + Out * Site.Lift, FVector(Length / 100.f, Site.Width / 100.f, 1.f)));
	AddPart(Plane, MakeWater(1.45f, Site.Width / 300.f, 0.1f, 1.1f, Length / 300.f),
		FTransform(Facing, Middle + Out * (Site.Lift + 90.f), FVector(Length / 100.f, Site.Width * 0.72f / 100.f, 1.f)));

	// The spring on the lip, a little back from the edge.
	const FVector Spring = Site.Top - DownFlat * Site.Width * 0.45f + FVector(0.f, 0.f, -40.f);
	AddPart(Cylinder, UDBArtMaterialSubsystem::Get(bBlood ? EDBArtMaterial::BloodRiver : EDBArtMaterial::Water),
		FTransform(FRotator::ZeroRotator, Spring, FVector(Site.Width * 1.3f / 100.f, Site.Width * 1.3f / 100.f, 0.4f)));

	// Where it lands: a plunge pool (above the sea only), churning foam and a cloud of spray.
	const FVector Landing = Site.Bottom + Out * Site.Lift * 0.4f;
	if (Site.Bottom.Z > 50.f && !bBlood && Site.bLevelFoot)
	{
		AddPart(Cylinder, UDBArtMaterialSubsystem::Get(bBlood ? EDBArtMaterial::BloodRiver : EDBArtMaterial::Water),
			FTransform(FRotator::ZeroRotator, Landing + FVector(0.f, 0.f, 20.f), FVector(Site.Width * 2.4f / 100.f, Site.Width * 2.4f / 100.f, 0.3f)));
	}
	UMaterialInstanceDynamic* Churn = MakeWater(0.35f, 4.f, 0.15f, 1.3f, 4.f);
	Churn->SetScalarParameterValue(TEXT("EdgeSoftness"), 0.f);
	if (Site.bLevelFoot)
	{
		AddPart(Cylinder, Churn,
			FTransform(FRotator::ZeroRotator, FVector(Landing.X, Landing.Y, FMath::Max(Landing.Z, 0.f) + 45.f), FVector(Site.Width * 1.5f / 100.f, Site.Width * 1.5f / 100.f, 0.2f)));
	}
	Foot = FVector(Landing.X, Landing.Y, FMath::Max(Landing.Z, 0.f) + 60.f);
	// The roar: heard from far, loudest at the foot (an audio component only on machines with audio).
	if (UDBAudioSubsystem* Audio = UDBAudioSubsystem::Get(this))
	{
		Audio->PlayLoopAttached(TEXT("Ambience/Waterfall"), Root, (Foot + Site.Top) * 0.5f, Site.Width * 0.8f + (Site.Top.Z - Site.Bottom.Z) * 0.4f, 16000.f);
	}
	FootRadius = Site.Width * 0.45f;
	Random.Initialize(static_cast<int32>(Site.Top.X + Site.Top.Y));
}

void ADBWaterfall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Spray where the water lands - only where someone can see it.
	const APlayerController* Local = GetWorld()->GetFirstPlayerController();
	if (Foot.IsZero() || !Local || !Local->PlayerCameraManager || GetNetMode() == NM_DedicatedServer
		|| FVector::DistSquared(Local->PlayerCameraManager->GetCameraLocation(), Foot) > FMath::Square(25000.f))
	{
		return;
	}
	for (int32 Burst = 0; Burst < 2; ++Burst)
	{
		const FVector At = Foot + FVector(Random.FRandRange(-FootRadius, FootRadius), Random.FRandRange(-FootRadius, FootRadius), 0.f);
		DBCombatFeedback::Play(this, EDBCombatFx::WaterSplash, At, FVector::UpVector, FootRadius / 450.f);
	}
}
