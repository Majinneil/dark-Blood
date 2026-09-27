#include "Combat/DBLockOnComponent.h"

#include "Character/DBCharacterBase.h"
#include "Combat/DBCombatStatics.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace R = DarkBlood::Rules;

UDBLockOnComponent::UDBLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UDBLockOnComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
		return;
	}
	if (AActor* Target = FindBestTarget(0.f, nullptr))
	{
		SetLockTarget(Target);
	}
}

void UDBLockOnComponent::ClearLockOn()
{
	if (IsLockedOn())
	{
		SetLockTarget(nullptr);
	}
}

bool UDBLockOnComponent::SwitchTarget(float Direction)
{
	if (!IsLockedOn() || FMath::IsNearlyZero(Direction))
	{
		return false;
	}
	AActor* Next = FindBestTarget(FMath::Sign(Direction), LockTarget.Get());
	if (Next)
	{
		SetLockTarget(Next);
	}
	return Next != nullptr;
}

void UDBLockOnComponent::AddSwitchInput(float YawInput)
{
	SwitchAccumulator += YawInput;
	if (SwitchCooldown <= 0.f && FMath::Abs(SwitchAccumulator) >= SwitchThreshold)
	{
		SwitchTarget(SwitchAccumulator);
		SwitchAccumulator = 0.f;
		SwitchCooldown = 0.35f;
	}
}

void UDBLockOnComponent::SetLockTarget(AActor* NewTarget)
{
	LockTarget = NewTarget;
	ApplyStrafeMode(NewTarget != nullptr);
	UE_LOG(LogDBCombat, Log, TEXT("%s lock-on: %s"), *DBCombat::GetCombatName(GetOwner()),
		NewTarget ? *DBCombat::GetCombatName(NewTarget) : TEXT("released"));
	if (!GetOwner()->HasAuthority())
	{
		ServerSetLockTarget(NewTarget);
	}
}

void UDBLockOnComponent::ServerSetLockTarget_Implementation(AActor* NewTarget)
{
	// A hint only: the server re-checks that the target is a valid enemy.
	LockTarget = DBCombat::CanTarget(GetOwner(), NewTarget) ? NewTarget : nullptr;
	ApplyStrafeMode(LockTarget.IsValid());
}

void UDBLockOnComponent::ApplyStrafeMode(bool bLocked)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		// Locked: face the control rotation (which tracks the target) and strafe around it.
		Movement->bOrientRotationToMovement = !bLocked;
		Movement->bUseControllerDesiredRotation = bLocked;
	}
}

AActor* UDBLockOnComponent::FindBestTarget(float SideFilter, const AActor* Exclude) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Controller = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	UWorld* World = GetWorld();
	if (!Controller || !World)
	{
		return nullptr;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	R::FLockOnRules Rules = DBCombat::GetLockOnRules();
	if (SideFilter != 0.f)
	{
		Rules.MaxAngleDegrees *= 2.f; // switching may reach further to the side
	}

	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<ADBCharacterBase> It(World); It; ++It)
	{
		ADBCharacterBase* Candidate = *It;
		if (Candidate == Exclude || !DBCombat::CanTarget(Pawn, Candidate))
		{
			continue;
		}
		const FVector ToTarget = Candidate->GetActorLocation() - ViewLocation;
		const float YawOffset = FMath::FindDeltaAngleDegrees(ViewRotation.Yaw, ToTarget.Rotation().Yaw);
		if (SideFilter != 0.f && FMath::Sign(YawOffset) != FMath::Sign(SideFilter))
		{
			continue;
		}
		// Horizontal angle only: looking at the ground must not prevent locking onto the enemy in front.
		const float Angle = FMath::Abs(YawOffset);
		const float Distance = FVector::Dist(Pawn->GetActorLocation(), Candidate->GetActorLocation());

		FCollisionQueryParams Params(SCENE_QUERY_STAT(DBLockOnSight), false, Pawn);
		Params.AddIgnoredActor(Candidate);
		const bool bVisible = !World->LineTraceTestByChannel(ViewLocation, Candidate->GetActorLocation(), ECC_Visibility, Params);

		float Score = R::ScoreLockOnCandidate(Distance, Angle, bVisible, Rules);
		if (Score < 0.f)
		{
			continue;
		}
		if (SideFilter != 0.f)
		{
			Score = FMath::Abs(YawOffset); // the nearest target on that side
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

void UDBLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SwitchCooldown = FMath::Max(0.f, SwitchCooldown - DeltaTime);
	SwitchAccumulator *= FMath::Exp(-8.f * DeltaTime);

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !LockTarget.IsValid())
	{
		if (LockTarget.IsStale())
		{
			SetLockTarget(nullptr);
		}
		return;
	}

	const AActor* Target = LockTarget.Get();
	const float Distance = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
	if (R::ShouldBreakLockOn(Distance, DBCombat::CanTarget(Pawn, Target), DBCombat::GetLockOnRules()))
	{
		ClearLockOn();
		return;
	}

	if (AController* Controller = Pawn->GetController())
	{
		const FRotator LookAt = (Target->GetActorLocation() - Pawn->GetPawnViewLocation()).Rotation();
		const FRotator Desired(LockedPitch + LookAt.Pitch * 0.5f, LookAt.Yaw, 0.f);
		Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaTime, CameraInterpSpeed));
	}
}
