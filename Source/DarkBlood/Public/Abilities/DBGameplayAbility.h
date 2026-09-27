// Base class for every DARK BLOOD gameplay ability (combat moves, class skills, movement abilities).
#pragma once

#include "Abilities/GameplayAbility.h"

#include "DBGameplayAbility.generated.h"

class ADBCharacterBase;
class UAnimMontage;
class UDBAbilitySystemComponent;

UENUM(BlueprintType)
enum class EDBAbilityActivationPolicy : uint8
{
	/** Activate once when the bound input is pressed. */
	OnInputTriggered,
	/** Keep trying to activate while the input is held (e.g. sprint, block). */
	WhileInputActive,
	/** Activate as soon as the ability is granted or the avatar is set (passives). */
	OnSpawn,
};

UCLASS(Abstract)
class DARKBLOOD_API UDBGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UDBGameplayAbility();

	EDBAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/** Stamina rules from the rules core: any positive stamina starts the action, the cost may drain it to zero. */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	/** Stamina spent when the ability is committed. */
	virtual float GetStaminaCost() const { return StaminaCost; }

	/** Spends stamina outside of CommitAbility (continuous drains, later combo steps). Server only. */
	void SpendStamina(float Amount) const;

	ADBCharacterBase* GetDBCharacter() const;
	UDBAbilitySystemComponent* GetDBAbilitySystem() const;

	/** Plays an optional montage (fire and forget). Timing never depends on it, so abilities work without animation assets. */
	void PlayOptionalMontage(UAnimMontage* Montage, float PlayRate = 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Activation")
	EDBAbilityActivationPolicy ActivationPolicy = EDBAbilityActivationPolicy::OnInputTriggered;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Cost", meta = (ClampMin = 0))
	float StaminaCost = 0.f;

	/** Stamina regeneration pauses this long after the ability spent stamina. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Cost", meta = (ClampMin = 0))
	float StaminaRegenDelaySeconds = 1.f;
};
