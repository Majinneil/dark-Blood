#include "Visual/DBAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "Character/DBCharacterBase.h"
#include "Core/DBGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "Visual/DBCharacterVisualComponent.h"
#include "Visual/DBCharacterVisualDefinition.h"

namespace
{
	constexpr float CombatStanceHoldSeconds = 4.f;
	constexpr float BlinkDurationSeconds = 0.16f;
	constexpr float LookAtMaxAngleDegrees = 100.f;
}

void UDBAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ADBCharacterBase* Character = Cast<ADBCharacterBase>(TryGetPawnOwner());
	if (!Character)
	{
		return;
	}

	const FVector Velocity = Character->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	VerticalSpeed = Velocity.Z;
	Direction = GroundSpeed > 1.f ? UKismetAnimationLibrary::CalculateDirection(Velocity, Character->GetActorRotation()) : 0.f;
	if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		bIsAccelerating = Movement->GetCurrentAcceleration().SizeSquared() > 1.f;
		bIsInAir = Movement->IsFalling();
		bIsFlying = Movement->IsFlying();
	}

	if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		bIsSprinting = ASC->HasMatchingGameplayTag(DBTags::State_Sprinting);
		bIsBlocking = ASC->HasMatchingGameplayTag(DBTags::State_Blocking);
		bIsAttacking = ASC->HasMatchingGameplayTag(DBTags::State_Attacking);
		bIsDodging = ASC->HasMatchingGameplayTag(DBTags::State_Dodging);
		bIsStaggered = ASC->HasMatchingGameplayTag(DBTags::State_Staggered);
		bIsKnockedDown = ASC->HasMatchingGameplayTag(DBTags::State_KnockedDown);
	}
	bIsDead = Character->IsDead();

	const AActor* Focus = Character->GetCombatFocusTarget();
	bIsLockedOn = Focus != nullptr;
	CombatStanceSeconds = (bIsAttacking || bIsBlocking || bIsLockedOn || bIsStaggered) ? CombatStanceHoldSeconds
		: FMath::Max(0.f, CombatStanceSeconds - DeltaSeconds);
	bInCombatStance = CombatStanceSeconds > 0.f;

	// Look-at: explicit target (dialogue partner) first, otherwise the combat focus.
	const UDBCharacterVisualComponent* Visuals = Character->GetVisuals();
	const AActor* LookTarget = Visuals && Visuals->GetLookAtTarget() ? Visuals->GetLookAtTarget() : Focus;
	const bool bLookEnabled = !Visuals || !Visuals->GetActiveProfile() || Visuals->GetActiveProfile()->bLookAtTargets;
	float TargetAlpha = 0.f;
	if (LookTarget && bLookEnabled && !bIsDead)
	{
		LookAtLocation = LookTarget->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		const FVector ToTarget = (LookAtLocation - Character->GetActorLocation()).GetSafeNormal2D();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ToTarget, Character->GetActorForwardVector().GetSafeNormal2D())));
		TargetAlpha = Angle < LookAtMaxAngleDegrees ? 1.f : 0.f;
	}
	LookAtAlpha = FMath::FInterpTo(LookAtAlpha, TargetAlpha, DeltaSeconds, 4.f);
	Emotion = Visuals ? Visuals->GetEmotion() : FName(TEXT("Neutral"));

	if (!Visuals || !Visuals->GetActiveProfile() || Visuals->GetActiveProfile()->bProceduralBlink)
	{
		UpdateBlink(DeltaSeconds);
	}
}

void UDBAnimInstance::UpdateBlink(float DeltaSeconds)
{
	if (bIsDead)
	{
		BlinkAlpha = 1.f;
		return;
	}
	if (BlinkPhase >= 0.f)
	{
		BlinkPhase += DeltaSeconds;
		const float T = BlinkPhase / BlinkDurationSeconds;
		BlinkAlpha = T < 0.5f ? T * 2.f : FMath::Max(0.f, 2.f - T * 2.f);
		if (T >= 1.f)
		{
			BlinkPhase = -1.f;
			BlinkAlpha = 0.f;
			BlinkTimer = FMath::FRandRange(2.f, 6.f);
		}
		return;
	}
	BlinkTimer -= DeltaSeconds;
	if (BlinkTimer <= 0.f)
	{
		BlinkPhase = 0.f;
	}
}
