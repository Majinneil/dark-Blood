#include "Abilities/DBCombatEffects.h"

#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBDamageExecution.h"
#include "Core/DBGameplayTags.h"

UDBDamageEffect::UDBDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UDBDamageExecution::StaticClass();
	Executions.Add(Execution);
}

UDBDamageOverTimeEffect::UDBDamageOverTimeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f));
	Period = 1.f;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UDBDamageExecution::StaticClass();
	Executions.Add(Execution);
}

UDBStaminaCostEffect::UDBStaminaCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Cost;
	Cost.DataTag = DBTags::SetByCaller_StaminaCost;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UDBAttributeSet::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	// SetByCaller values cannot be scaled, so callers pass the cost already negated.
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Cost);
	Modifiers.Add(Modifier);
}

namespace
{
	FGameplayModifierInfo SetByCallerModifier(const FGameplayAttribute& Attribute)
	{
		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = DBTags::SetByCaller_Magnitude;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
		return Modifier;
	}
}

UDBManaChangeEffect::UDBManaChangeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(SetByCallerModifier(UDBAttributeSet::GetManaAttribute()));
}

UDBHealEffect::UDBHealEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Modifiers.Add(SetByCallerModifier(UDBAttributeSet::GetHealthAttribute()));
}

UDBSurvivalEffect::UDBSurvivalEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	auto Multiply = [this](const FGameplayAttribute& Attribute, const FGameplayTag& Tag)
	{
		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = Tag;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
		Modifiers.Add(Modifier);
	};
	Multiply(UDBAttributeSet::GetStaminaRegenAttribute(), DBTags::SetByCaller_SurvivalStamina);
	Multiply(UDBAttributeSet::GetHealthRegenAttribute(), DBTags::SetByCaller_SurvivalHealth);
}

UDBIronStanceEffect::UDBIronStanceEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(SetByCallerModifier(UDBAttributeSet::GetArmorAttribute()));
	Modifiers.Add(SetByCallerModifier(UDBAttributeSet::GetMaxPoiseAttribute()));
}

UDBManaFlowEffect::UDBManaFlowEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(SetByCallerModifier(UDBAttributeSet::GetManaRegenAttribute()));
}

UDBIronBodyEffect::UDBIronBodyEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Modifiers.Add(SetByCallerModifier(UDBAttributeSet::GetMaxPoiseAttribute()));
}
