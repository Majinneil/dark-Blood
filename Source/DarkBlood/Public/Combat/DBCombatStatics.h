// Shared combat helpers used by abilities, the damage execution and characters.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "DarkBloodRules/Combat.h"

class AActor;
class UAbilitySystemComponent;
class UDBAbilitySystemComponent;

/** Everything needed to apply one melee/ability hit. */
struct FDBHitParams
{
	float BaseDamage = 0.f;
	float PoiseDamage = 0.f;
	/** Damage.Type.* (defaults to physical). */
	FGameplayTag DamageType;
	bool bKnockdown = false;
	bool bUnblockable = false;
	bool bUnparryable = false;
	/** Level of the attacker (armor formula). */
	int32 AttackerLevel = 1;
};

namespace DBCombat
{
	/** Readable name for logs and UI (character name for players, display name for enemies). */
	DARKBLOOD_API FString GetCombatName(const AActor* Actor);

	/** True if Attacker may damage / lock onto Target (different teams, neither neutral, target alive). */
	DARKBLOOD_API bool CanTarget(const AActor* Attacker, const AActor* Target);

	/** Server: applies a hit from Source to Target through UDBDamageEffect. Returns false if nothing was applied. */
	DARKBLOOD_API bool ApplyHit(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, const FDBHitParams& Params);

	/** Server: tells the victim to play a hit reaction (deferred to the next tick). Causer = knockback source. */
	DARKBLOOD_API void SendHitReact(UDBAbilitySystemComponent* VictimASC, DarkBlood::Rules::EHitReaction Reaction, const AActor* Causer);

	DARKBLOOD_API const DarkBlood::Rules::FPoiseRules& GetPoiseRules();
	DARKBLOOD_API const DarkBlood::Rules::FLockOnRules& GetLockOnRules();
}
