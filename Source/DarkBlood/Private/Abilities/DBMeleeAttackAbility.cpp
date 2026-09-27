#include "Abilities/DBMeleeAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/DBCharacterBase.h"
#include "Combat/DBCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

namespace R = DarkBlood::Rules;

R::FAttackStep FDBAttackStepConfig::ToRules() const
{
	R::FAttackStep Step;
	Step.BaseDamage = BaseDamage;
	Step.StaminaCost = StaminaCost;
	Step.PoiseDamage = PoiseDamage;
	Step.WindupSeconds = WindupSeconds;
	Step.ActiveSeconds = ActiveSeconds;
	Step.RecoverySeconds = RecoverySeconds;
	Step.ComboWindowSeconds = ComboWindowSeconds;
	Step.Range = Range;
	Step.HalfAngleDegrees = HalfAngleDegrees;
	Step.bKnockdown = bKnockdown;
	return Step;
}

UDBMeleeAttackAbility::UDBMeleeAttackAbility()
{
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Attack));
	ActivationOwnedTags.AddTag(DBTags::State_Attacking);
	// One attack at a time; attacking lowers the guard (block -> counter) and stops a sprint.
	BlockAbilitiesWithTag.AddTag(DBTags::Ability_Attack);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Block);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	ActivationBlockedTags.AddTag(DBTags::State_Dodging);
	DamageType = DBTags::Damage_Type_Physical;
}

const FDBAttackStepConfig* UDBMeleeAttackAbility::GetCurrentStep() const
{
	switch (Context)
	{
	case EDBAttackContext::Air: return &AirAttack;
	case EDBAttackContext::Sprint: return &SprintAttack;
	case EDBAttackContext::Dash: return &DashAttack;
	case EDBAttackContext::Combo: break;
	}
	return Steps.IsValidIndex(CurrentStep) ? &Steps[CurrentStep] : nullptr;
}

void UDBMeleeAttackAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	bWasSprinting = ASC && ASC->HasMatchingGameplayTag(DBTags::State_Sprinting);
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
}

EDBAttackContext UDBMeleeAttackAbility::ChooseContext() const
{
	const ADBCharacterBase* Self = GetDBCharacter();
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (bHasAirAttack && Self && Self->GetCharacterMovement()->IsFalling())
	{
		return EDBAttackContext::Air;
	}
	if (bHasSprintAttack && bWasSprinting)
	{
		return EDBAttackContext::Sprint;
	}
	if (bHasDashAttack && ASC && ASC->HasMatchingGameplayTag(DBTags::State_DodgeRecovery))
	{
		return EDBAttackContext::Dash;
	}
	return EDBAttackContext::Combo;
}

void UDBMeleeAttackAbility::ApplyContextMovement(const FDBAttackStepConfig& Step)
{
	const ADBCharacterBase* Self = GetDBCharacter();
	if (!Self || Context == EDBAttackContext::Combo || Step.WindupSeconds <= 0.f)
	{
		return;
	}
	FVector Direction = Self->GetActorForwardVector();
	float Speed = 0.f;
	switch (Context)
	{
	case EDBAttackContext::Air:
		Direction = FVector::DownVector;
		Speed = AirPlungeSpeed;
		break;
	case EDBAttackContext::Sprint:
		Speed = SprintLungeDistance / Step.WindupSeconds;
		break;
	case EDBAttackContext::Dash:
		Speed = DashLungeDistance / Step.WindupSeconds;
		break;
	case EDBAttackContext::Combo:
		break;
	}
	UAbilityTask_ApplyRootMotionConstantForce* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this,
		TEXT("AttackMovement"), Direction, Speed, Step.WindupSeconds, false, nullptr, ERootMotionFinishVelocityMode::ClampVelocity,
		FVector::ZeroVector, 100.f, Context != EDBAttackContext::Air);
	if (Task)
	{
		Task->ReadyForActivation();
	}
}

float UDBMeleeAttackAbility::GetStaminaCost() const
{
	const FDBAttackStepConfig* Step = GetCurrentStep();
	return Step ? Step->StaminaCost : 0.f;
}

void UDBMeleeAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (Steps.IsEmpty() || !GetWorld())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Situational attacks take precedence; otherwise continue the chain if this press came inside
	// the previous hit's combo window.
	Context = ChooseContext();
	const double Now = GetWorld()->GetTimeSeconds();
	CurrentStep = Context == EDBAttackContext::Combo && Steps.IsValidIndex(LastHitStep)
		? R::NextComboStep(LastHitStep, Steps.Num(), static_cast<float>(Now - LastHitWorldTime), Steps[LastHitStep].ComboWindowSeconds)
		: 0;
	bNextStepQueued = false;
	bSwingStarted = false;
	Charge = R::FChargeResult();

	// Commit after choosing the step: the stamina cost depends on it.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	FaceTarget();

	if (!bChargeable || Context != EDBAttackContext::Combo)
	{
		BeginSwing();
		return;
	}

	// Only the owning machine knows whether the button is still down; the server waits for the
	// replicated release event and measures the hold time itself.
	if (UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, IsLocallyControlled()))
	{
		Release->OnRelease.AddDynamic(this, &UDBMeleeAttackAbility::OnChargeReleased);
		Release->ReadyForActivation();
	}
	// Holding past the full charge releases automatically.
	if (UAbilityTask_WaitDelay* MaxHold = UAbilityTask_WaitDelay::WaitDelay(this, FullChargeSeconds + 0.5f))
	{
		MaxHold->OnFinish.AddDynamic(this, &UDBMeleeAttackAbility::OnMaxChargeReached);
		MaxHold->ReadyForActivation();
	}
}

