// Defensive and movement combat abilities: block/parry, dodge, sprint and hit reactions.
// All timing is data driven; montages are optional presentation.
#pragma once

#include "Abilities/DBGameplayAbility.h"

#include "DarkBloodRules/Combat.h"

#include "DBCombatAbilities.generated.h"

class UAnimMontage;

/**
 * Hold to block (70 % damage reduction, costs stamina per blocked point, see rules core).
 * The first ParryWindowSeconds after raising the guard are a perfect parry: no damage, the attacker
 * is staggered and the defender gets a counter window.
 */
UCLASS()
class DARKBLOOD_API UDBAbility_Block : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_Block();

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnReleased(float TimeHeld);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Block", meta = (ClampMin = 0, Units = "s"))
	float ParryWindowSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Presentation")
	TObjectPtr<UAnimMontage> GuardMontage = nullptr;
};

/** Dodge in the input direction (backstep without input) with invulnerability frames. */
UCLASS()
class DARKBLOOD_API UDBAbility_Dodge : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_Dodge();

	virtual float GetStaminaCost() const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UFUNCTION()
	void OnDodgeFinished();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Dodge", meta = (ClampMin = 0, Units = "cm"))
	float Distance = 450.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Dodge", meta = (ClampMin = 0.05, Units = "s"))
	float DurationSeconds = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Dodge", meta = (ClampMin = 0, Units = "s"))
	float InvulnerableSeconds = 0.3f;

	/** After the dodge, an attack within this window becomes a dash attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Dodge", meta = (ClampMin = 0, Units = "s"))
	float DashAttackWindowSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Presentation")
	TObjectPtr<UAnimMontage> DodgeMontage = nullptr;
};

/** Hold to sprint: faster movement, drains stamina while moving, pauses stamina regeneration. */
UCLASS()
class DARKBLOOD_API UDBAbility_Sprint : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_Sprint();

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnReleased(float TimeHeld);

	UFUNCTION()
	void OnDrainTick();

	void ScheduleDrain();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Sprint", meta = (ClampMin = 1))
	float SpeedMultiplier = 1.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Sprint", meta = (ClampMin = 0))
	float StaminaPerSecond = 12.f;

private:
	float BaseWalkSpeed = 0.f;
	static constexpr float DrainInterval = 0.25f;
};

/**
 * Hit reaction triggered by Event.Combat.HitReact (magnitude = DarkBlood::Rules::EHitReaction).
 * Interrupts attacks and blocks, pushes the victim away from the attacker and locks it for the
 * reaction time from the rules core.
 */
UCLASS()
class DARKBLOOD_API UDBAbility_HitReact : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_HitReact();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnReactionFinished();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Knockback", meta = (Units = "cm"))
	float StaggerKnockback = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Knockback", meta = (Units = "cm"))
	float KnockdownKnockback = 280.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Knockback", meta = (Units = "cm"))
	float ParriedKnockback = 60.f;

	/** Invulnerability while getting up after a knockdown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Knockback", meta = (ClampMin = 0, Units = "s"))
	float GetUpInvulnerableSeconds = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Presentation")
	TObjectPtr<UAnimMontage> StaggerMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Presentation")
	TObjectPtr<UAnimMontage> KnockdownMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Presentation")
	TObjectPtr<UAnimMontage> ParriedMontage = nullptr;

private:
	bool bAddedKnockdownTag = false;
};

/**
 * Passive unlock: while granted, the character can jump twice. Granted by a skill node or ability set
 * (the development moveset grants it so it can be tested).
 */
UCLASS()
class DARKBLOOD_API UDBAbility_DoubleJump : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBAbility_DoubleJump();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};
