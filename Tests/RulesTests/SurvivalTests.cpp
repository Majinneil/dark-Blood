#include "TestFramework.h"

#include "DarkBloodRules/Items.h"
#include "DarkBloodRules/Survival.h"

using namespace DarkBlood::Rules;

DB_TEST(Survival_HungerSlowsRegenerationButNeverHurts)
{
	FSurvivalState State;
	State.Satiety = 100.f;
	DB_CHECK(GetSurvivalModifiers(State).Status & ESurvivalStatus::WellFed);
	DB_CHECK(GetSurvivalModifiers(State).StaminaRegen > 1.f);

	AdvanceSurvival(State, 24.0, 0.f, false);
	DB_CHECK(State.Satiety < 25.f);
	const FSurvivalModifiers Hungry = GetSurvivalModifiers(State);
	DB_CHECK(Hungry.Status & ESurvivalStatus::Hungry);
	DB_CHECK(Hungry.StaminaRegen < 1.f);

	AdvanceSurvival(State, 100.0, 0.f, false);
	DB_CHECK(State.Satiety == 0.f);
	const FSurvivalModifiers Starving = GetSurvivalModifiers(State);
	DB_CHECK(Starving.Status & ESurvivalStatus::Starving);
	DB_CHECK(Starving.HealthRegen == 0.f);
	DB_CHECK(Starving.StaminaRegen > 0.f); // slower, never zero: no frustration mechanic

	EatFood(State, 35.f);
	DB_CHECK(State.Satiety == 35.f);
	DB_CHECK((GetSurvivalModifiers(State).Status & (ESurvivalStatus::Hungry | ESurvivalStatus::Starving)) == 0);
}

DB_TEST(Survival_ColdChillsAndSettlementsWarm)
{
	// The ice waste at night, wet from the sea, without protection: freezing within minutes of game time.
	const float Exposure = ComputeColdExposure(0.8f, 200.0, true, true, 0.f);
	DB_CHECK(Exposure == 1.f);
	DB_CHECK(ComputeColdExposure(0.f, 100.0, true, true, 0.f) == 0.f); // warm lowlands stay warm
	DB_CHECK(ComputeColdExposure(0.f, 1100.0, false, false, 0.f) > 0.5f); // mountain tops are cold
	DB_CHECK(ComputeColdExposure(0.8f, 200.0, false, false, 0.5f) < ComputeColdExposure(0.8f, 200.0, false, false, 0.f));

	FSurvivalState State;
	AdvanceSurvival(State, 1.1, Exposure, false);
	DB_CHECK(GetSurvivalModifiers(State).Status & ESurvivalStatus::Cold);
	AdvanceSurvival(State, 1.0, Exposure, false);
	DB_CHECK(GetSurvivalModifiers(State).Status & ESurvivalStatus::Freezing);
	AdvanceSurvival(State, 1.0, Exposure, true); // into the inn
	DB_CHECK(State.Warmth == 100.f);
}

DB_TEST(Survival_FoodIsAConsumable)
{
	FConsumableEffect Food;
	Food.Satiety = 35.f;
	DB_CHECK(!Food.IsEmpty());
	DB_CHECK(FConsumableEffect().IsEmpty());
}
