#include "DarkBloodRules/Progression.h"

#include <algorithm>
#include <cmath>

namespace DarkBlood::Rules
{
	int64 FXpCurve::XpToNextLevel(int32 Level) const
	{
		if (Level < 1 || Level >= MaxLevel)
		{
			return 0;
		}
		return static_cast<int64>(std::llround(Base * std::pow(static_cast<double>(Level), Exponent)));
	}

	int64 FXpCurve::TotalXpForLevel(int32 Level) const
	{
		int64 Total = 0;
		const int32 Target = std::clamp(Level, 1, MaxLevel);
		for (int32 Current = 1; Current < Target; ++Current)
		{
			Total += XpToNextLevel(Current);
		}
		return Total;
	}

	FXpGrantResult GrantXp(FProgressionState& State, int64 Amount, const FProgressionRules& Rules)
	{
		FXpGrantResult Result;
		if (Amount <= 0 || State.Level >= Rules.Curve.MaxLevel)
		{
			return Result;
		}

		int64 Remaining = Amount;
		while (Remaining > 0 && State.Level < Rules.Curve.MaxLevel)
		{
			const int64 Needed = Rules.Curve.XpToNextLevel(State.Level) - State.XpIntoLevel;
			if (Remaining < Needed)
			{
				State.XpIntoLevel += Remaining;
				Result.XpApplied += Remaining;
				Remaining = 0;
				break;
			}

			Remaining -= Needed;
			Result.XpApplied += Needed;
			State.XpIntoLevel = 0;
			++State.Level;
			++Result.LevelsGained;

			if (Rules.SkillPointLevelInterval > 0 && State.Level % Rules.SkillPointLevelInterval == 0)
			{
				Result.SkillPointsGained += GrantSkillPoints(State, 1, Rules);
			}
		}

		State.TotalXp += Result.XpApplied;
		return Result;
	}

	int32 GrantSkillPoints(FProgressionState& State, int32 Amount, const FProgressionRules& Rules)
	{
		if (Amount <= 0)
		{
			return 0;
		}
		const int32 Granted = std::min(Amount, std::max(0, Rules.MaxUnspentSkillPoints - State.UnspentSkillPoints));
		State.UnspentSkillPoints += Granted;
		State.TotalSkillPointsEarned += Granted;
		return Granted;
	}

	bool SpendSkillPoints(FProgressionState& State, int32 Amount)
	{
		if (Amount <= 0 || State.UnspentSkillPoints < Amount)
		{
			return false;
		}
		State.UnspentSkillPoints -= Amount;
		return true;
	}

	void SetLevel(FProgressionState& State, int32 Level, const FProgressionRules& Rules)
	{
		State.Level = std::clamp(Level, 1, Rules.Curve.MaxLevel);
		State.XpIntoLevel = 0;
		State.TotalXp = Rules.Curve.TotalXpForLevel(State.Level);
	}

	bool IsProgressionConsistent(const FProgressionState& State, const FProgressionRules& Rules)
	{
		if (State.Level < 1 || State.Level > Rules.Curve.MaxLevel)
		{
			return false;
		}
		if (State.XpIntoLevel < 0 || State.TotalXp < 0)
		{
			return false;
		}
		if (State.Level < Rules.Curve.MaxLevel && State.XpIntoLevel >= Rules.Curve.XpToNextLevel(State.Level))
		{
			return false;
		}
		if (State.Level == Rules.Curve.MaxLevel && State.XpIntoLevel != 0)
		{
			return false;
		}
		if (State.TotalXp < Rules.Curve.TotalXpForLevel(State.Level) + State.XpIntoLevel)
		{
			return false;
		}
		if (State.UnspentSkillPoints < 0 || State.UnspentSkillPoints > State.TotalSkillPointsEarned ||
			State.UnspentSkillPoints > Rules.MaxUnspentSkillPoints)
		{
			return false;
		}
		return true;
	}
}
