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
