#include "World/DBRealmDirector.h"

#include "Boss/DBBoss.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Player/DBPlayerState.h"
#include "World/DBRealmLayout.h"
#include "World/DBCarriageStation.h"
#include "World/DBDungeon.h"
#include "World/DBRegionLife.h"
#include "World/DBTheEnd.h"
#include "World/DBParadise.h"
#include "World/DBRealmMoodComponent.h"
#include "World/DBRealmSkyComponent.h"
#include "World/DBSettlementLifeComponent.h"
#include "World/DBSettlementBuilder.h"
#include "World/DBShip.h"
#include "Art/DBArtBuilder.h"
#include "Art/DBModularBuilding.h"
#include "Algo/Reverse.h"
#include "Art/DBArtBatcher.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Settlements appear within this distance of the camera (m) and go away beyond the second. */
	constexpr double BuildDistance = 2800.0;
	constexpr double ClearDistance = 3500.0;
	/** Game thread time per frame for building settlement houses (s). */
	constexpr double HouseBudgetSeconds = 0.003;
	/** Houses per shared block: few primitives, yet each block registers without a hitch. */
	constexpr int32 HousesPerBatch = 40;
}

FDBRealmSiteBatch::~FDBRealmSiteBatch() = default;

ADBRealmDirector::ADBRealmDirector()
{
	Mood = CreateDefaultSubobject<UDBRealmMoodComponent>(TEXT("Mood"));
	Sky = CreateDefaultSubobject<UDBRealmSkyComponent>(TEXT("Sky"));
	Life = CreateDefaultSubobject<UDBSettlementLifeComponent>(TEXT("Life"));
	RegionLife = CreateDefaultSubobject<UDBRegionLifeComponent>(TEXT("RegionLife"));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = false;
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif
}

ADBRealmDirector* ADBRealmDirector::Get(const UWorld* World)
{
	for (TActorIterator<ADBRealmDirector> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADBRealmDirector::BeginPlay()
{
	Super::BeginPlay();
	Sites.SetNum(DBRealm::GetSettlements().Num());
	if (HasAuthority())
	{
		// Sailing ships wait in the water off every harbor: the capital's harbor holds the whole fleet.
		for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
		{
			if (Site.SeaDirection.IsZero())
			{
				continue;
			}
			TArray<EDBShipStyle> Fleet;
			if (Site.Type == EDBSettlementType::FishingVillage)
			{
				Fleet = {EDBShipStyle::Boat, EDBShipStyle::Boat};
			}
			else if (FCString::Strcmp(Site.Name, TEXT("Hauptstadthafen")) == 0)
			{
				Fleet = {EDBShipStyle::Boat, EDBShipStyle::Fighting, EDBShipStyle::War, EDBShipStyle::Merchant};
			}
			else
			{
				Fleet = {EDBShipStyle::Boat, EDBShipStyle::Merchant, EDBShipStyle::Fighting};
			}
			const FVector2D Side(-Site.SeaDirection.Y, Site.SeaDirection.X);
			const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Side.Y, Side.X)));
			for (int32 Index = 0; Index < Fleet.Num(); ++Index)
			{
				const double Along = (Index - (Fleet.Num() - 1) * 0.5) * 75.0;
				const double Out = Fleet[Index] == EDBShipStyle::Boat ? 60.0 : 115.0;
				const FVector2D At = Site.Center + Site.SeaDirection * (Site.ShoreDistance + Out) + Side * Along;
				const FText Name = FText::Format(FText::FromString(TEXT("{0} von {1}")), DBShipArt::GetSpec(Fleet[Index]).DisplayName, FText::FromString(Site.Name));
				ADBShip::SpawnAt(GetWorld(), At * 100.0, Yaw, Name, Fleet[Index]);
			}
		}
		// Every settlement has a carriage to every other one.
		ADBCarriageStation::SpawnStations(GetWorld());
		// Dungeon gates in their regions.
		ADBDungeonPortal::SpawnEntrances(GetWorld());
		// Arenas of the 16 vassals and the demon king.
		ADBBossArena::SpawnArenas(GetWorld());
		// A demon camp in every vassal region (Phase 11).
		ADBDemonCamp::SpawnCamps(GetWorld());
		// DAS ENDE: the gate with its blood seal, the Last Bastion, the path to the throne (Phase 13).
		DBTheEnd::SpawnLandmarks(GetWorld());
		// The Paradise above DAS ENDE and the gate of light at the throne (Phase 15).
		DBParadise::SpawnParadise(GetWorld());
	}
	UE_LOG(LogDarkBlood, Log, TEXT("DBREALM director ready: %d regions, %d settlements"), DBRealm::GetRegions().Num(), Sites.Num());
}

void ADBRealmDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int32 Index = 0; Index < Sites.Num(); ++Index)
	{
		ClearSettlement(Index);
	}
	Super::EndPlay(EndPlayReason);
}

void ADBRealmDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RegionTimer -= DeltaSeconds;
	if (HasAuthority() && RegionTimer <= 0.f)
	{
		RegionTimer = 1.f;
		UpdatePlayerRegions();
	}
	BuildPendingHouses();
	StreamTimer -= DeltaSeconds;
	if (StreamTimer <= 0.f && GetNetMode() != NM_DedicatedServer)
	{
		StreamTimer = 1.f;
		StreamSettlements();
	}
}

void ADBRealmDirector::UpdatePlayerRegions()
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}
	const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
	for (APlayerState* State : GameState->PlayerArray)
	{
		ADBPlayerState* PlayerState = Cast<ADBPlayerState>(State);
		const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		if (DBParadise::IsInParadise(Pawn->GetActorLocation()))
		{
			PlayerState->SetRealmRegion(TEXT("Paradise"));
			continue;
		}
		const FVector2D Realm = ToRealm(Pawn->GetActorLocation());
		PlayerState->SetRealmRegion(Regions[DBRealm::FindRegionIndex(Realm.X, Realm.Y)].RegionId);
	}
}

