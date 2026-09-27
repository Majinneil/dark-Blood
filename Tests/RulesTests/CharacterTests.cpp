#include "TestFramework.h"

#include "DarkBloodRules/CharacterName.h"
#include "DarkBloodRules/Damage.h"
#include "DarkBloodRules/Progression.h"
#include "DarkBloodRules/SkillTree.h"
#include "DarkBloodRules/Stats.h"

using namespace DarkBlood::Rules;

DB_TEST(CharacterName_AcceptsFreeNamesIndependentOfAccount)
{
	DB_CHECK_EQ(ValidateCharacterName("Jin Akagi"), ENameValidation::Ok);
	DB_CHECK_EQ(ValidateCharacterName("Jürgen Weiß"), ENameValidation::Ok);
	DB_CHECK_EQ(ValidateCharacterName("O'Neil"), ENameValidation::Ok);
	DB_CHECK_EQ(ValidateCharacterName("Hana-Mei"), ENameValidation::Ok);
	DB_CHECK_EQ(ValidateCharacterName("赤木 仁"), ENameValidation::Ok);
	DB_CHECK_EQ(ValidateCharacterName("アカギ"), ENameValidation::Ok);
}

DB_TEST(CharacterName_RejectsInvalidNames)
{
	DB_CHECK_EQ(ValidateCharacterName(""), ENameValidation::Empty);
	DB_CHECK_EQ(ValidateCharacterName("J"), ENameValidation::TooShort);
	DB_CHECK_EQ(ValidateCharacterName("Abcdefghijklmnopqrstuvwxy"), ENameValidation::TooLong);
	DB_CHECK_EQ(ValidateCharacterName("Player12345"), ENameValidation::InvalidCharacter);
	DB_CHECK_EQ(ValidateCharacterName("Jin_Akagi"), ENameValidation::InvalidCharacter);
	DB_CHECK_EQ(ValidateCharacterName("-Jin"), ENameValidation::SeparatorAtEdge);
	DB_CHECK_EQ(ValidateCharacterName("Jin--Akagi"), ENameValidation::ConsecutiveSeparators);
	DB_CHECK_EQ(ValidateCharacterName("A B C D"), ENameValidation::TooManyWords);
	DB_CHECK_EQ(ValidateCharacterName("Jin\xC3"), ENameValidation::InvalidUtf8);
	DB_CHECK_EQ(ValidateCharacterName("J\xC0\xAFn"), ENameValidation::InvalidUtf8); // overlong '/'
}

DB_TEST(CharacterName_NormalizeAndCallName)
{
	DB_CHECK_EQ(NormalizeCharacterName("   Jin    Akagi  "), std::string("Jin Akagi"));
	DB_CHECK_EQ(NormalizeCharacterName("\tJin\nAkagi"), std::string("Jin Akagi"));
	DB_CHECK_EQ(GetCallName("Jin Akagi"), std::string("Jin"));
	DB_CHECK_EQ(GetCallName("Jin"), std::string("Jin"));
}

DB_TEST(Progression_LevelsUpAndCarriesOverflow)
{
	FProgressionRules Rules;
	FProgressionState State;
	const int64 Needed = Rules.Curve.XpToNextLevel(1);
	DB_CHECK(Needed > 0);

	FXpGrantResult Result = GrantXp(State, Needed - 1, Rules);
	DB_CHECK_EQ(State.Level, 1);
	DB_CHECK_EQ(Result.LevelsGained, 0);

	Result = GrantXp(State, 11, Rules);
	DB_CHECK_EQ(State.Level, 2);
	DB_CHECK_EQ(State.XpIntoLevel, int64(10));
	DB_CHECK_EQ(Result.LevelsGained, 1);
	DB_CHECK(IsProgressionConsistent(State, Rules));
}

