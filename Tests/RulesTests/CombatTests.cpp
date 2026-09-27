#include "TestFramework.h"

#include "DarkBloodRules/Combat.h"

using namespace DarkBlood::Rules;

DB_TEST(Combat_ComboChainAdvancesInsideWindowAndResets)
{
	// Three-hit chain, 0.5 s window.
	DB_CHECK_EQ(NextComboStep(0, 3, 0.2f, 0.5f), 1);
	DB_CHECK_EQ(NextComboStep(1, 3, 0.5f, 0.5f), 2); // window end is inclusive
	DB_CHECK_EQ(NextComboStep(2, 3, 0.1f, 0.5f), 0); // chain finished -> restart
	DB_CHECK_EQ(NextComboStep(1, 3, 0.51f, 0.5f), 0); // missed the window
	DB_CHECK_EQ(NextComboStep(0, 1, 0.1f, 0.5f), 0); // single-hit move
	DB_CHECK_EQ(NextComboStep(0, 0, 0.1f, 0.5f), 0);
	DB_CHECK_EQ(NextComboStep(0, 3, -0.1f, 0.5f), 0); // invalid time
}

DB_TEST(Combat_ChargeScalesBetweenMinAndFull)
{
	const FChargeRules Rules;
	DB_CHECK(!EvaluateCharge(0.1f, Rules).bCharged);
	DB_CHECK_NEAR(EvaluateCharge(0.1f, Rules).DamageMultiplier, 1.f, 0.0001);

	const FChargeResult Min = EvaluateCharge(Rules.MinChargeSeconds, Rules);
	DB_CHECK(Min.bCharged);
	DB_CHECK_NEAR(Min.ChargeFraction, 0.f, 0.0001);
	DB_CHECK_NEAR(Min.DamageMultiplier, 1.f, 0.0001);

	const float Half = (Rules.MinChargeSeconds + Rules.FullChargeSeconds) * 0.5f;
	DB_CHECK_NEAR(EvaluateCharge(Half, Rules).DamageMultiplier, 1.75f, 0.001);

	const FChargeResult Full = EvaluateCharge(10.f, Rules); // holding longer never exceeds the maximum
	DB_CHECK_NEAR(Full.ChargeFraction, 1.f, 0.0001);
	DB_CHECK_NEAR(Full.DamageMultiplier, Rules.MaxDamageMultiplier, 0.0001);
	DB_CHECK_NEAR(Full.PoiseMultiplier, Rules.MaxPoiseMultiplier, 0.0001);
}

DB_TEST(Combat_AttackArcRespectsRangeAngleAndTargetSize)
{
	// 200 cm range, 60 degree half angle.
	DB_CHECK(IsInsideAttackArc(150.f, 0.f, 200.f, 60.f, 0.f));
	DB_CHECK(!IsInsideAttackArc(250.f, 0.f, 200.f, 60.f, 0.f));
	DB_CHECK(IsInsideAttackArc(250.f, 0.f, 200.f, 60.f, 60.f)); // body edge within reach
	DB_CHECK(!IsInsideAttackArc(-100.f, 0.f, 200.f, 60.f, 0.f)); // behind
	DB_CHECK(!IsInsideAttackArc(50.f, 150.f, 200.f, 60.f, 0.f)); // ~72 degrees to the side
	DB_CHECK(IsInsideAttackArc(50.f, 150.f, 200.f, 60.f, 60.f)); // ...but a wide body still gets clipped
	DB_CHECK(IsInsideAttackArc(0.f, 10.f, 200.f, 60.f, 40.f)); // overlapping capsules always hit
}

DB_TEST(Combat_StaminaActionsNeedPositiveStaminaAndNeverGoNegative)
{
	DB_CHECK(CanStartStaminaAction(1.f, 30.f));
	DB_CHECK(!CanStartStaminaAction(0.f, 30.f));
	DB_CHECK(CanStartStaminaAction(0.f, 0.f)); // free actions always work
	DB_CHECK_NEAR(StaminaAfterCost(20.f, 30.f), 0.f, 0.0001);
	DB_CHECK_NEAR(StaminaAfterCost(50.f, 30.f), 20.f, 0.0001);
	DB_CHECK_NEAR(StaminaAfterCost(50.f, -5.f), 50.f, 0.0001);
}

