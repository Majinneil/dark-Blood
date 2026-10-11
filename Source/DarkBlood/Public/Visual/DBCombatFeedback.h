// Combat feedback (Phase 17): particle effects for hits, blocks, parries and deaths, played locally on every machine
// that renders. The server raises GameplayCues (DBDamageExecution, abilities); ADBCharacterBase receives them through
// IGameplayCueInterface and hands them here. Effects are the Paragon particle systems in Content/Paragon* (Cascade);
// a machine without those packs simply plays nothing - gameplay never depends on them.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;
class UParticleSystem;
struct FGameplayCueParameters;

/** What happened, decides the effect. */
enum class EDBCombatFx : uint8
{
	HitByPlayer,
	HitByPlayerHeavy,
	HitByDemon,
	Blocked,
	Parried,
	Stagger,
	DemonDeath,
	BossDeath,
	/** A boss's telegraphed blow lands. */
	GroundBlast,
	/** Water crashing down (foot of a waterfall). */
	WaterSplash,
	Count
};

namespace DBCombatFeedback
{
	/** Plays the effect of a combat cue on Target (Executed events of GameplayCue.Combat.*). */
	DARKBLOOD_API void HandleCue(AActor* Target, FGameplayTag Cue, const FGameplayCueParameters& Parameters);

	/** Plays one effect at a place (Normal points from the target towards the attacker). */
	DARKBLOOD_API void Play(const UObject* WorldContext, EDBCombatFx Fx, const FVector& Location, const FVector& Normal, float Scale = 1.f);

	/** Only the sound of an effect (Phase 18; Play includes it). */
	DARKBLOOD_API void PlaySound(const UObject* WorldContext, EDBCombatFx Fx, const FVector& Location, float Scale = 1.f);

	/** The particle system of an effect (loaded on first use; null if the pack is missing). */
	DARKBLOOD_API UParticleSystem* GetSystem(EDBCombatFx Fx);
	DARKBLOOD_API const TCHAR* GetName(EDBCombatFx Fx);

	/** Damage at or above this counts as a heavy hit (bigger effect). */
	constexpr float HeavyHitDamage = 60.f;
}
