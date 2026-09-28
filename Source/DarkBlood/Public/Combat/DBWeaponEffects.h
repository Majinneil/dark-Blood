// On-hit effects of the wielded weapon (UDBItemDefinition::WeaponEffects): elemental damage, damage over time,
// lifesteal, poise break and chain strikes. Server only; everything goes through the normal damage execution,
// so resistances, i-frames and blocking apply to the extra damage as well.
#pragma once

#include "CoreMinimal.h"

class ADBCharacterBase;
class UDBItemDefinition;
struct FDBHitParams;

namespace DBWeaponEffects
{
	/** Main-hand weapon of a player character (from the replicated equipment; null for enemies / unarmed). */
	DARKBLOOD_API const UDBItemDefinition* GetMainHandWeapon(const ADBCharacterBase* Character);

	/** Before the hit is applied: weapon modifiers of the hit itself (poise break). */
	DARKBLOOD_API void ModifyOutgoingHit(const ADBCharacterBase* Attacker, FDBHitParams& Hit);

	/** After a landed melee hit on Target: rolls and applies the weapon's effects. */
	DARKBLOOD_API void ApplyOnHit(ADBCharacterBase* Attacker, ADBCharacterBase* Target, const FDBHitParams& Hit);
}
