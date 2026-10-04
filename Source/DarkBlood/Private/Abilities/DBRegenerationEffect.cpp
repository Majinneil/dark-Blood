#include "Abilities/DBRegenerationEffect.h"

#include "Abilities/DBAttributeSet.h"
#include "Core/DBGameplayTags.h"

UDBRegenerationEffect::UDBRegenerationEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(TickSeconds);
	bExecutePeriodicEffectOnApplication = false;

	// Modifier tag requirements are evaluated on every periodic execution against the owner's current tags.
	auto AddRegen = [this](const FGameplayAttribute& Target, const FGameplayAttribute& RatePerSecond, float Scale,
		std::initializer_list<FGameplayTag> PausedBy)
	{
		FAttributeBasedFloat PerTick;
		PerTick.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(RatePerSecond, EGameplayEffectAttributeCaptureSource::Target, false);
		PerTick.Coefficient = FScalableFloat(TickSeconds * Scale);

		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Target;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(PerTick);
		Modifier.TargetTags.IgnoreTags.AddTag(DBTags::State_Dead);
		for (const FGameplayTag& Tag : PausedBy)
		{
			Modifier.TargetTags.IgnoreTags.AddTag(Tag);
		}
		Modifiers.Add(Modifier);
	};

	AddRegen(UDBAttributeSet::GetHealthAttribute(), UDBAttributeSet::GetHealthRegenAttribute(), 1.f, {});
	// Stamina pauses briefly after being spent and while sprinting or holding a block.
	AddRegen(UDBAttributeSet::GetStaminaAttribute(), UDBAttributeSet::GetStaminaRegenAttribute(), 1.f,
		{DBTags::State_StaminaRegenDelay, DBTags::State_Sprinting, DBTags::State_Blocking, DBTags::State_Swimming});
	AddRegen(UDBAttributeSet::GetManaAttribute(), UDBAttributeSet::GetManaRegenAttribute(), 1.f, {});
	// Poise refills completely within a second once no poise damage was taken for a while.
	AddRegen(UDBAttributeSet::GetPoiseAttribute(), UDBAttributeSet::GetMaxPoiseAttribute(), 1.f, {DBTags::State_PoiseRecoverDelay});
}
