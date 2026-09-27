// Melee attacks: light combo chains, heavy/charged swings and enemy attacks.
// Timing comes from data (windup -> hit -> recovery), not from animations, so attacks work with the
// placeholder body. Montages are optional presentation. Hits are traced and applied on the server only.
#pragma once

#include "Abilities/DBGameplayAbility.h"
#include "GameplayTagContainer.h"

#include "DarkBloodRules/Combat.h"

#include "DBMeleeAttackAbility.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FDBAttackStepConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0))
	float BaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0))
	float StaminaCost = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = 0))
	float PoiseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0, Units = "s"))
	float WindupSeconds = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0, Units = "s"))
	float ActiveSeconds = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0, Units = "s"))
	float RecoverySeconds = 0.35f;

	/** Time after the hit in which the next press continues the combo. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = 0, Units = "s"))
	float ComboWindowSeconds = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reach", meta = (ClampMin = 0, Units = "cm"))
	float Range = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reach", meta = (ClampMin = 0, ClampMax = 180, Units = "deg"))
	float HalfAngleDegrees = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	bool bKnockdown = false;

	/** Optional presentation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	DarkBlood::Rules::FAttackStep ToRules() const;
};

/** Which move an activation performs: the combo chain or a situational attack. */
UENUM(BlueprintType)
enum class EDBAttackContext : uint8
{
	Combo,
	/** In the air: plunge down, hits all around on landing. */
	Air,
	/** Out of a sprint: lunge forward. */
	Sprint,
	/** Right after a dodge: quick dash thrust. */
	Dash,
};

UCLASS(Abstract)
class DARKBLOOD_API UDBMeleeAttackAbility : public UDBGameplayAbility
{
	GENERATED_BODY()

public:
	UDBMeleeAttackAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	virtual float GetStaminaCost() const override;
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Attack", meta = (TitleProperty = "BaseDamage"))
	TArray<FDBAttackStepConfig> Steps;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Attack", meta = (Categories = "Damage.Type"))
	FGameplayTag DamageType;

	/** Hold the input to charge; release to swing (single-step moves only use step 0). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Charge")
	bool bChargeable = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Charge", meta = (EditCondition = "bChargeable"))
	float MinChargeSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Charge", meta = (EditCondition = "bChargeable"))
	float FullChargeSeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Charge", meta = (EditCondition = "bChargeable"))
	float MaxChargeDamageMultiplier = 2.5f;

	/** A fully charged swing knocks down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Charge", meta = (EditCondition = "bChargeable"))
	bool bFullChargeKnocksDown = true;

	/** Applied to the first hit inside the counter window after a perfect parry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Counter")
	float CounterDamageMultiplier = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Counter")
	float CounterPoiseMultiplier = 3.f;

	// ---- Situational attacks (replace the combo step when their condition holds) ----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context")
	bool bHasAirAttack = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context", meta = (EditCondition = "bHasAirAttack"))
	FDBAttackStepConfig AirAttack;

	/** Downward speed of the plunge during the air attack windup. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context", meta = (EditCondition = "bHasAirAttack", Units = "cm/s"))
	float AirPlungeSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context")
	bool bHasSprintAttack = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context", meta = (EditCondition = "bHasSprintAttack"))
	FDBAttackStepConfig SprintAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context", meta = (EditCondition = "bHasSprintAttack", Units = "cm"))
	float SprintLungeDistance = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context")
	bool bHasDashAttack = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context", meta = (EditCondition = "bHasDashAttack"))
	FDBAttackStepConfig DashAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Context", meta = (EditCondition = "bHasDashAttack", Units = "cm"))
	float DashLungeDistance = 200.f;

private:
	UFUNCTION()
	void OnChargeReleased(float TimeHeld);

	UFUNCTION()
	void OnMaxChargeReached();

	UFUNCTION()
	void OnWindupFinished();

	UFUNCTION()
	void OnStepFinished();

	void BeginSwing();
	void PerformHit();
	void FaceTarget() const;

	const FDBAttackStepConfig* GetCurrentStep() const;
	EDBAttackContext ChooseContext() const;
	void ApplyContextMovement(const FDBAttackStepConfig& Step);
	DarkBlood::Rules::FChargeRules GetChargeRules() const;

	/** Combo step of the current (or next) activation. Instances persist per actor, so this spans activations. */
	int32 CurrentStep = 0;
	int32 LastHitStep = INDEX_NONE;
	double LastHitWorldTime = -1000.0;
	bool bNextStepQueued = false;
	bool bSwingStarted = false;
	/** Sampled in PreActivate: activating an attack cancels the sprint before ActivateAbility runs. */
	bool bWasSprinting = false;
	EDBAttackContext Context = EDBAttackContext::Combo;
	DarkBlood::Rules::FChargeResult Charge;
};

/** DEVELOPMENT moveset: three-hit light combo. */
UCLASS()
class DARKBLOOD_API UDBAbility_LightCombo : public UDBMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UDBAbility_LightCombo();
};

/** DEVELOPMENT moveset: heavy swing, hold to charge. */
UCLASS()
class DARKBLOOD_API UDBAbility_HeavyAttack : public UDBMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UDBAbility_HeavyAttack();
};

/** DEVELOPMENT enemy attack: lesser demon claw strike (shorter windup than the dummy). */
UCLASS()
class DARKBLOOD_API UDBAbility_DemonClaw : public UDBMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UDBAbility_DemonClaw();
};

/** Telegraphed enemy swing (long windup; used by training dummies to practise block, parry and dodge). */
UCLASS()
class DARKBLOOD_API UDBAbility_EnemySwing : public UDBMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UDBAbility_EnemySwing();
};

/** Schattenlaeufer moveset: four quick kunai cuts. */
UCLASS()
class DARKBLOOD_API UDBAbility_KunaiCombo : public UDBMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UDBAbility_KunaiCombo();
};

/** Moench moveset: five fast strikes, the last one a spinning kick. */
UCLASS()
class DARKBLOOD_API UDBAbility_FistCombo : public UDBMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UDBAbility_FistCombo();
};

/** Moench heavy: palm strike with strong poise damage (chargeable). */
UCLASS()
class DARKBLOOD_API UDBAbility_PalmStrike : public UDBMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UDBAbility_PalmStrike();
};
