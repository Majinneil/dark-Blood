// Base class for every DARK BLOOD gameplay ability (combat moves, class skills, movement abilities).
#pragma once

#include "Abilities/GameplayAbility.h"

#include "DBGameplayAbility.generated.h"

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

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Activation")
	EDBAbilityActivationPolicy ActivationPolicy = EDBAbilityActivationPolicy::OnInputTriggered;
};
