#include "DarkBloodRules/Stats.h"

#include <algorithm>
#include <cmath>

namespace DarkBlood::Rules
{
	FPrimaryStats& FPrimaryStats::operator+=(const FPrimaryStats& Other)
	{
		Strength += Other.Strength;
		Dexterity += Other.Dexterity;
		Intelligence += Other.Intelligence;
		Spirit += Other.Spirit;
		Vitality += Other.Vitality;
		Endurance += Other.Endurance;
		return *this;
	}

	FPrimaryStats ComputePrimaryStats(const FClassGrowth& Growth, int32 Level)
	{
		const float Levels = static_cast<float>(std::max(0, Level - 1));
		FPrimaryStats Result = Growth.BaseAtLevel1;
		Result.Strength += Growth.PerLevel.Strength * Levels;
		Result.Dexterity += Growth.PerLevel.Dexterity * Levels;
		Result.Intelligence += Growth.PerLevel.Intelligence * Levels;
		Result.Spirit += Growth.PerLevel.Spirit * Levels;
		Result.Vitality += Growth.PerLevel.Vitality * Levels;
		Result.Endurance += Growth.PerLevel.Endurance * Levels;
		return Result;
	}

	FDerivedStats ComputeDerivedStats(const FPrimaryStats& S, const FDerivedStatFormula& F)
	{
		FDerivedStats D;
		D.MaxHealth = F.BaseHealth + S.Vitality * F.HealthPerVitality;
		D.MaxStamina = F.BaseStamina + S.Endurance * F.StaminaPerEndurance;
		D.MaxMana = F.BaseMana + S.Intelligence * F.ManaPerIntelligence + S.Spirit * F.ManaPerSpirit;
		D.HealthRegen = S.Vitality * F.HealthRegenPerVitality;
		D.StaminaRegen = F.BaseStaminaRegen + S.Endurance * F.StaminaRegenPerEndurance;
		D.ManaRegen = F.BaseManaRegen + S.Spirit * F.ManaRegenPerSpirit;
		D.AttackPower = S.Strength * F.AttackPowerPerStrength + S.Dexterity * F.AttackPowerPerDexterity;
		D.SpellPower = S.Intelligence * F.SpellPowerPerIntelligence + S.Spirit * F.SpellPowerPerSpirit;
		D.CritChance = std::min(F.MaxCritChance, F.BaseCritChance + S.Dexterity * F.CritChancePerDexterity);
		return D;
	}

	int32 ComputePowerRating(int32 Level, float GearScore)
	{
		const float Clamped = std::max(0.f, GearScore);
		return std::max(1, static_cast<int32>(std::lround(0.75f * static_cast<float>(Level) + 0.25f * Clamped)));
	}

	EDangerTier AssessDanger(int32 PlayerPower, int32 RecommendedMin, int32 RecommendedMax, const FDangerThresholds& T)
	{
		if (RecommendedMax < RecommendedMin)
		{
			std::swap(RecommendedMin, RecommendedMax);
		}
		if (PlayerPower >= RecommendedMax + T.TrivialAboveMax)
		{
			return EDangerTier::Trivial;
		}
		if (PlayerPower >= RecommendedMin)
		{
			return EDangerTier::Appropriate;
		}
		if (PlayerPower >= RecommendedMin - T.ChallengingBelowMin)
		{
			return EDangerTier::Challenging;
		}
		if (PlayerPower >= RecommendedMin - T.DangerousBelowMin)
		{
			return EDangerTier::Dangerous;
		}
		return EDangerTier::Extreme;
	}

	const char* ToString(EDangerTier Tier)
	{
		switch (Tier)
		{
		case EDangerTier::Trivial: return "Trivial";
		case EDangerTier::Appropriate: return "Appropriate";
		case EDangerTier::Challenging: return "Challenging";
		case EDangerTier::Dangerous: return "Dangerous";
		case EDangerTier::Extreme: return "Extreme";
		}
		return "Unknown";
	}
}