R::FChargeRules UDBMeleeAttackAbility::GetChargeRules() const
{
	R::FChargeRules Rules;
	Rules.MinChargeSeconds = MinChargeSeconds;
	Rules.FullChargeSeconds = FullChargeSeconds;
	Rules.MaxDamageMultiplier = MaxChargeDamageMultiplier;
	return Rules;
}

void UDBMeleeAttackAbility::OnChargeReleased(float TimeHeld)
{
	if (!bSwingStarted)
	{
		Charge = R::EvaluateCharge(TimeHeld, GetChargeRules());
		BeginSwing();
	}
}

void UDBMeleeAttackAbility::OnMaxChargeReached()
{
	OnChargeReleased(FullChargeSeconds);
}

void UDBMeleeAttackAbility::BeginSwing()
{
	if (bSwingStarted || !IsActive())
	{
		return;
	}
	bSwingStarted = true;

	const FDBAttackStepConfig* Step = GetCurrentStep();
	PlayOptionalMontage(Step->Montage);
	ApplyContextMovement(*Step);
	if (UAbilityTask_WaitDelay* Windup = UAbilityTask_WaitDelay::WaitDelay(this, Step->WindupSeconds))
	{
		Windup->OnFinish.AddDynamic(this, &UDBMeleeAttackAbility::OnWindupFinished);
		Windup->ReadyForActivation();
	}
}

void UDBMeleeAttackAbility::OnWindupFinished()
{
	const FDBAttackStepConfig* Step = GetCurrentStep();
	// Situational attacks do not feed the combo chain.
	LastHitStep = Context == EDBAttackContext::Combo ? CurrentStep : INDEX_NONE;
	LastHitWorldTime = GetWorld()->GetTimeSeconds();
	if (HasAuthority(&CurrentActivationInfo))
	{
		PerformHit();
	}
	if (UAbilityTask_WaitDelay* Recovery = UAbilityTask_WaitDelay::WaitDelay(this, Step->ActiveSeconds + Step->RecoverySeconds))
	{
		Recovery->OnFinish.AddDynamic(this, &UDBMeleeAttackAbility::OnStepFinished);
		Recovery->ReadyForActivation();
	}
}

void UDBMeleeAttackAbility::OnStepFinished()
{
	// A press during the swing continues the combo: end this step and re-activate on the owning
	// machine (a new predicted activation, so the server runs the next step as well).
	const bool bContinue = bNextStepQueued && IsLocallyControlled();
	const FGameplayAbilitySpecHandle Handle = CurrentSpecHandle;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

	if (bContinue && ASC && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(ASC, [ASC, Handle]()
		{
			ASC->TryActivateAbility(Handle);
		}));
	}
}

void UDBMeleeAttackAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	if (bSwingStarted)
	{
		bNextStepQueued = true;
	}
}

void UDBMeleeAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	bSwingStarted = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UDBMeleeAttackAbility::FaceTarget() const
{
	ADBCharacterBase* Self = GetDBCharacter();
	if (!Self)
	{
		return;
	}
	FVector Direction = FVector::ZeroVector;
	if (const AActor* Target = Self->GetCombatFocusTarget())
	{
		Direction = Target->GetActorLocation() - Self->GetActorLocation();
	}
	else
	{
		Direction = Self->GetLastMovementInputVector();
	}
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		Self->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}
}

