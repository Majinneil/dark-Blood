#include "Abilities/DBCombatAbilities.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBClassAbilities.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/DBCharacterBase.h"
#include "Combat/DBCombatStatics.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

namespace R = DarkBlood::Rules;

namespace
{
	bool HasStamina(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		return ASC && ASC->GetNumericAttribute(UDBAttributeSet::GetStaminaAttribute()) > 0.f;
	}

	void ApplyPush(UGameplayAbility* Ability, const FName TaskName, const FVector& Direction, float Distance, float Seconds)
	{
		if (Distance <= 0.f || Seconds <= 0.f || Direction.IsNearlyZero())
		{
			return;
		}
		UAbilityTask_ApplyRootMotionConstantForce* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(Ability,
			TaskName, Direction.GetSafeNormal2D(), Distance / Seconds, Seconds, false, nullptr, ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector, 100.f, true);
		if (Task)
		{
			Task->ReadyForActivation();
		}
	}
}

// ---- Block / parry -------------------------------------------------------------------------------

UDBAbility_Block::UDBAbility_Block()
{
	DisplayName = FText::FromString(TEXT("Block"));
	ActivationPolicy = EDBAbilityActivationPolicy::WhileInputActive;
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Block));
	ActivationOwnedTags.AddTag(DBTags::State_Blocking);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	ActivationBlockedTags.AddTag(DBTags::State_Attacking);
	ActivationBlockedTags.AddTag(DBTags::State_Dodging);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
}

bool UDBAbility_Block::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// Raising the guard is free, but not with an empty stamina bar.
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && HasStamina(ActorInfo);
}

void UDBAbility_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystem())
	{
		// Moench "Innere Ruhe": +0.1 s perfect-parry window per rank.
		ASC->AddTimedLooseTag(DBTags::State_ParryWindow, ParryWindowSeconds + 0.1f * static_cast<float>(GetSkillRank(DBSkillNodes::InnerCalm)));
	}
	PlayPresentationMontage(GuardMontage, DBTags::Anim_Block);

	if (UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, IsLocallyControlled()))
	{
		Release->OnRelease.AddDynamic(this, &UDBAbility_Block::OnReleased);
		Release->ReadyForActivation();
	}
}

void UDBAbility_Block::OnReleased(float /*TimeHeld*/)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ---- Dodge ---------------------------------------------------------------------------------------

UDBAbility_Dodge::UDBAbility_Dodge()
{
	DisplayName = FText::FromString(TEXT("Ausweichen"));
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Dodge));
	ActivationOwnedTags.AddTag(DBTags::State_Dodging);
	// Dodging cancels attacks and blocks (responsive action combat).
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Attack);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Block);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	ActivationBlockedTags.AddTag(DBTags::State_Dodging);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	StaminaCost = 20.f;
}

void UDBAbility_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Character = GetDBCharacter();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The movement component's acceleration carries the player's input on the owning client and,
	// through the replicated moves, on the server - so both pick the same direction.
	FVector Direction = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = -Character->GetActorForwardVector(); // backstep
	}

	if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystem())
	{
		ASC->AddTimedLooseTag(DBTags::State_Invulnerable, InvulnerableSeconds);
	}
	// Presentation: directional roll (8 variants clockwise from forward; sets with fewer variants wrap around).
	const FVector Local = Character->GetActorTransform().InverseTransformVectorNoScale(Direction);
	const float Degrees = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	const int32 Octant = (FMath::RoundToInt(Degrees / 45.f) + 8) % 8;
	PlayPresentationMontage(DodgeMontage, DBTags::Anim_Dodge, Octant, DurationSeconds);
	ApplyPush(this, TEXT("Dodge"), Direction, Distance, DurationSeconds);
	UE_LOG(LogDBCombat, Log, TEXT("%s dodges"), *DBCombat::GetCombatName(Character));
	if (HasAuthority(&ActivationInfo))
	{
		GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(DBTags::GameplayCue_Combat_Dodge);
	}

	if (UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, DurationSeconds))
	{
		Wait->OnFinish.AddDynamic(this, &UDBAbility_Dodge::OnDodgeFinished);
		Wait->ReadyForActivation();
	}
}

float UDBAbility_Dodge::GetStaminaCost() const
{
	// Schattenlaeufer "Leichtfuessig": dodging costs half.
	return GetSkillRank(DBSkillNodes::LightFooted) > 0 ? StaminaCost * 0.5f : StaminaCost;
}

void UDBAbility_Dodge::OnDodgeFinished()
{
	// A short window after the dodge turns the next attack into a dash attack.
	if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystem())
	{
		ASC->AddTimedLooseTag(DBTags::State_DodgeRecovery, DashAttackWindowSeconds);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ---- Sprint --------------------------------------------------------------------------------------

UDBAbility_Sprint::UDBAbility_Sprint()
{
	ActivationPolicy = EDBAbilityActivationPolicy::WhileInputActive;
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Sprint));
	ActivationOwnedTags.AddTag(DBTags::State_Sprinting);
	ActivationBlockedTags.AddTag(DBTags::State_Attacking);
	ActivationBlockedTags.AddTag(DBTags::State_Blocking);
	ActivationBlockedTags.AddTag(DBTags::State_Dodging);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	StaminaRegenDelaySeconds = 0.75f;
}

bool UDBAbility_Sprint::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && HasStamina(ActorInfo);
}