DB_TEST(Progression_MultiLevelGrantAwardsIntervalSkillPoints)
{
	FProgressionRules Rules;
	FProgressionState State;
	const FXpGrantResult Result = GrantXp(State, Rules.Curve.TotalXpForLevel(10), Rules);
	DB_CHECK_EQ(State.Level, 10);
	DB_CHECK_EQ(Result.LevelsGained, 9);
	DB_CHECK_EQ(Result.SkillPointsGained, 2); // levels 5 and 10
	DB_CHECK_EQ(State.UnspentSkillPoints, 2);
	DB_CHECK(IsProgressionConsistent(State, Rules));
}

DB_TEST(Progression_CapsAtMaxLevel)
{
	FProgressionRules Rules;
	FProgressionState State;
	const FXpGrantResult Result = GrantXp(State, Rules.Curve.TotalXpForLevel(100) + 123456, Rules);
	DB_CHECK_EQ(State.Level, 100);
	DB_CHECK_EQ(State.XpIntoLevel, int64(0));
	DB_CHECK_EQ(Result.XpApplied, Rules.Curve.TotalXpForLevel(100));
	DB_CHECK_EQ(GrantXp(State, 1000, Rules).XpApplied, int64(0));
	DB_CHECK(IsProgressionConsistent(State, Rules));
}

DB_TEST(Progression_RejectsNegativeAndDetectsTampering)
{
	FProgressionRules Rules;
	FProgressionState State;
	DB_CHECK_EQ(GrantXp(State, -500, Rules).XpApplied, int64(0));
	DB_CHECK_EQ(GrantSkillPoints(State, -3, Rules), 0);
	DB_CHECK(!SpendSkillPoints(State, 1));

	FProgressionState Tampered;
	Tampered.Level = 50; // level without the matching total XP
	DB_CHECK(!IsProgressionConsistent(Tampered, Rules));
	Tampered.TotalXp = Rules.Curve.TotalXpForLevel(50);
	DB_CHECK(IsProgressionConsistent(Tampered, Rules));
	Tampered.UnspentSkillPoints = 5; // points that were never earned
	DB_CHECK(!IsProgressionConsistent(Tampered, Rules));
}

DB_TEST(Stats_DerivedStatsAndPower)
{
	FClassGrowth Growth;
	Growth.BaseAtLevel1.Vitality = 10.f;
	Growth.PerLevel.Vitality = 2.f;
	const FPrimaryStats Stats = ComputePrimaryStats(Growth, 11);
	DB_CHECK_NEAR(Stats.Vitality, 30.f, 0.001);

	const FDerivedStatFormula Formula;
	const FDerivedStats Derived = ComputeDerivedStats(Stats, Formula);
	DB_CHECK_NEAR(Derived.MaxHealth, Formula.BaseHealth + 30.f * Formula.HealthPerVitality, 0.001);
	DB_CHECK(Derived.CritChance <= Formula.MaxCritChance);

	DB_CHECK_EQ(ComputePowerRating(13, 13.f), 13);
	DB_CHECK_EQ(ComputePowerRating(1, 0.f), 1);
}

DB_TEST(Stats_DangerTiersNeverBlockButInform)
{
	DB_CHECK_EQ(AssessDanger(13, 50, 60), EDangerTier::Extreme);
	DB_CHECK_EQ(AssessDanger(40, 50, 60), EDangerTier::Dangerous);
	DB_CHECK_EQ(AssessDanger(47, 50, 60), EDangerTier::Challenging);
	DB_CHECK_EQ(AssessDanger(55, 50, 60), EDangerTier::Appropriate);
	DB_CHECK_EQ(AssessDanger(75, 50, 60), EDangerTier::Trivial);
	DB_CHECK_EQ(AssessDanger(55, 60, 50), EDangerTier::Appropriate); // swapped range tolerated
}