void UDBMeleeAttackAbility::PerformHit()
{
	ADBCharacterBase* Self = GetDBCharacter();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	const FDBAttackStepConfig* Step = GetCurrentStep();
	UWorld* World = GetWorld();
	if (!Self || !SourceASC || !Step || !World)
	{
		return;
	}

	FDBHitParams Hit;
	Hit.BaseDamage = Step->BaseDamage * Charge.DamageMultiplier;
	Hit.PoiseDamage = Step->PoiseDamage * Charge.PoiseMultiplier;
	Hit.DamageType = DamageType;
	Hit.bKnockdown = Step->bKnockdown || (bChargeable && bFullChargeKnocksDown && Charge.ChargeFraction >= 1.f);
	Hit.AttackerLevel = Self->GetCombatLevel();

	const bool bCounter = SourceASC->HasMatchingGameplayTag(DBTags::State_CounterWindow);
	if (bCounter)
	{
		Hit.BaseDamage *= CounterDamageMultiplier;
		Hit.PoiseDamage *= CounterPoiseMultiplier;
		SourceASC->SetLooseGameplayTagCount(DBTags::State_CounterWindow, 0);
	}

	UE_LOG(LogDBCombat, Log, TEXT("%s: %s %s step %d/%d%s%s"), *DBCombat::GetCombatName(Self), *GetClass()->GetName(),
		*StaticEnum<EDBAttackContext>()->GetNameStringByValue(static_cast<int64>(Context)), CurrentStep + 1, Steps.Num(), Charge.bCharged ? *FString::Printf(TEXT(" charged %.0f%%"), Charge.ChargeFraction * 100.f) : TEXT(""),
		bCounter ? TEXT(" COUNTER") : TEXT(""));

	const FVector Origin = Self->GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DBMeleeHit), false, Self);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Step->Range + 100.f), Params);

	TSet<const AActor*> AlreadyHit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ADBCharacterBase* Target = Cast<ADBCharacterBase>(Overlap.GetActor());
		if (!Target || AlreadyHit.Contains(Target) || !DBCombat::CanTarget(Self, Target))
		{
			continue;
		}
		const FVector Local = Self->GetActorTransform().InverseTransformPositionNoScale(Target->GetActorLocation());
		const float TargetRadius = Target->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (FMath::Abs(Local.Z) > 200.f || !R::IsInsideAttackArc(Local.X, Local.Y, Step->Range, Step->HalfAngleDegrees, TargetRadius))
		{
			continue;
		}
		AlreadyHit.Add(Target);
		DBCombat::ApplyHit(SourceASC, Target->GetAbilitySystemComponent(), Hit);
	}
	if (AlreadyHit.IsEmpty())
	{
		UE_LOG(LogDBCombat, Log, TEXT("%s: swing missed"), *DBCombat::GetCombatName(Self));
	}
}

// ---- Development movesets ----------------------------------------------------------------------

namespace
{
	FDBAttackStepConfig MakeStep(float Damage, float Stamina, float Poise, float Windup, float Active, float Recovery, float Range = 200.f,
		float HalfAngle = 60.f, bool bKnockdown = false)
	{
		FDBAttackStepConfig Step;
		Step.BaseDamage = Damage;
		Step.StaminaCost = Stamina;
		Step.PoiseDamage = Poise;
		Step.WindupSeconds = Windup;
		Step.ActiveSeconds = Active;
		Step.RecoverySeconds = Recovery;
		Step.Range = Range;
		Step.HalfAngleDegrees = HalfAngle;
		Step.bKnockdown = bKnockdown;
		return Step;
	}
}

UDBAbility_LightCombo::UDBAbility_LightCombo()
{
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{DBTags::Ability_Attack, DBTags::Ability_Attack_Light}));
	Steps = {
		MakeStep(10.f, 8.f, 12.f, 0.18f, 0.12f, 0.30f),
		MakeStep(12.f, 9.f, 14.f, 0.20f, 0.12f, 0.32f),
		MakeStep(18.f, 14.f, 26.f, 0.28f, 0.15f, 0.45f, 220.f, 75.f),
	};

	// Plunge from the air: hits everything around the landing spot and knocks down.
	bHasAirAttack = true;
	AirAttack = MakeStep(20.f, 12.f, 30.f, 0.3f, 0.1f, 0.45f, 230.f, 180.f, true);
	// Lunge out of a sprint.
	bHasSprintAttack = true;
	SprintAttack = MakeStep(22.f, 14.f, 28.f, 0.25f, 0.12f, 0.5f, 230.f, 50.f);
	// Quick thrust right after a dodge.
	bHasDashAttack = true;
	DashAttack = MakeStep(16.f, 10.f, 18.f, 0.12f, 0.1f, 0.35f, 220.f, 45.f);
}

UDBAbility_HeavyAttack::UDBAbility_HeavyAttack()
{
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{DBTags::Ability_Attack, DBTags::Ability_Attack_Heavy}));
	Steps = {MakeStep(28.f, 22.f, 30.f, 0.45f, 0.15f, 0.60f, 230.f, 50.f)};
	bChargeable = true;
}

UDBAbility_EnemySwing::UDBAbility_EnemySwing()
{
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Attack));
	// Long, readable windup: 0.8 s to react with block, parry or dodge.
	Steps = {MakeStep(15.f, 0.f, 20.f, 0.8f, 0.15f, 0.8f, 230.f, 70.f)};
}

UDBAbility_DemonClaw::UDBAbility_DemonClaw()
{
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Attack));
	DamageType = DBTags::Damage_Type_Physical;
	Steps = {MakeStep(18.f, 0.f, 22.f, 0.5f, 0.15f, 0.6f, 200.f, 60.f)};
}
