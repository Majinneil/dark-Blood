#include "DarkBloodRules/Survival.h"

#include <algorithm>

namespace DarkBlood::Rules
{
	void AdvanceSurvival(FSurvivalState& State, double GameHours, float ColdExposure, bool bWarming, const FSurvivalRules& Rules)
	{
		if (GameHours <= 0.0)
		{
			return;
		}
		const float Hours = static_cast<float>(GameHours);
		State.Satiety = std::clamp(State.Satiety - Rules.SatietyLossPerHour * Hours, 0.f, 100.f);
		const float Exposure = std::clamp(ColdExposure, 0.f, 1.f);
		float Change = 0.f;
		if (bWarming)
		{
			Change = Rules.WarmthGainPerHour;
		}
		else if (Exposure > 0.f)
		{
			Change = -Rules.WarmthLossPerHour * Exposure;
		}
		else
		{
			Change = Rules.NeutralWarmthGainPerHour;
		}
		State.Warmth = std::clamp(State.Warmth + Change * Hours, 0.f, 100.f);
	}

	void EatFood(FSurvivalState& State, float Satiety)
	{
		State.Satiety = std::clamp(State.Satiety + std::max(0.f, Satiety), 0.f, 100.f);
	}

	FSurvivalModifiers GetSurvivalModifiers(const FSurvivalState& State, const FSurvivalRules& Rules)
	{
		FSurvivalModifiers Out;
		if (State.Satiety < Rules.StarvingBelow)
		{
			Out.Status |= ESurvivalStatus::Starving;
			Out.StaminaRegen *= 0.5f;
			Out.HealthRegen = 0.f;
		}
		else if (State.Satiety < Rules.HungryBelow)
		{
			Out.Status |= ESurvivalStatus::Hungry;
			Out.StaminaRegen *= 0.75f;
			Out.HealthRegen *= 0.5f;
		}
		else if (State.Satiety > Rules.WellFedAbove)
		{
			Out.Status |= ESurvivalStatus::WellFed;
			Out.StaminaRegen *= 1.1f;
			Out.HealthRegen *= 1.1f;
		}
		if (State.Warmth < Rules.FreezingBelow)
		{
			Out.Status |= ESurvivalStatus::Freezing;
			Out.StaminaRegen *= 0.6f;
			Out.HealthRegen = 0.f;
		}
		else if (State.Warmth < Rules.ColdBelow)
		{
			Out.Status |= ESurvivalStatus::Cold;
			Out.StaminaRegen *= 0.8f;
		}
		return Out;
	}

	float ComputeColdExposure(float RegionCold, double AltitudeMeters, bool bNight, bool bWet, float Protection)
	{
		// High ground is cold everywhere above ~600 m.
		double Cold = std::clamp(static_cast<double>(RegionCold), 0.0, 1.0) + std::clamp((AltitudeMeters - 600.0) / 800.0, 0.0, 0.6);
		if (Cold > 0.0)
		{
			Cold += bNight ? 0.15 : 0.0;
			Cold += bWet ? 0.3 : 0.0;
		}
		Cold *= 1.0 - std::clamp(static_cast<double>(Protection), 0.0, 1.0);
		return static_cast<float>(std::clamp(Cold, 0.0, 1.0));
	}
}