void ADBRealmDirector::StreamSettlements()
{
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Camera)
	{
		return;
	}
	const FVector2D Viewer = ToRealm(Camera->GetCameraLocation());
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	for (int32 Index = 0; Index < Settlements.Num() && Index < Sites.Num(); ++Index)
	{
		const double Distance = FVector2D::Distance(Viewer, Settlements[Index].Center);
		if (!Sites[Index].bBuilt && Distance < BuildDistance)
		{
			BuildSettlement(Index);
		}
		else if (Sites[Index].bBuilt && Distance > ClearDistance)
		{
			ClearSettlement(Index);
		}
	}
}

void ADBRealmDirector::BuildSettlement(int32 Index)
{
	const FDBRealmSettlement& Site = DBRealm::GetSettlements()[Index];
	const double StartSeconds = FPlatformTime::Seconds();
	const int32 Seed = 1000 + Index;
	const FTransform Frame(FRotator(0.f, DBSettlements::FrameYaw(Site, Seed), 0.f), FVector(Site.Center.X * 100.0, Site.Center.Y * 100.0, Site.GroundHeight * 100.0));
	DBArtBuild::FArtBuilder Builder{*GetWorld(), Frame, Sites[Index].Actors};
	// Houses are placed now but built later, a few per frame, into one shared batch for the whole settlement.
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	if (AActor* BatchActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params))
	{
		USceneComponent* Root = NewObject<USceneComponent>(BatchActor, TEXT("Root"), RF_Transient);
		Root->SetMobility(EComponentMobility::Static);
		BatchActor->SetRootComponent(Root);
		Root->RegisterComponent();
		Sites[Index].BatchRoot = Root;
		Sites[Index].Actors.Add(BatchActor);
	}
	ADBModularBuilding::SetDeferRebuild(Sites[Index].BatchRoot.IsValid());
	DBSettlements::Build(Builder, Site, Seed);
	ADBModularBuilding::SetDeferRebuild(false);
	for (AActor* Actor : Sites[Index].Actors)
	{
		if (ADBModularBuilding* House = Cast<ADBModularBuilding>(Actor); House && Sites[Index].BatchRoot.IsValid())
		{
			Sites[Index].PendingHouses.Add(House);
		}
	}
	Algo::Reverse(Sites[Index].PendingHouses); // Pop() takes from the back: build in placement order
	Sites[Index].bBuilt = true;
	UE_LOG(LogDarkBlood, Log, TEXT("DBREALM settlement %s built: %d actors in %.0f ms"), Site.Name, Sites[Index].Actors.Num(),
		(FPlatformTime::Seconds() - StartSeconds) * 1000.0);
}

void ADBRealmDirector::FinishBatch(FDBRealmSiteActors& Site)
{
	if (!Site.Batch)
	{
		return;
	}
	for (UInstancedStaticMeshComponent* Component : Site.Batch->Components)
	{
		if (Component && !Component->IsRegistered())
		{
			Component->RegisterComponent();
			++Site.BatchComponents;
			Site.BatchInstances += Component->GetInstanceCount();
		}
	}
	Site.Batch.Reset();
}

void ADBRealmDirector::BuildPendingHouses()
{
	const double Deadline = FPlatformTime::Seconds() + HouseBudgetSeconds;
	for (int32 Index = 0; Index < Sites.Num(); ++Index)
	{
		FDBRealmSiteActors& Site = Sites[Index];
		if (Site.PendingHouses.IsEmpty() || !Site.BatchRoot.IsValid())
		{
			continue;
		}
		while (!Site.PendingHouses.IsEmpty() && FPlatformTime::Seconds() < Deadline)
		{
			if (!Site.Batch)
			{
				Site.Batch = MakeShared<FDBRealmSiteBatch>();
				Site.Batch->Batcher = MakeUnique<FDBArtBatcher>(*Site.BatchRoot->GetOwner(), *Site.BatchRoot.Get(), Site.Batch->Components);
				Site.Batch->Batcher->SetDeferRegister(true);
			}
			if (ADBModularBuilding* House = Site.PendingHouses.Pop(EAllowShrinking::No).Get())
			{
				ADBModularBuilding::SetSharedBatcher(Site.Batch->Batcher.Get());
				House->Rebuild();
				ADBModularBuilding::SetSharedBatcher(nullptr);
				++Site.Batch->Houses;
			}
			if (Site.Batch->Houses >= HousesPerBatch || Site.PendingHouses.IsEmpty())
			{
				FinishBatch(Site);
			}
		}
		if (Site.PendingHouses.IsEmpty())
		{
			UE_LOG(LogDarkBlood, Log, TEXT("DBREALM settlement %s: houses done, %d shared components, %d instances"), DBRealm::GetSettlements()[Index].Name,
				Site.BatchComponents, Site.BatchInstances);
		}
		if (FPlatformTime::Seconds() >= Deadline)
		{
			return;
		}
	}
}

void ADBRealmDirector::ClearSettlement(int32 Index)
{
	for (AActor* Actor : Sites[Index].Actors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Sites[Index].Actors.Reset();
	Sites[Index].PendingHouses.Reset();
	Sites[Index].Batch.Reset();
	Sites[Index].BatchRoot.Reset();
	Sites[Index].BatchComponents = 0;
	Sites[Index].BatchInstances = 0;
	Sites[Index].bBuilt = false;
}
