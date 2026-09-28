#include "World/DBRealmDirector.h"

#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Player/DBPlayerState.h"
#include "World/DBRealmLayout.h"
#include "World/DBSettlementBuilder.h"
#include "Art/DBArtBuilder.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Settlements appear within this distance of the camera (m) and go away beyond the second. */
	constexpr double BuildDistance = 2800.0;
	constexpr double ClearDistance = 3500.0;
}

ADBRealmDirector::ADBRealmDirector()
{
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
	DBSettlements::Build(Builder, Site, Seed);
	Sites[Index].bBuilt = true;
	UE_LOG(LogDarkBlood, Log, TEXT("DBREALM settlement %s built: %d actors in %.0f ms"), Site.Name, Sites[Index].Actors.Num(),
		(FPlatformTime::Seconds() - StartSeconds) * 1000.0);
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
	Sites[Index].bBuilt = false;
}
