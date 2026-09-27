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

	/**
	 * Adds a loose (non-replicated) tag for Seconds; adding it again restarts the timer.
	 * Called by predicted abilities on client and server alike, so both see the same windows
	 * (parry window, i-frames, regeneration delays). The server's copy is the one damage uses.
	 */
	void AddTimedLooseTag(const FGameplayTag& Tag, float Seconds);

	/** Seconds left on a timed loose tag (0 if not active). Used for cooldown displays. */
	float GetTimedTagRemaining(const FGameplayTag& Tag) const;

	/**
	 * Sends a gameplay event on the next tick. Use from inside effect execution, where triggering
	 * abilities (which may cancel the ability that is applying the effect) is not safe.
	 */
	void SendGameplayEventDeferred(const FGameplayTag& EventTag, const FGameplayEventData& Payload);

private:
	TMap<FGameplayTag, FTimerHandle> TimedTagTimers;

	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;
};
