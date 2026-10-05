// DARK BLOOD - Rules Core: boss rules (Phase 10, docs/BOSS_FRAMEWORK.md). Co-op scaling is not just "more health":
// more players add summoned demons, split the boss' attention and add area pressure; health rises moderately as the
// last lever. Phases change at health thresholds and never fall back; long fights enrage.
#pragma once

#include "DarkBloodRules/RulesCore.h"

#include <vector>

namespace DarkBlood::Rules
{
	struct FBossScaling
	{
		float HealthMultiplier = 1.f;
		/** Extra demons per summon wave. */
		int32 ExtraAdds = 0;
		/** 2+ players: the boss switches targets after every attack chain. */
		bool bSplitAttention = false;
		/** 3+ players: additional area attacks (hazards under every player). */
		bool bAreaPressure = false;
		/** Cooldowns of special attacks are multiplied by this. */
		float CooldownMultiplier = 1.f;
	};

	DARKBLOODRULES_API FBossScaling GetBossScaling(int32 PlayerCount);

	/**
	 * Phase (0-based) for the boss' health fraction. Thresholds are descending health fractions (e.g. {0.66, 0.33}: phase 1
	 * below 66 %, phase 2 below 33 %). The phase never decreases, even if the boss heals.
	 */
	DARKBLOODRULES_API int32 EvaluateBossPhase(float HealthFraction, int32 CurrentPhase, const std::vector<float>& Thresholds);

	/** Damage multiplier: 1 until EnrageAfterSeconds, then +25 % every 30 s, capped at 2. */
	DARKBLOODRULES_API float GetEnrageMultiplier(double FightSeconds, double EnrageAfterSeconds);
}
