#include "DarkBloodRules/Combat.h"

#include <algorithm>
#include <cmath>

namespace DarkBlood::Rules
{
	namespace
	{
		constexpr float Pi = 3.14159265358979f;
		constexpr float RadToDeg = 180.f / Pi;
	}

	int32 NextComboStep(int32 CurrentStep, int32 NumSteps, float SecondsSinceHit, float ComboWindowSeconds)
	{
		if (NumSteps <= 0 || CurrentStep < 0 || SecondsSinceHit < 0.f || SecondsSinceHit > ComboWindowSeconds)
		{
			return 0;
		}
		const int32 Next = CurrentStep + 1;
		return Next < NumSteps ? Next : 0;
	}

	FChargeResult EvaluateCharge(float HeldSeconds, const FChargeRules& Rules)
	{
		FChargeResult Result;
		if (HeldSeconds < Rules.MinChargeSeconds)
		{
			return Result;
		}
		const float Span = std::max(0.001f, Rules.FullChargeSeconds - Rules.MinChargeSeconds);
		Result.bCharged = true;
		Result.ChargeFraction = std::clamp((HeldSeconds - Rules.MinChargeSeconds) / Span, 0.f, 1.f);
		Result.DamageMultiplier = 1.f + (Rules.MaxDamageMultiplier - 1.f) * Result.ChargeFraction;
		Result.PoiseMultiplier = 1.f + (Rules.MaxPoiseMultiplier - 1.f) * Result.ChargeFraction;
		return Result;
	}

	bool IsInsideAttackArc(float RelX, float RelY, float Range, float HalfAngleDegrees, float TargetRadius)
	{
		const float Distance = std::sqrt(RelX * RelX + RelY * RelY);
		const float Radius = std::max(0.f, TargetRadius);
		if (Distance - Radius > Range)
		{
			return false;
		}
		if (Distance <= Radius)
		{
			return true; // overlapping the attacker
		}
		const float Angle = std::atan2(std::fabs(RelY), RelX) * RadToDeg;
		// The target's body widens the arc: a hit on its edge still counts.
		const float Tolerance = std::asin(std::min(1.f, Radius / Distance)) * RadToDeg;
		return Angle <= HalfAngleDegrees + Tolerance;
	}

	bool CanStartStaminaAction(float CurrentStamina, float Cost)
	{
		return Cost <= 0.f || CurrentStamina > 0.f;
	}

	float StaminaAfterCost(float CurrentStamina, float Cost)
	{
		return std::max(0.f, CurrentStamina - std::max(0.f, Cost));
	}

	FPoiseResult ApplyPoiseDamage(float CurrentPoise, float MaxPoise, float PoiseDamage, bool bForceKnockdown, const FPoiseRules& Rules)
	{
		FPoiseResult Result;
		Result.NewPoise = std::clamp(CurrentPoise, 0.f, std::max(0.f, MaxPoise));
		if (PoiseDamage <= 0.f && !bForceKnockdown)
		{
			return Result;
		}
		if (bForceKnockdown)
		{
			Result.NewPoise = MaxPoise;
			Result.Reaction = EHitReaction::Knockdown;
			return Result;
		}

		const float Remaining = Result.NewPoise - PoiseDamage;
		if (Remaining > 0.f)
		{
			Result.NewPoise = Remaining;
			Result.Reaction = EHitReaction::Flinch;
			return Result;
		}

		Result.NewPoise = MaxPoise;
		const bool bHeavyHit = MaxPoise > 0.f && PoiseDamage >= MaxPoise * Rules.KnockdownFraction;
		Result.Reaction = bHeavyHit ? EHitReaction::Knockdown : EHitReaction::Stagger;
		return Result;
	}

	float ReactionDurationSeconds(EHitReaction Reaction, const FPoiseRules& Rules)
	{
		switch (Reaction)
		{
		case EHitReaction::Stagger: return Rules.StaggerSeconds;
		case EHitReaction::Knockdown: return Rules.KnockdownSeconds;
		case EHitReaction::ParriedStagger: return Rules.ParriedStaggerSeconds;
		case EHitReaction::None:
		case EHitReaction::Flinch: return 0.f;
		}
		return 0.f;
	}

	bool InterruptsAction(EHitReaction Reaction)
	{
		return Reaction == EHitReaction::Stagger || Reaction == EHitReaction::Knockdown || Reaction == EHitReaction::ParriedStagger;
	}

	const char* ToString(EHitReaction Reaction)
	{
		switch (Reaction)
		{
		case EHitReaction::None: return "None";
		case EHitReaction::Flinch: return "Flinch";
		case EHitReaction::Stagger: return "Stagger";
		case EHitReaction::Knockdown: return "Knockdown";
		case EHitReaction::ParriedStagger: return "ParriedStagger";
		}
		return "Unknown";
	}

	float ScoreLockOnCandidate(float Distance, float AngleDegrees, bool bHasLineOfSight, const FLockOnRules& Rules)
	{
		if (!bHasLineOfSight || Distance < 0.f || Distance > Rules.MaxDistance || std::fabs(AngleDegrees) > Rules.MaxAngleDegrees)
		{
			return -1.f;
		}
		// Both terms normalized to 0..1: a target near the screen center wins over a slightly closer one.
		const float DistanceTerm = Distance / std::max(1.f, Rules.MaxDistance);
		const float AngleTerm = std::fabs(AngleDegrees) / std::max(1.f, Rules.MaxAngleDegrees);
		return DistanceTerm + Rules.AngleWeight * AngleTerm;
	}

	bool ShouldBreakLockOn(float Distance, bool bTargetAlive, const FLockOnRules& Rules)
	{
		return !bTargetAlive || Distance > Rules.BreakDistance;
	}
}
