// Ability system component with input-tag based activation (Enhanced Input -> gameplay tag -> ability).
#pragma once

#include "AbilitySystemComponent.h"

#include "DBAbilitySystemComponent.generated.h"

UCLASS()
class DARKBLOOD_API UDBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UDBAbilitySystemComponent();

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/** Called once per frame by the owning player controller after input processing. */
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();

	/** Activates abilities whose policy is OnSpawn (passives). */
	void TryActivatePassiveAbilities();

private:
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
