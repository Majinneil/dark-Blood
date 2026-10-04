#include "World/DBSettlementLifeComponent.h"

#include "Character/DBLesserDemon.h"
#include "Character/DBVillagerCharacter.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "Framework/DBGameState.h"
#include "GameFramework/PlayerState.h"
#include "Player/DBPlayerController.h"
#include "World/DBRealmLayout.h"
#include "World/DBWorldStateComponent.h"

#include "DarkBloodRules/Settlement.h"

#define LOCTEXT_NAMESPACE "DarkBloodSettlementLife"

namespace
{
	/** Settlements come alive within this distance (m) beyond their radius and empty again beyond the second. */
	constexpr double ActivateMargin = 400.0;
	constexpr double DeactivateMargin = 700.0;
	constexpr int32 MaxVillagers = 20;

	const TCHAR* VillagerNames[] = {TEXT("Taro"), TEXT("Hanako"), TEXT("Kenta"), TEXT("Yuki"), TEXT("Haru"), TEXT("Aiko"), TEXT("Sora"), TEXT("Rin"),
		TEXT("Daichi"), TEXT("Emi"), TEXT("Goro"), TEXT("Mei"), TEXT("Isamu"), TEXT("Nori"), TEXT("Akira"), TEXT("Chiyo"), TEXT("Botan"), TEXT("Ren"),
		TEXT("Kaede"), TEXT("Jiro")};

	FVector SiteCenter(const FDBRealmSettlement& Site)
	{
		return FVector(Site.Center.X * 100.0, Site.Center.Y * 100.0, Site.GroundHeight * 100.0);
	}

	const UDBWorldStateComponent* GetWorldState(const UWorld* World)
	{
		const ADBGameState* GameState = World ? World->GetGameState<ADBGameState>() : nullptr;
		return GameState ? GameState->GetWorldState() : nullptr;
	}
}

UDBSettlementLifeComponent::UDBSettlementLifeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDBSettlementLifeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
		return;
	}
	Sites.SetNum(DBRealm::GetSettlements().Num());
}

void UDBSettlementLifeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDBWorldStateComponent* WorldState = const_cast<UDBWorldStateComponent*>(GetWorldState(GetWorld())))
	{
		WorldState->OnSettlementEvent.Remove(EventHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UDBSettlementLifeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// The game state may arrive after the director's BeginPlay.
	if (!EventHandle.IsValid())
	{
		if (UDBWorldStateComponent* WorldState = const_cast<UDBWorldStateComponent*>(GetWorldState(GetWorld())))
		{
			EventHandle = WorldState->OnSettlementEvent.AddUObject(this, &UDBSettlementLifeComponent::OnSettlementEvent);
		}
	}
	UpdateTimer -= DeltaTime;
	if (UpdateTimer <= 0.f)
	{
		UpdateTimer = 2.f;
		UpdateSites();
	}
}

float UDBSettlementLifeComponent::GetNearestPlayerDistance(int32 SiteIndex) const
{
	const FVector Center = SiteCenter(DBRealm::GetSettlements()[SiteIndex]);
	float Nearest = TNumericLimits<float>::Max();
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr)
			{
				Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(Pawn->GetActorLocation(), Center)));
			}
		}
	}
	return Nearest;
}

void UDBSettlementLifeComponent::UpdateSites()
{
	const UDBWorldStateComponent* WorldState = GetWorldState(GetWorld());
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	if (!WorldState)
	{
		return;
	}
	for (int32 Index = 0; Index < Sites.Num() && Index < Settlements.Num(); ++Index)
	{
		FSiteLife& Life = Sites[Index];
		const FDBRealmSettlement& Site = Settlements[Index];
		Life.Villagers.RemoveAll([](const TWeakObjectPtr<ADBVillagerCharacter>& Villager) { return !Villager.IsValid() || Villager->IsDead(); });

		const float Distance = GetNearestPlayerDistance(Index) / 100.f;
		const bool bWasActive = Life.bActive;
		Life.bActive = Distance < Site.Radius + (Life.bActive ? DeactivateMargin : ActivateMargin);
		if (!Life.bActive)
		{
			for (const TWeakObjectPtr<ADBVillagerCharacter>& Villager : Life.Villagers)
			{
				Villager->Destroy();
			}
			Life.Villagers.Reset();
			if (bWasActive)
			{
				UE_LOG(LogDBWorld, Log, TEXT("Settlement %s: abstract again"), Site.Name);
			}
			continue;
		}
		// As many residents in the streets as the simulation supports; most stay indoors at night.
		FDBSettlementView View;
		const int32 Population = WorldState->GetSettlementView(FName(Site.Name), View) ? View.Population : 0;
		int32 Wanted = FMath::Clamp(Population / 12, Population > 0 ? 3 : 0, MaxVillagers);
		if (WorldState->IsNight())
		{
			Wanted = FMath::Min(Wanted, FMath::Max(1, Wanted / 4));
		}
		if (!bWasActive)
		{
			UE_LOG(LogDBWorld, Display, TEXT("Settlement %s: active (%d residents, %d in the streets)"), Site.Name, Population, Wanted);
		}
		// A few per update, so arriving never hitches.
		for (int32 Spawned = 0; Spawned < 3 && Life.Villagers.Num() < Wanted; ++Spawned)
		{
			SpawnVillager(Index);
		}
		if (Life.Villagers.Num() > Wanted)
		{
			Life.Villagers.Pop()->Destroy();
		}
	}
}

