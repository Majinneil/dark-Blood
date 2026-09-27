#include "Character/DBTrainingDummy.h"

#include "Abilities/DBMeleeAttackAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

ADBTrainingDummy::ADBTrainingDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EnemyId = TEXT("TrainingDummy");
	DisplayName = FText::FromString(TEXT("Trainingspuppe [DEV]"));
	Level = 1;
	MaxHealth = 60.f;
	MaxPoise = 30.f;
	XpReward = 20;
	AttackAbility = UDBAbility_EnemySwing::StaticClass();
	bRespawnInPlace = true;
	RespawnSeconds = 3.f;

	// Stationary and anchored: it only turns and swings.
	GetCharacterMovement()->MaxWalkSpeed = 0.f;
	KnockbackScale = 0.f;
	PlaceholderColor = FLinearColor(0.45f, 0.32f, 0.2f); // straw/wood
}

void ADBTrainingDummy::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && AutoAttackInterval > 0.f)
	{
		SetAutoAttack(AutoAttackInterval);
	}
}

bool ADBTrainingDummy::SwingAtNearestPlayer()
{
	AActor* Target = FindNearestPlayer(AttackReach);
	return Target && AttackTarget(Target);
}

void ADBTrainingDummy::SetAutoAttack(float IntervalSeconds)
{
	AutoAttackInterval = FMath::Max(0.f, IntervalSeconds);
	if (AutoAttackInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(AutoAttackTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SwingAtNearestPlayer(); }),
			AutoAttackInterval, true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(AutoAttackTimer);
	}
}
