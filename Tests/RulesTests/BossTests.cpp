#include "TestFramework.h"

#include "DarkBloodRules/Boss.h"

using namespace DarkBlood::Rules;

DB_TEST(Boss_CoopScalingAddsMechanicsNotOnlyHealth)
{
	const FBossScaling Solo = GetBossScaling(1);
	const FBossScaling Duo = GetBossScaling(2);
	const FBossScaling Party = GetBossScaling(4);
	DB_CHECK(Solo.HealthMultiplier == 1.f && Solo.ExtraAdds == 0 && !Solo.bSplitAttention && !Solo.bAreaPressure);
	DB_CHECK(Duo.bSplitAttention && !Duo.bAreaPressure && Duo.ExtraAdds == 1);
	DB_CHECK(Party.bAreaPressure && Party.ExtraAdds == 3 && Party.CooldownMultiplier < Duo.CooldownMultiplier);
	// Health is the last lever: four players get far less than four times the health.
	DB_CHECK(Party.HealthMultiplier < 2.5f);
	DB_CHECK(GetBossScaling(9).HealthMultiplier == Party.HealthMultiplier);
	DB_CHECK(GetBossScaling(0).HealthMultiplier == 1.f);
}

DB_TEST(Boss_PhasesNeverFallBack)
{
	const std::vector<float> Thresholds = {0.66f, 0.33f};
	DB_CHECK_EQ(EvaluateBossPhase(1.0f, 0, Thresholds), 0);
	DB_CHECK_EQ(EvaluateBossPhase(0.5f, 0, Thresholds), 1);
	DB_CHECK_EQ(EvaluateBossPhase(0.2f, 1, Thresholds), 2);
	DB_CHECK_EQ(EvaluateBossPhase(0.9f, 2, Thresholds), 2); // healed: still phase 2
	DB_CHECK_EQ(EvaluateBossPhase(0.0f, 0, {}), 0);
}

DB_TEST(Boss_EnrageAfterTime)
{
	DB_CHECK(GetEnrageMultiplier(100.0, 300.0) == 1.f);
	DB_CHECK(GetEnrageMultiplier(300.0, 300.0) == 1.25f);
	DB_CHECK(GetEnrageMultiplier(345.0, 300.0) == 1.5f);
	DB_CHECK(GetEnrageMultiplier(3000.0, 300.0) == 2.f);
	DB_CHECK(GetEnrageMultiplier(3000.0, 0.0) == 1.f);
}

DB_TEST(Boss_SignatureComesFasterInLaterPhases)
{
	DB_CHECK(GetSignatureCooldown(0, 1) == 18.f);
	DB_CHECK(GetSignatureCooldown(1, 1) == 15.f);
	DB_CHECK(GetSignatureCooldown(5, 1) == 9.f); // floor
	DB_CHECK(GetSignatureCooldown(0, 4) < GetSignatureCooldown(0, 1)); // co-op cooldowns
	DB_CHECK(GetSignatureCooldown(-3, 1) == 18.f);
}
