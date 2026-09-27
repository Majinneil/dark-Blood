// DARK BLOOD - Rules Core: character level, XP and skill points.
// XP and skill points are only ever granted by the server.
#pragma once

#include "DarkBloodRules/RulesCore.h"

namespace DarkBlood::Rules
{
	struct FXpCurve
	{
		double Base = 120.0;
		double Exponent = 1.65;
		int32 MaxLevel = 100;

		/** XP needed to advance from Level to Level + 1. Returns 0 at or above MaxLevel. */
		DARKBLOODRULES_API int64 XpToNextLevel(int32 Level) const;

		/** Total XP needed to reach Level from level 1. */
		DARKBLOODRULES_API int64 TotalXpForLevel(int32 Level) const;
	};

	struct FProgressionRules
	{
		FXpCurve Curve;
		/** Skill points granted on every Nth level-up (0 = none). Bosses and quests grant the rest. */
		int32 SkillPointLevelInterval = 5;
		int32 MaxUnspentSkillPoints = 999;
	};

	struct FProgressionState
	{
		int32 Level = 1;
		int64 XpIntoLevel = 0;
		int64 TotalXp = 0;
		int32 UnspentSkillPoints = 0;
		int32 TotalSkillPointsEarned = 0;
	};

	struct FXpGrantResult
	{
		int64 XpApplied = 0;
		int32 LevelsGained = 0;
		int32 SkillPointsGained = 0;
	};

	/** Adds XP, performing level-ups. XP beyond the max level is discarded. Negative amounts are ignored. */
	DARKBLOODRULES_API FXpGrantResult GrantXp(FProgressionState& State, int64 Amount, const FProgressionRules& Rules);

	/** Adds skill points (bosses, important quests). Returns the amount actually granted. */
	DARKBLOODRULES_API int32 GrantSkillPoints(FProgressionState& State, int32 Amount, const FProgressionRules& Rules);

	/** Spends skill points atomically. */
	DARKBLOODRULES_API bool SpendSkillPoints(FProgressionState& State, int32 Amount);

	/** Sets a level directly (developer command). Resets XP into level. */
	DARKBLOODRULES_API void SetLevel(FProgressionState& State, int32 Level, const FProgressionRules& Rules);

	/** Consistency check used when loading or receiving uploaded character data. */
	DARKBLOODRULES_API bool IsProgressionConsistent(const FProgressionState& State, const FProgressionRules& Rules);
}
