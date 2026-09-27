#include "AI/DBMeleeAIComponent.h"

#include "Character/DBEnemyCharacter.h"
#include "Combat/DBCombatStatics.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

UDBMeleeAIComponent::UDBMeleeAIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
}

void UDBMeleeAIComponent::BeginPlay()
{
	Super::BeginPlay();
	HomeLocation = GetOwner()->GetActorLocation();
	// Decisions are server-side; clients only see the replicated movement.
	SetComponentTickEnabled(GetOwner()->HasAuthority());
}

ADBEnemyCharacter* UDBMeleeAIComponent::GetEnemy() const
{
	return Cast<ADBEnemyCharacter>(GetOwner());
}

void UDBMeleeAIComponent::SetState(EDBMeleeAIState NewState)
{
	if (State == NewState)
	{
		return;
	}
	UE_LOG(LogDBCombat, Log, TEXT("%s AI: %s -> %s"), *DBCombat::GetCombatName(GetOwner()), *UEnum::GetValueAsString(State),
		*UEnum::GetValueAsString(NewState));
	State = NewState;
	StateTime = 0.f;
}

void UDBMeleeAIComponent::NotifyAttackedBy(AActor* Attacker)
{
	if (const APlayerState* PlayerState = Cast<APlayerState>(Attacker))
	{
		Attacker = PlayerState->GetPawn();
	}
	if ((State == EDBMeleeAIState::Idle || State == EDBMeleeAIState::ReturnHome) && DBCombat::CanTarget(GetOwner(), Attacker))
	{
		Target = Attacker;
		SetState(EDBMeleeAIState::Chase);
	}
}

bool UDBMeleeAIComponent::CanSee(const AActor* Candidate) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DBAISight), false, GetOwner());
	Params.AddIgnoredActor(Candidate);
	const FVector Eye = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	return !GetWorld()->LineTraceTestByChannel(Eye, Candidate->GetActorLocation(), ECC_Visibility, Params);
}

AActor* UDBMeleeAIComponent::FindTarget() const
{
	const ADBEnemyCharacter* Enemy = GetEnemy();
	AActor* Candidate = Enemy ? Enemy->FindNearestPlayer(SightRadius) : nullptr;
	return Candidate && CanSee(Candidate) ? Candidate : nullptr;
}

void UDBMeleeAIComponent::MoveTowards(const FVector& Location, float AcceptRadius)
{
	ADBEnemyCharacter* Enemy = GetEnemy();
	const FVector ToTarget = Location - Enemy->GetActorLocation();
	if (ToTarget.Size2D() > AcceptRadius)
	{
		// Direct steering: good enough for open test spaces; navigation-based movement comes with real encounters.
		Enemy->AddMovementInput(ToTarget.GetSafeNormal2D());
	}
}

void UDBMeleeAIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ADBEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy)
	{
		return;
	}
	if (Enemy->IsDead())
	{
		Target.Reset();
		SetState(EDBMeleeAIState::Idle);
		return;
	}

	StateTime += DeltaTime;
	Cooldown = FMath::Max(0.f, Cooldown - DeltaTime);
	if (Enemy->IsMovementInputBlocked() && State != EDBMeleeAIState::Attack)
	{
		return; // staggered / knocked down: no decisions until it recovers
	}

	AActor* CurrentTarget = Target.Get();
	const bool bTargetValid = CurrentTarget && DBCombat::CanTarget(Enemy, CurrentTarget);
	const float DistanceToTarget = bTargetValid ? FVector::Dist2D(Enemy->GetActorLocation(), CurrentTarget->GetActorLocation()) : 0.f;
	const float DistanceFromHome = FVector::Dist2D(Enemy->GetActorLocation(), HomeLocation);

	switch (State)
	{
	case EDBMeleeAIState::Idle:
		SearchTimer -= DeltaTime;
		if (SearchTimer <= 0.f)
		{
			SearchTimer = SearchInterval;
			if (AActor* Found = FindTarget())
			{
				Target = Found;
				SetState(EDBMeleeAIState::Chase);
			}
		}
		break;

	case EDBMeleeAIState::Chase:
		if (!bTargetValid || DistanceToTarget > LeashRadius || DistanceFromHome > LeashRadius)
		{
			Target.Reset();
			SetState(EDBMeleeAIState::ReturnHome);
		}
		else if (DistanceToTarget <= AttackDistance && Cooldown <= 0.f)
		{
			if (Enemy->AttackTarget(CurrentTarget))
			{
				SetState(EDBMeleeAIState::Attack);
			}
		}
		else
		{
			MoveTowards(CurrentTarget->GetActorLocation(), AttackDistance * 0.8f);
		}
		break;

	case EDBMeleeAIState::Attack:
		// The attack ability owns the body until it ends (or is interrupted by a stagger).
		if (!Enemy->IsMovementInputBlocked() || StateTime > 5.f)
		{
			Cooldown = FMath::FRandRange(MinAttackCooldown, MaxAttackCooldown);
			SetState(EDBMeleeAIState::Recover);
		}
		break;

	case EDBMeleeAIState::Recover:
		if (bTargetValid)
		{
			const FRotator Facing(0.f, (CurrentTarget->GetActorLocation() - Enemy->GetActorLocation()).Rotation().Yaw, 0.f);
			Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), Facing, DeltaTime, 6.f));
		}
		if (Cooldown <= 0.f)
		{
			SetState(bTargetValid ? EDBMeleeAIState::Chase : EDBMeleeAIState::ReturnHome);
		}
		break;

	case EDBMeleeAIState::ReturnHome:
		if (AActor* Found = FindTarget(); Found && DistanceFromHome < LeashRadius * 0.5f)
		{
			Target = Found;
			SetState(EDBMeleeAIState::Chase);
		}
		else if (DistanceFromHome < 100.f)
		{
			SetState(EDBMeleeAIState::Idle);
		}
		else
		{
			MoveTowards(HomeLocation, 80.f);
		}
		break;
	}
}