DB_TEST(Damage_ArmorResistanceCritBlockParry)
{
	FDamageRequest Hit;
	Hit.BaseDamage = 100.f;
	Hit.AttackerLevel = 1;

	FDefenseSnapshot Naked;
	DB_CHECK_NEAR(ResolveDamage(Hit, Naked).FinalDamage, 100.f, 0.01);

	FDefenseSnapshot Armored;
	Armored.Armor = 50.f; // equal to the constant at level 1 -> 50% mitigation
	DB_CHECK_NEAR(ResolveDamage(Hit, Armored).FinalDamage, 50.f, 0.01);

	FDamageRequest Crit = Hit;
	Crit.CritChance = 0.5f;
	Crit.CritRoll = 0.1f;
	const FDamageResult CritResult = ResolveDamage(Crit, Naked);
	DB_CHECK(CritResult.bCritical);
	DB_CHECK_NEAR(CritResult.FinalDamage, 150.f, 0.01);

	FDamageRequest Fire = Hit;
	Fire.Type = EDamageType::Fire;
	FDefenseSnapshot FireResistant;
	FireResistant.Resistances[static_cast<int>(EDamageType::Fire)] = 5.f; // clamped to MaxResistance (0.8)
	DB_CHECK_NEAR(ResolveDamage(Fire, FireResistant).FinalDamage, 20.f, 0.01);

	FDefenseSnapshot Blocking;
	Blocking.State = EDefenseState::Blocking;
	const FDamageResult Blocked = ResolveDamage(Hit, Blocking);
	DB_CHECK(Blocked.bBlocked);
	DB_CHECK_NEAR(Blocked.FinalDamage, 30.f, 0.01);
	DB_CHECK(Blocked.BlockStaminaCost > 0.f);

	FDefenseSnapshot Parry;
	Parry.State = EDefenseState::PerfectParry;
	const FDamageResult Parried = ResolveDamage(Hit, Parry);
	DB_CHECK(Parried.bParried);
	DB_CHECK_NEAR(Parried.FinalDamage, 0.f, 0.001);

	FDamageRequest Unparryable = Hit;
	Unparryable.bCanBeParried = false;
	DB_CHECK(!ResolveDamage(Unparryable, Parry).bParried);

	FDefenseSnapshot Dodging;
	Dodging.State = EDefenseState::Invulnerable;
	DB_CHECK(ResolveDamage(Hit, Dodging).bImmune);
}

DB_TEST(SkillTree_UnlockRules)
{
	FSkillTreeDefinition Tree;
	Tree.Id = "Shadowrunner";
	Tree.Nodes.push_back({"ShadowMark", 1, 3, 1, {}});
	Tree.Nodes.push_back({"ShadowMark_MultiMark", 2, 1, 10, {"ShadowMark"}});

	FSkillTreeState State;
	FProgressionState Progression;
	DB_CHECK_EQ(UnlockSkill(Tree, State, Progression, "ShadowMark"), ESkillUnlockResult::NotEnoughPoints);

	Progression.UnspentSkillPoints = 5;
	Progression.TotalSkillPointsEarned = 5;
	DB_CHECK_EQ(UnlockSkill(Tree, State, Progression, "Unknown"), ESkillUnlockResult::UnknownNode);
	DB_CHECK_EQ(UnlockSkill(Tree, State, Progression, "ShadowMark_MultiMark"), ESkillUnlockResult::LevelTooLow);
	Progression.Level = 10;
	DB_CHECK_EQ(UnlockSkill(Tree, State, Progression, "ShadowMark_MultiMark"), ESkillUnlockResult::MissingPrerequisite);
	DB_CHECK_EQ(UnlockSkill(Tree, State, Progression, "ShadowMark"), ESkillUnlockResult::Ok);
	DB_CHECK_EQ(UnlockSkill(Tree, State, Progression, "ShadowMark_MultiMark"), ESkillUnlockResult::Ok);
	DB_CHECK_EQ(Progression.UnspentSkillPoints, 2);
	DB_CHECK_EQ(UnlockSkill(Tree, State, Progression, "ShadowMark_MultiMark"), ESkillUnlockResult::MaxRankReached);
	DB_CHECK_EQ(CountSpentPoints(Tree, State), 3);
}
