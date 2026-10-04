#include "Character/DBVillagerCharacter.h"

#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DBPlayerController.h"
#include "World/DBRealmLayout.h"
#include "World/DBWorldStateComponent.h"

#define LOCTEXT_NAMESPACE "DarkBloodVillager"

ADBVillagerCharacter::ADBVillagerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCharacterMovement()->MaxWalkSpeed = 140.f;
	// Wanders by movement input without a controller (like the enemies).
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationYaw = false;
	NpcId = TEXT("Villager");
}

void ADBVillagerCharacter::SetupVillager(FName InSettlementId, const FText& InName, const FVector& InHome, float InWanderRadius, int32 InSeed)
{
	Setup(TEXT("Villager"), InName, NAME_None);
	SettlementId = InSettlementId;
	Home = InHome;
	WanderRadius = InWanderRadius;
	Random.Initialize(InSeed);
	RemarkIndex = InSeed;
	IdleSeconds = Random.FRandRange(0.f, 4.f);
	PickWanderTarget();
}

void ADBVillagerCharacter::PickWanderTarget()
{
	const FVector2D Offset = FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)).GetSafeNormal() * Random.FRandRange(0.f, WanderRadius);
	WanderTarget = Home + FVector(Offset, 0.f);
	WalkSeconds = 0.f;
}

void ADBVillagerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || IsDead())
	{
		return;
	}
	// Stand still for a player close by (conversation distance).
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (It->IsPlayerControlled() && FVector::DistSquared2D(It->GetActorLocation(), GetActorLocation()) < FMath::Square(250.f))
		{
			return;
		}
	}
	if (IdleSeconds > 0.f)
	{
		IdleSeconds -= DeltaSeconds;
		return;
	}
	WalkSeconds += DeltaSeconds;
	const FVector ToTarget = WanderTarget - GetActorLocation();
	if (ToTarget.Size2D() < 80.f || WalkSeconds > 25.f)
	{
		// Arrived (or blocked by a house): pause, then walk somewhere else.
		IdleSeconds = Random.FRandRange(2.f, 7.f);
		PickWanderTarget();
		return;
	}
	AddMovementInput(FVector(ToTarget.X, ToTarget.Y, 0.f).GetSafeNormal());
}

FText ADBVillagerCharacter::MakeRemark() const
{
	const ADBGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADBGameState>() : nullptr;
	const UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr;
	FDBSettlementView View;
	if (!WorldState || !WorldState->GetSettlementView(SettlementId, View))
	{
		return LOCTEXT("Generic", "Bleib wachsam, Wanderer.");
	}
	bool bLiberated = false;
	for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
	{
		if (SettlementId == FName(Site.Name))
		{
			const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
			const int32 Index = DBRealm::FindRegionIndex(Site.Center.X, Site.Center.Y);
			FDBRegionStateView Region;
			bLiberated = Regions.IsValidIndex(Index) && WorldState->GetRegionState(Regions[Index].RegionId, Region) && Region.Control == EDBRegionControl::Liberated;
			break;
		}
	}
	const double Now = WorldState->GetDay() * 24.0 - 24.0 + WorldState->GetTimeOfDay();
	const FText Place = FText::FromName(SettlementId);
	if (View.LastAttackHours >= 0.f && Now - View.LastAttackHours < 24.0)
	{
		return FText::Format(LOCTEXT("Attack", "Vor {0} Stunden haben uns die Daemonen ueberfallen. Die Wachen sind erschoepft."),
			FText::AsNumber(FMath::Max(1, FMath::RoundToInt(Now - View.LastAttackHours))));
	}
	if (View.bHungry)
	{
		return LOCTEXT("Hungry", "Die Vorraete sind aufgebraucht. Die Kinder hungern.");
	}
	if (View.GetWorstCondition() < 0.5f)
	{
		return LOCTEXT("Ruins", "Seit dem letzten Angriff liegen Haeuser in Truemmern. Wir bauen wieder auf.");
	}
	if (bLiberated)
	{
		return LOCTEXT("Liberated", "Der Vasall ist gefallen! Endlich schlafen wir wieder ohne Angst.");
	}
	if (View.GetThreat() > 0.6f && View.GetSecurity() < 0.4f)
	{
		return LOCTEXT("Danger", "Zu wenig Wachen, zu viele Daemonen. Jede Nacht beten wir zu den Ahnen.");
	}
	if (View.FoodDays < 3)
	{
		return LOCTEXT("LowFood", "Die Ernte reicht kaum bis zum naechsten Mond.");
	}
	if (View.GetProsperity() > 0.7f)
	{
		return LOCTEXT("Prosperous", "Die Maerkte sind voll, die Felder reich. Gute Zeiten, Wanderer.");
	}
	switch (RemarkIndex % 3)
	{
	case 0: return FText::Format(LOCTEXT("Count", "{0} zaehlt {1} Seelen und {2} Wachen. Jede Klinge hilft."), Place, FText::AsNumber(View.Population), FText::AsNumber(View.Guards));
	case 1: return LOCTEXT("Lanterns", "Bei Nacht bleib in der Naehe der Laternen.");
	default: return LOCTEXT("Rumor", "Die Wachen sagen, an den Grenzen sammeln sich Daemonen.");
	}
}

void ADBVillagerCharacter::Interact(APlayerController* User)
{
	ADBPlayerController* Speaker = Cast<ADBPlayerController>(User);
	if (!Speaker || !HasAuthority())
	{
		return;
	}
	if (const APawn* Pawn = Speaker->GetPawn())
	{
		SetActorRotation(FRotator(0.f, (Pawn->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
	}
	++RemarkIndex;
	Speaker->ClientShowNotification(FText::Format(LOCTEXT("Says", "{0}: \"{1}\""), FText::FromString(GetCombatDisplayName()), MakeRemark()));
}

#undef LOCTEXT_NAMESPACE