void UDBAbility_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Character = GetDBCharacter();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	BaseWalkSpeed = Movement->MaxWalkSpeed;
	Movement->MaxWalkSpeed = BaseWalkSpeed * SpeedMultiplier;

	if (UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, IsLocallyControlled()))
	{
		Release->OnRelease.AddDynamic(this, &UDBAbility_Sprint::OnReleased);
		Release->ReadyForActivation();
	}
	ScheduleDrain();
}

void UDBAbility_Sprint::ScheduleDrain()
{
	if (UAbilityTask_WaitDelay* Tick = UAbilityTask_WaitDelay::WaitDelay(this, DrainInterval))
	{
		Tick->OnFinish.AddDynamic(this, &UDBAbility_Sprint::OnDrainTick);
		Tick->ReadyForActivation();
	}
}

void UDBAbility_Sprint::OnDrainTick()
{
	const ADBCharacterBase* Character = GetDBCharacter();
	if (!Character)
	{
		return;
	}
	if (Character->GetVelocity().Size2D() > 50.f)
	{
		SpendStamina(StaminaPerSecond * DrainInterval); // server only
	}
	if (!HasStamina(GetCurrentActorInfo()))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	ScheduleDrain();
}

void UDBAbility_Sprint::OnReleased(float /*TimeHeld*/)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDBAbility_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ADBCharacterBase* Character = GetDBCharacter(); Character && BaseWalkSpeed > 0.f)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
		BaseWalkSpeed = 0.f;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---- Hit reaction --------------------------------------------------------------------------------

UDBAbility_HitReact::UDBAbility_HitReact()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	bRetriggerInstancedAbility = true;
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_HitReact));
	ActivationOwnedTags.AddTag(DBTags::State_Staggered);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Attack);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Block);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = DBTags::Event_Combat_HitReact;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UDBAbility_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Character = GetDBCharacter();
	const R::EHitReaction Reaction =
		TriggerEventData ? static_cast<R::EHitReaction>(FMath::RoundToInt(TriggerEventData->EventMagnitude)) : R::EHitReaction::Stagger;
	const float Seconds = R::ReactionDurationSeconds(Reaction, DBCombat::GetPoiseRules());
	if (!Character || Seconds <= 0.f || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	float Knockback = StaggerKnockback;
	UAnimMontage* Montage = StaggerMontage;
	FGameplayTag AnimKey = DBTags::Anim_HitReact;
	if (Reaction == R::EHitReaction::Knockdown)
	{
		Knockback = KnockdownKnockback;
		Montage = KnockdownMontage;
		AnimKey = DBTags::Anim_Knockdown;
		GetAbilitySystemComponentFromActorInfo()->AddLooseGameplayTag(DBTags::State_KnockedDown);
		bAddedKnockdownTag = true;
	}
	else if (Reaction == R::EHitReaction::ParriedStagger)
	{
		Knockback = ParriedKnockback;
		Montage = ParriedMontage;
		AnimKey = DBTags::Anim_HitReact_Parried;
	}

	const AActor* Source = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
	const FVector Away = Source ? Character->GetActorLocation() - Source->GetActorLocation() : -Character->GetActorForwardVector();
	ApplyPush(this, TEXT("Knockback"), Away, Knockback * Character->GetKnockbackScale(), FMath::Min(0.25f, Seconds));
	PlayPresentationMontage(Montage, AnimKey, 0, Seconds);
	UE_LOG(LogDBCombat, Log, TEXT("%s reacts: %hs (%.1f s)"), *DBCombat::GetCombatName(Character), R::ToString(Reaction), Seconds);
	if (HasAuthority(&ActivationInfo))
	{
		GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(DBTags::GameplayCue_Combat_Stagger);
	}

	if (UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Seconds))
	{
		Wait->OnFinish.AddDynamic(this, &UDBAbility_HitReact::OnReactionFinished);
		Wait->ReadyForActivation();
	}
}

void UDBAbility_HitReact::OnReactionFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDBAbility_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bAddedKnockdownTag)
	{
		if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystem())
		{
			ASC->RemoveLooseGameplayTag(DBTags::State_KnockedDown);
			// Getting up is briefly invulnerable, so a knockdown cannot be chained forever.
			if (!bWasCancelled && GetUpInvulnerableSeconds > 0.f)
			{
				ASC->AddTimedLooseTag(DBTags::State_Invulnerable, GetUpInvulnerableSeconds);
			}
		}
		bAddedKnockdownTag = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---- Double jump (passive unlock) ----------------------------------------------------------------

UDBAbility_DoubleJump::UDBAbility_DoubleJump()
{
	DisplayName = FText::FromString(TEXT("Doppelsprung"));
	ActivationPolicy = EDBAbilityActivationPolicy::OnSpawn;
	// The server owns unlocks. A server-triggered LocalPredicted activation fails for remote clients whose
	// ability list has not replicated yet, so activate on the server only and replicate the tag instead.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	// Passive: stays active (also while dead) so the unlock survives death and respawn.
	ActivationBlockedTags.RemoveTag(DBTags::State_Dead);
}

void UDBAbility_DoubleJump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Intentionally never ends; the replicated tag raises the jump count on server and owning client
	// (character movement predicts jumps on the client, so it must know about the unlock too).
	CommitAbility(Handle, ActorInfo, ActivationInfo);
	ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(DBTags::Movement_DoubleJump, 1, EGameplayTagReplicationState::TagOnly);
}

void UDBAbility_DoubleJump::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(DBTags::Movement_DoubleJump, 1, EGameplayTagReplicationState::TagOnly);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
