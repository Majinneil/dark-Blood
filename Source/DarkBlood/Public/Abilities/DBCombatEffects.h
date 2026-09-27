// Native gameplay effects used by combat code. Configured in C++ so combat works without authored
// assets; Blueprint children can add cues (VFX/audio) later.
#pragma once

#include "GameplayEffect.h"

#include "DBCombatEffects.generated.h"

/**
 * Instant damage through UDBDamageExecution.
 * SetByCaller.Damage / SetByCaller.PoiseDamage + Damage.* asset tags (see DBCombat::ApplyHit).
 */
UCLASS()
class DARKBLOOD_API UDBDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDBDamageEffect();
};

/** Instant stamina change (SetByCaller.StaminaCost; pass the cost negated, e.g. -20). */
UCLASS()
class DARKBLOOD_API UDBStaminaCostEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDBStaminaCostEffect();
};