DB_TEST(Combat_PoiseFlinchStaggerKnockdown)
{
	// Poise holds -> flinch only.
	FPoiseResult Hit = ApplyPoiseDamage(50.f, 50.f, 20.f, false);
	DB_CHECK_EQ(Hit.Reaction, EHitReaction::Flinch);
	DB_CHECK_NEAR(Hit.NewPoise, 30.f, 0.0001);
	DB_CHECK(!InterruptsAction(Hit.Reaction));

	// Accumulated damage breaks poise -> stagger and full refill.
	Hit = ApplyPoiseDamage(Hit.NewPoise, 50.f, 30.f, false);
	DB_CHECK_EQ(Hit.Reaction, EHitReaction::Stagger);
	DB_CHECK_NEAR(Hit.NewPoise, 50.f, 0.0001);
	DB_CHECK(InterruptsAction(Hit.Reaction));

	// One huge hit (>= 75 % of max) breaking poise knocks down; the same hit on full poise only flinches.
	DB_CHECK_EQ(ApplyPoiseDamage(30.f, 50.f, 40.f, false).Reaction, EHitReaction::Knockdown);
	DB_CHECK_EQ(ApplyPoiseDamage(50.f, 50.f, 40.f, false).Reaction, EHitReaction::Flinch);
	// Forced knockdown ignores poise.
	DB_CHECK_EQ(ApplyPoiseDamage(50.f, 50.f, 1.f, true).Reaction, EHitReaction::Knockdown);
	// Zero poise damage does nothing.
	DB_CHECK_EQ(ApplyPoiseDamage(50.f, 50.f, 0.f, false).Reaction, EHitReaction::None);
	// Targets without poise (MaxPoise 0) stagger on every hit.
	DB_CHECK_EQ(ApplyPoiseDamage(0.f, 0.f, 5.f, false).Reaction, EHitReaction::Stagger);

	DB_CHECK_NEAR(ReactionDurationSeconds(EHitReaction::Flinch), 0.f, 0.0001);
	DB_CHECK(ReactionDurationSeconds(EHitReaction::Knockdown) > ReactionDurationSeconds(EHitReaction::Stagger));
	DB_CHECK_EQ(std::string(ToString(EHitReaction::ParriedStagger)), std::string("ParriedStagger"));
}

DB_TEST(Combat_LockOnPrefersCenteredTargetsAndBreaksAtRange)
{
	const FLockOnRules Rules;
	DB_CHECK(ScoreLockOnCandidate(500.f, 10.f, true, Rules) >= 0.f);
	DB_CHECK(ScoreLockOnCandidate(500.f, 10.f, false, Rules) < 0.f); // no line of sight
	DB_CHECK(ScoreLockOnCandidate(Rules.MaxDistance + 1.f, 0.f, true, Rules) < 0.f);
	DB_CHECK(ScoreLockOnCandidate(500.f, Rules.MaxAngleDegrees + 1.f, true, Rules) < 0.f);
	// A centered target further away beats a close one at the edge of the cone.
	DB_CHECK(ScoreLockOnCandidate(900.f, 2.f, true, Rules) < ScoreLockOnCandidate(400.f, 40.f, true, Rules));
	// Same angle: closer wins.
	DB_CHECK(ScoreLockOnCandidate(300.f, 5.f, true, Rules) < ScoreLockOnCandidate(600.f, 5.f, true, Rules));

	DB_CHECK(!ShouldBreakLockOn(1000.f, true, Rules));
	DB_CHECK(ShouldBreakLockOn(Rules.BreakDistance + 1.f, true, Rules));
	DB_CHECK(ShouldBreakLockOn(100.f, false, Rules));
}
