// DARK BLOOD - Rules Core: combat timing, poise and targeting.
// Everything that decides *whether* an action succeeds lives here; the UE layer only feeds in
// measured times, distances and attribute values (server authoritative).
#pragma once

#include "DarkBloodRules/RulesCore.h"

namespace DarkBlood::Rules
{
	// ---- Attacks ------------------------------------------------------------------------------

	/** One step of a melee move (a combo hit, a heavy swing ...). Times in seconds, distances in cm. */
	struct FAttackStep
	{
		float BaseDamage = 10.f;
		float StaminaCost = 10.f;
		float PoiseDamage = 10.f;
		float WindupSeconds = 0.2f;
		float ActiveSeconds = 0.15f;
		float RecoverySeconds = 0.35f;
		/** Time after the hit in which the next input continues the combo. */
		float ComboWindowSeconds = 0.5f;
		float Range = 200.f;
		/** Half opening angle of the hit arc in front of the attacker. */
		float HalfAngleDegrees = 60.f;
		/** Knocks the target down even if its poise holds. */
		bool bKnockdown = false;
	};

	/**
	 * Next combo step for an input that arrives SecondsSinceHit after the current step connected.
	 * Returns 0 (restart) when the window was missed or the chain is finished.
	 */
	DARKBLOODRULES_API int32 NextComboStep(int32 CurrentStep, int32 NumSteps, float SecondsSinceHit, float ComboWindowSeconds);

	struct FChargeRules
	{
		/** Holding shorter than this is a normal (uncharged) attack. */
		float MinChargeSeconds = 0.35f;
		float FullChargeSeconds = 1.5f;
		float MaxDamageMultiplier = 2.5f;
		float MaxPoiseMultiplier = 3.f;
	};

	struct FChargeResult
	{
		bool bCharged = false;
		/** 0..1 */
		float ChargeFraction = 0.f;
		float DamageMultiplier = 1.f;
		float PoiseMultiplier = 1.f;
	};

	DARKBLOODRULES_API FChargeResult EvaluateCharge(float HeldSeconds, const FChargeRules& Rules = FChargeRules());

	/** True if Point (relative to the attacker, forward = +X) lies inside the attack arc. */
	DARKBLOODRULES_API bool IsInsideAttackArc(float RelX, float RelY, float Range, float HalfAngleDegrees, float TargetRadius);

	// ---- Stamina ------------------------------------------------------------------------------

	/**
	 * Actions may start with any positive stamina; the cost may drain it to zero (never below).
	 * Blocking with zero stamina breaks the guard.
	 */
	DARKBLOODRULES_API bool CanStartStaminaAction(float CurrentStamina, float Cost);
	DARKBLOODRULES_API float StaminaAfterCost(float CurrentStamina, float Cost);

	// ---- Poise / hit reactions ----------------------------------------------------------------

	enum class EHitReaction : uint8
	{
		None,
		/** Cosmetic reaction, the current action continues. */
		Flinch,
		/** Poise broken: current action is interrupted. */
		Stagger,
		/** Thrown to the ground, longer recovery (get-up). */
		Knockdown,
		/** Attacker bounced off a perfect parry. */
		ParriedStagger,
	};

	struct FPoiseRules
	{
		/** A single hit dealing at least this fraction of max poise knocks down after breaking poise. */
		float KnockdownFraction = 0.75f;
		float StaggerSeconds = 0.6f;
		float KnockdownSeconds = 1.6f;
		float ParriedStaggerSeconds = 1.0f;
		/** Poise refills completely after this long without poise damage. */
		float RecoverDelaySeconds = 3.f;
	};

	struct FPoiseResult
	{
		float NewPoise = 0.f;
		EHitReaction Reaction = EHitReaction::None;
	};

	/** Applies poise damage. A broken poise resets to max (the reaction is the punishment). */
	DARKBLOODRULES_API FPoiseResult ApplyPoiseDamage(float CurrentPoise, float MaxPoise, float PoiseDamage, bool bForceKnockdown,
		const FPoiseRules& Rules = FPoiseRules());

	DARKBLOODRULES_API float ReactionDurationSeconds(EHitReaction Reaction, const FPoiseRules& Rules = FPoiseRules());

	/** Reactions that cancel the victim's current action. */
	DARKBLOODRULES_API bool InterruptsAction(EHitReaction Reaction);

	DARKBLOODRULES_API const char* ToString(EHitReaction Reaction);

	// ---- Lock-on ------------------------------------------------------------------------------

	struct FLockOnRules
	{
		float MaxDistance = 2000.f;
		/** Half opening angle around the camera direction for acquiring a target. */
		float MaxAngleDegrees = 45.f;
		/** A locked target further away than this is released. */
		float BreakDistance = 2600.f;
		/** How strongly angle (vs. distance) matters when picking a target. */
		float AngleWeight = 2.f;
	};

	/**
	 * Score of a lock-on candidate (lower is better). Returns a negative value for candidates that
	 * cannot be acquired (too far, outside the cone, no line of sight).
	 */
	DARKBLOODRULES_API float ScoreLockOnCandidate(float Distance, float AngleDegrees, bool bHasLineOfSight,
		const FLockOnRules& Rules = FLockOnRules());

	DARKBLOODRULES_API bool ShouldBreakLockOn(float Distance, bool bTargetAlive, const FLockOnRules& Rules = FLockOnRules());
}
