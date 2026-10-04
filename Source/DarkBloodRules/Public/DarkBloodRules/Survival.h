// DARK BLOOD - Rules Core: light survival (Phase 8). Satiety and warmth slow or speed up regeneration; they never deal
// damage ("keine Frustmechaniken"). Eating fills satiety, settlements and fires warm, cold regions, altitude, night and
// wet clothes chill. The game decides exposure from the world (region, height, swimming); the rules stay pure.
#pragma once

#include "DarkBloodRules/RulesCore.h"

namespace DarkBlood::Rules
{
	struct FSurvivalState
	{
		/** 0..100 */
		float Satiety = 80.f;
		float Warmth = 100.f;
	};

	/** Bit flags of FSurvivalModifiers::Status. */
	namespace ESurvivalStatus
	{
		constexpr uint8 None = 0;
		constexpr uint8 WellFed = 1 << 0;
		constexpr uint8 Hungry = 1 << 1;
		constexpr uint8 Starving = 1 << 2;
		constexpr uint8 Cold = 1 << 3;
		constexpr uint8 Freezing = 1 << 4;
	}

	struct FSurvivalRules
	{
		/** Full to empty in ~28 game hours (about an hour of play). */
		float SatietyLossPerHour = 3.5f;
		/** Warmth lost per game hour at exposure 1 (from full to freezing in ~1.5 game hours = 3 real minutes). */
		float WarmthLossPerHour = 60.f;
		/** Warmth regained per game hour by a settlement / fire, and slowly anywhere without cold. */
		float WarmthGainPerHour = 120.f;
		float NeutralWarmthGainPerHour = 20.f;
		float WellFedAbove = 75.f;
		float HungryBelow = 25.f;
		float StarvingBelow = 5.f;
		float ColdBelow = 40.f;
		float FreezingBelow = 10.f;
	};

	struct FSurvivalModifiers
	{
		float StaminaRegen = 1.f;
		float HealthRegen = 1.f;
		uint8 Status = ESurvivalStatus::None;
	};

	DARKBLOODRULES_API void AdvanceSurvival(FSurvivalState& State, double GameHours, float ColdExposure, bool bWarming,
		const FSurvivalRules& Rules = FSurvivalRules());

	DARKBLOODRULES_API void EatFood(FSurvivalState& State, float Satiety);

	DARKBLOODRULES_API FSurvivalModifiers GetSurvivalModifiers(const FSurvivalState& State, const FSurvivalRules& Rules = FSurvivalRules());

	/**
	 * Cold exposure 0..1 from the region's cold (0..1), the height above sea level (m), night, wet clothes (swimming) and the
	 * character's cold protection (0..1, e.g. frost resistance of the gear).
	 */
	DARKBLOODRULES_API float ComputeColdExposure(float RegionCold, double AltitudeMeters, bool bNight, bool bWet, float Protection);
}
