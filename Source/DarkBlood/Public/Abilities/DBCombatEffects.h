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

/** Instant mana change (SetByCaller.Magnitude; negative = spent). */
UCLASS()
class DARKBLOOD_API UDBManaChangeEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDBManaChangeEffect();
};

/** Instant heal (SetByCaller.Magnitude). */
UCLASS()
class DARKBLOOD_API UDBHealEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDBHealEffect();
};

/** Warrior Iron Stance while active: +Armor and +MaxPoise (SetByCaller.Magnitude scales both). */
UCLASS()
class DARKBLOOD_API UDBIronStanceEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDBIronStanceEffect();
};

/** Passive (Magier "Manafluss"): +ManaRegen (SetByCaller.Magnitude). */
UCLASS()
class DARKBLOOD_API UDBManaFlowEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDBManaFlowEffect();
};

/** Passive (Moench "Eisenkoerper"): +MaxPoise (SetByCaller.Magnitude). */
UCLASS()
class DARKBLOOD_API UDBIronBodyEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDBIronBodyEffect();
};
