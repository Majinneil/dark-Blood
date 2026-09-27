// DARK BLOOD - Rules Core: character stats, power rating and regional danger.
#pragma once

#include "DarkBloodRules/RulesCore.h"

namespace DarkBlood::Rules
{
	struct FPrimaryStats
	{
		float Strength = 0.f;     // Kraft: melee power, heavy weapons
		float Dexterity = 0.f;    // Geschick: speed weapons, crit
		float Intelligence = 0.f; // Intellekt: spell power, max mana
		float Spirit = 0.f;       // Geist: mana regen, ki techniques, barrier strength
		float Vitality = 0.f;     // Vitalitaet: max health
		float Endurance = 0.f;    // Ausdauer: max stamina, stamina regen

		DARKBLOODRULES_API FPrimaryStats& operator+=(const FPrimaryStats& Other);
	};

	/** Per-class growth profile. Authored in UE class definition data assets. */
	struct FClassGrowth
	{
		FPrimaryStats BaseAtLevel1;
		FPrimaryStats PerLevel;
	};

	struct FDerivedStatFormula
	{
		float BaseHealth = 100.f;
		float HealthPerVitality = 12.f;
		float BaseStamina = 100.f;
		float StaminaPerEndurance = 3.f;
		float BaseMana = 50.f;
		float ManaPerIntelligence = 6.f;
		float ManaPerSpirit = 2.f;
		float HealthRegenPerVitality = 0.05f;
		float BaseStaminaRegen = 18.f;
		float StaminaRegenPerEndurance = 0.15f;
		float BaseManaRegen = 2.f;
		float ManaRegenPerSpirit = 0.12f;
		float AttackPowerPerStrength = 2.f;
		float AttackPowerPerDexterity = 1.f;
		float SpellPowerPerIntelligence = 2.f;
		float SpellPowerPerSpirit = 0.5f;
		float BaseCritChance = 0.05f;
		float CritChancePerDexterity = 0.001f;
		float MaxCritChance = 0.6f;
	};

	struct FDerivedStats
	{
		float MaxHealth = 0.f;
		float MaxStamina = 0.f;
		float MaxMana = 0.f;
		float HealthRegen = 0.f;
		float StaminaRegen = 0.f;
		float ManaRegen = 0.f;
		float AttackPower = 0.f;
		float SpellPower = 0.f;
		float CritChance = 0.f;
	};

	DARKBLOODRULES_API FPrimaryStats ComputePrimaryStats(const FClassGrowth& Growth, int32 Level);

	/** Equipment bonuses are added to the primary stats before deriving. */
	DARKBLOODRULES_API FDerivedStats ComputeDerivedStats(const FPrimaryStats& Stats, const FDerivedStatFormula& Formula);

	/**
	 * Power rating ("Staerke") shown in the UI and compared against region recommendations.
	 * GearScore is the average item level of equipped gear (0 when nothing is equipped).
	 */
	DARKBLOODRULES_API int32 ComputePowerRating(int32 Level, float GearScore);

	enum class EDangerTier : uint8
	{
		Trivial,     // Gering
		Appropriate, // Angemessen
		Challenging, // Herausfordernd
		Dangerous,   // Gefaehrlich
		Extreme,     // Extrem
	};

	struct FDangerThresholds
	{
		int32 TrivialAboveMax = 10;
		int32 ChallengingBelowMin = 5;
		int32 DangerousBelowMin = 15;
	};

	/** Danger relative to the player. Never blocks travel; it only informs the player. */
	DARKBLOODRULES_API EDangerTier AssessDanger(int32 PlayerPower, int32 RecommendedMin, int32 RecommendedMax,
		const FDangerThresholds& Thresholds = FDangerThresholds());

	DARKBLOODRULES_API const char* ToString(EDangerTier Tier);
}