void UDBSettlementLifeComponent::SpawnVillager(int32 SiteIndex)
{
	const FDBRealmSettlement& Site = DBRealm::GetSettlements()[SiteIndex];
	FSiteLife& Life = Sites[SiteIndex];
	const int32 Seed = static_cast<int32>(FCrc::StrCrc32(Site.Name)) + Life.SpawnCounter * 7919;
	FRandomStream Random(Seed);
	const float Radius = Site.Radius * 100.f * 0.7f;
	const FVector Home = SiteCenter(Site);
	const FVector2D Offset = FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)).GetSafeNormal() * Random.FRandRange(0.f, Radius);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADBVillagerCharacter* Villager = GetWorld()->SpawnActor<ADBVillagerCharacter>(ADBVillagerCharacter::StaticClass(),
		Home + FVector(Offset, 120.f), FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Params);
	if (Villager)
	{
		Villager->SetupVillager(FName(Site.Name), FText::FromString(VillagerNames[Life.SpawnCounter % UE_ARRAY_COUNT(VillagerNames)]), Home, Radius, Seed);
		Life.Villagers.Add(Villager);
	}
	++Life.SpawnCounter;
}

void UDBSettlementLifeComponent::SpawnAttackers(int32 SiteIndex)
{
	const FDBRealmSettlement& Site = DBRealm::GetSettlements()[SiteIndex];
	int32 PlayersNear = 0;
	const FVector Center = SiteCenter(Site);
	const double NotifyRange = (Site.Radius + 1000.0) * 100.0;
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
			if (!Pawn || FVector::Dist2D(Pawn->GetActorLocation(), Center) > NotifyRange)
			{
				continue;
			}
			++PlayersNear;
			if (ADBPlayerController* Controller = Cast<ADBPlayerController>(Pawn->GetController()))
			{
				Controller->ClientShowNotification(FText::Format(LOCTEXT("Attack", "Daemonenangriff auf {0}!"), FText::FromString(Site.Name)));
			}
		}
	}
	// They come out of the dark from one side; more players, more demons.
	const int32 Count = FMath::Clamp(2 + PlayersNear, 2, 6);
	FRandomStream Random(static_cast<int32>(GetWorld()->GetTimeSeconds() * 1000.0));
	const float Angle = Random.FRandRange(0.f, 2.f * UE_PI);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float A = Angle + (Index - Count * 0.5f) * 0.12f;
		const FVector2D Meters = Site.Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Site.Radius * 0.9);
		const double Ground = FMath::Max(DBRealm::SampleHeight(Meters.X, Meters.Y), Site.GroundHeight);
		const FVector Location(Meters.X * 100.0, Meters.Y * 100.0, Ground * 100.0 + 150.0);
		GetWorld()->SpawnActor<ADBLesserDemon>(ADBLesserDemon::StaticClass(), Location, (Center - Location).Rotation(), Params);
	}
	UE_LOG(LogDBWorld, Display, TEXT("Settlement %s: demon attack with %d demons (%d players there)"), Site.Name, Count, PlayersNear);
}

void UDBSettlementLifeComponent::OnSettlementEvent(const DarkBlood::Rules::FSettlementEvent& Event)
{
	namespace R = DarkBlood::Rules;
	if (Event.Kind != R::ESettlementEventKind::DemonAttack && Event.Kind != R::ESettlementEventKind::AttackRepelled)
	{
		return;
	}
	const int32 Index = FindSite(UTF8_TO_TCHAR(Event.SettlementId.c_str()));
	if (Sites.IsValidIndex(Index) && Sites[Index].bActive)
	{
		SpawnAttackers(Index);
	}
}

int32 UDBSettlementLifeComponent::FindSite(const FString& NamePrefix) const
{
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	for (int32 Index = 0; Index < Settlements.Num(); ++Index)
	{
		if (FString(Settlements[Index].Name).Equals(NamePrefix, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}
	for (int32 Index = 0; Index < Settlements.Num(); ++Index)
	{
		if (FString(Settlements[Index].Name).StartsWith(NamePrefix, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool UDBSettlementLifeComponent::StartAttackEncounter(const FString& SettlementName)
{
	const int32 Index = FindSite(SettlementName);
	if (!Sites.IsValidIndex(Index) || GetNearestPlayerDistance(Index) / 100.f > DBRealm::GetSettlements()[Index].Radius + DeactivateMargin)
	{
		return false;
	}
	SpawnAttackers(Index);
	return true;
}

int32 UDBSettlementLifeComponent::CountVillagers(const FString& SettlementName) const
{
	const int32 Index = FindSite(SettlementName);
	if (!Sites.IsValidIndex(Index))
	{
		return -1;
	}
	int32 Count = 0;
	for (const TWeakObjectPtr<ADBVillagerCharacter>& Villager : Sites[Index].Villagers)
	{
		Count += Villager.IsValid() ? 1 : 0;
	}
	return Count;
}

#undef LOCTEXT_NAMESPACE
