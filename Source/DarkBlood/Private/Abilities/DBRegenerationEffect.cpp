#include "Abilities/DBRegenerationEffect.h"

#include "Abilities/DBAttributeSet.h"

UDBRegenerationEffect::UDBRegenerationEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(TickSeconds);
	bExecutePeriodicEffectOnApplication = false;

	auto AddRegen = [this](const FGameplayAttribute& Target, const FGameplayAttribute& RatePerSecond)
	{
		FAttributeBasedFloat PerTick;
		PerTick.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(RatePerSecond, EGameplayEffectAttributeCaptureSource::Target, false);
		PerTick.Coefficient = FScalableFloat(TickSeconds);

		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Target;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(PerTick);
		Modifiers.Add(Modifier);
	};

	AddRegen(UDBAttributeSet::GetHealthAttribute(), UDBAttributeSet::GetHealthRegenAttribute());
	AddRegen(UDBAttributeSet::GetStaminaAttribute(), UDBAttributeSet::GetStaminaRegenAttribute());
	AddRegen(UDBAttributeSet::GetManaAttribute(), UDBAttributeSet::GetManaRegenAttribute());
	// Phase 2: stamina regeneration delay after exertion is implemented with an "exhausted" tag requirement.
}
