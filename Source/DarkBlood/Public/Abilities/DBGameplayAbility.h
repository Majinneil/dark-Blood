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

	/** Cooldowns are timed loose tags (predicted on the owning client, authoritative on the server). */
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	const FText& GetDisplayName() const { return DisplayName; }
	const FGameplayTag& GetCooldownTag() const { return CooldownTag; }

protected:
	/** Stamina spent when the ability is committed. */
	virtual float GetStaminaCost() const { return StaminaCost; }
	virtual float GetManaCost() const { return ManaCost; }
	virtual float GetCooldownSeconds() const { return CooldownSeconds; }

	/** Server only: mana change (negative spends). */
	void ChangeMana(float Delta) const;
	/** Server only: restores stamina. */
	void RestoreStamina(float Amount) const;

	/** Rank of a skill tree node of the owning player (0 for NPCs or not learned). */
	int32 GetSkillRank(FName NodeId) const;

	/** Spends stamina outside of CommitAbility (continuous drains, later combo steps). Server only. */
	void SpendStamina(float Amount) const;

	ADBCharacterBase* GetDBCharacter() const;
	UDBAbilitySystemComponent* GetDBAbilitySystem() const;

	/** Plays an optional montage (fire and forget). Timing never depends on it, so abilities work without animation assets. */
	void PlayOptionalMontage(UAnimMontage* Montage, float PlayRate = 1.f);

	/**
	 * Presentation montage: Override if set, otherwise the avatar's animation set entry for Key (Anim.*).
	 * DesiredSeconds > 0 fits the montage length to the gameplay duration of the action.
	 */
	void PlayPresentationMontage(UAnimMontage* Override, const FGameplayTag& Key, int32 Variant = 0, float DesiredSeconds = 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Activation")
	EDBAbilityActivationPolicy ActivationPolicy = EDBAbilityActivationPolicy::OnInputTriggered;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Cost", meta = (ClampMin = 0))
	float StaminaCost = 0.f;

	/** Mana spent on commit. Unlike stamina, the full amount must be available. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Cost", meta = (ClampMin = 0))
	float ManaCost = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Cooldown", meta = (ClampMin = 0, Units = "s"))
	float CooldownSeconds = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Cooldown", meta = (Categories = "Cooldown"))
	FGameplayTag CooldownTag;

	/** Name shown in the HUD ability bar and the skill tree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|UI")
	FText DisplayName;

	/** Stamina regeneration pauses this long after the ability spent stamina. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Cost", meta = (ClampMin = 0))
	float StaminaRegenDelaySeconds = 1.f;
};
