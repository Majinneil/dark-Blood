// Light survival on the PlayerState (Phase 8, server authoritative; values replicated to the owner): satiety falls with
// game time and is refilled by food, warmth falls in cold regions, on high ground, at night and when wet and returns in
// settlements. Both only scale stamina / health regeneration (DarkBloodRules/Survival.h) - no damage. Swimming drains
// stamina; an exhausted swimmer loses health and can drown.
#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"

#include "DarkBloodRules/Survival.h"

#include "DBSurvivalComponent.generated.h"

class UGameplayEffect;

UCLASS(ClassGroup = (DarkBlood))
class DARKBLOOD_API UDBSurvivalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBSurvivalComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server: food was eaten. */
	void Eat(float Satiety);
	/** Server, development: set satiety and warmth directly. */
	void SetValues(float Satiety, float Warmth);

	float GetSatiety() const { return ReplicatedSatiety; }
	float GetWarmth() const { return ReplicatedWarmth; }
	/** DarkBlood::Rules::ESurvivalStatus flags. */
	uint8 GetStatus() const { return Status; }
	/** Cold exposure 0..1 at the last update (server). */
	float GetColdExposure() const { return LastExposure; }

private:
	void UpdateSurvival(float Seconds);
	void UpdateSwimming(float Seconds);
	void ApplyModifiers(const DarkBlood::Rules::FSurvivalModifiers& Modifiers);
	void Notify(const FText& Text) const;

	DarkBlood::Rules::FSurvivalState State;

	UPROPERTY(Replicated)
	float ReplicatedSatiety = 80.f;

	UPROPERTY(Replicated)
	float ReplicatedWarmth = 100.f;

	UPROPERTY(Replicated)
	uint8 Status = 0;

	FActiveGameplayEffectHandle SurvivalEffect;
	float AppliedStamina = 1.f;
	float AppliedHealth = 1.f;
	float Accumulator = 0.f;
	float LastExposure = 0.f;
	bool bSwimming = false;
	bool bExhaustedWarned = false;

	UPROPERTY(Transient)
	TObjectPtr<UGameplayEffect> DrownEffect;
};
