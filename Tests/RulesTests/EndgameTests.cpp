#include "TestFramework.h"
#include "TestCatalog.h"

#include "DarkBloodRules/Endgame.h"
#include "DarkBloodRules/Records.h"
#include "DarkBloodRules/Region.h"
#include "DarkBloodRules/WorldState.h"

using namespace DarkBlood::Rules;

DB_TEST(Endgame_CycleScaleGrowsAndCaps)
{
	const FEndgameScale First = GetCycleScale(0);
	DB_CHECK_NEAR(First.EnemyHealth, 1.f, 1e-6);
	DB_CHECK_EQ(First.EnemyLevelBonus, 0);
	DB_CHECK(GetCycleLabel(0).empty());

	const FEndgameScale Second = GetCycleScale(1);
	DB_CHECK_NEAR(Second.EnemyHealth, 1.6f, 1e-6);
	DB_CHECK_NEAR(Second.EnemyDamage, 1.3f, 1e-6);
	DB_CHECK_NEAR(Second.Experience, 1.5f, 1e-6);
	DB_CHECK_EQ(Second.EnemyLevelBonus, 10);
	DB_CHECK_EQ(GetCycleLabel(1), std::string("NG+"));
	DB_CHECK_EQ(GetCycleLabel(3), std::string("NG+3"));

	// NG+7 is the summit: later cycles keep its scale.
	DB_CHECK_NEAR(GetCycleScale(7).EnemyHealth, GetCycleScale(12).EnemyHealth, 1e-6);
	DB_CHECK(GetCycleScale(7).EnemyHealth > GetCycleScale(6).EnemyHealth);
	DB_CHECK_NEAR(GetCycleScale(-3).EnemyHealth, 1.f, 1e-6);
}

DB_TEST(Endgame_NewCycleResetsTheWorldButKeepsMemory)
{
	FWorldState World;
	World.AddRegion("Capital", ERegionKind::Capital);
	World.AddRegion("Region01", ERegionKind::VassalRegion);
	World.AddRegion("TheEnd", ERegionKind::FinalRegion).VassalCount = 2;
	World.MarkVassalDefeated("Region01", "V_Akakage");
	World.RecordBossDefeat("V_Akakage");
	World.RecordBossDefeat("B_DemonKing");
	World.MarkDungeonCleared("Dungeon_01");
	World.Advance(5.0);
	const double Hours = World.Clock.TotalHours;

	DB_CHECK(!World.BeginNewCycle()); // not before the demon king fell
	DB_CHECK_EQ(World.Cycle, 0);

	World.PurifyWorld();
	DB_CHECK(World.CanBeginNewCycle());
	DB_CHECK(World.BeginNewCycle());
	DB_CHECK_EQ(World.Cycle, 1);
	for (const char* Id : {"Region01", "TheEnd"})
	{
		const FRegionState* Region = World.FindRegion(Id);
		DB_CHECK(Region->Control == ERegionControl::Occupied && Region->DemonInfluence == 1.f);
		DB_CHECK(!Region->bVassalDefeated && Region->VassalsDefeated == 0 && Region->LiberatedAtHours < 0.0);
		DB_CHECK_EQ(GetRegionalPackBudget(*Region, true), 3);
	}
	DB_CHECK_EQ(World.FindRegion("TheEnd")->VassalCount, 2);
	DB_CHECK(World.DefeatedBosses.empty());
	DB_CHECK(!World.IsPurified() && World.StoryFlags.empty());
	DB_CHECK(World.Dungeons.empty());
	DB_CHECK(World.SharedQuests.GetAll().empty());
	DB_CHECK_NEAR(World.Clock.TotalHours, Hours, 1e-9);
	// The Hall of Echoes remembers every boss of earlier cycles.
	DB_CHECK(World.RememberedBosses.count("V_Akakage") == 1 && World.RememberedBosses.count("B_DemonKing") == 1);
	DB_CHECK(!World.BeginNewCycle()); // the new cycle must be won again first
}

DB_TEST(Endgame_EchoesGrowStrongerPerVictory)
{
	FWorldState World;
	DB_CHECK_EQ(World.GetEchoRank("V_Akakage"), 0);
	DB_CHECK_EQ(World.RecordEchoVictory("V_Akakage"), 1);
	DB_CHECK_EQ(World.RecordEchoVictory("V_Akakage"), 2);
	DB_CHECK_EQ(World.GetEchoRank("V_Akakage"), 2);
	DB_CHECK_EQ(World.GetEchoRank("V_Yukimaru"), 0);

	const FEndgameScale Rank1 = GetEchoScale(0, 1);
	const FEndgameScale Rank3 = GetEchoScale(0, 3);
	DB_CHECK_NEAR(Rank1.EnemyHealth, 1.35f, 1e-6);
	DB_CHECK(Rank3.EnemyHealth > Rank1.EnemyHealth && Rank3.EnemyDamage > Rank1.EnemyDamage);
	// Echoes in NG+ stack on the cycle.
	DB_CHECK_NEAR(GetEchoScale(1, 1).EnemyHealth, 1.6f * 1.35f, 1e-5);
	DB_CHECK_NEAR(GetEchoScale(0, 99).EnemyHealth, GetEchoScale(0, 10).EnemyHealth, 1e-6);
}

DB_TEST(Endgame_AbyssFloorsDeepenDeterministically)
{
	const FAbyssFloor One = GetAbyssFloor(1, 0);
	DB_CHECK_EQ(One.Rooms, 4);
	DB_CHECK_EQ(One.EnemiesPerRoom, 2);
	DB_CHECK_NEAR(One.EnemyHealth, 1.f, 1e-6);
	DB_CHECK(!One.bGuardianFloor);
	DB_CHECK(GetAbyssFloor(5, 0).bGuardianFloor && GetAbyssFloor(10, 0).bGuardianFloor && !GetAbyssFloor(11, 0).bGuardianFloor);

	const FAbyssFloor Deep = GetAbyssFloor(30, 0);
	DB_CHECK_EQ(Deep.Rooms, 9);       // capped
	DB_CHECK_EQ(Deep.EnemiesPerRoom, 6);
	DB_CHECK(Deep.EnemyHealth > One.EnemyHealth && Deep.EnemyLevelBonus == 29);
	DB_CHECK_NEAR(GetAbyssFloor(80, 0).RarityBonus, 0.3f, 1e-6);

	// Same floor, same layout; another floor or cycle, another one.
	DB_CHECK_EQ(GetAbyssFloor(7, 0).Seed, GetAbyssFloor(7, 0).Seed);
	DB_CHECK(GetAbyssFloor(7, 0).Seed != GetAbyssFloor(8, 0).Seed);
	DB_CHECK(GetAbyssFloor(7, 0).Seed != GetAbyssFloor(7, 1).Seed);
	DB_CHECK(GetAbyssFloor(1, 2).EnemyHealth > GetAbyssFloor(1, 0).EnemyHealth);
	DB_CHECK_EQ(GetAbyssFloor(0, 0).Depth, 1);
}

DB_TEST(Save_WorldRoundTripKeepsEndgame)
{
	FWorldRecord Record;
	Record.WorldId = "world-ng";
	Record.World.AddRegion("Region01", ERegionKind::VassalRegion);
	Record.World.Cycle = 2;
	Record.World.RememberedBosses = {"V_Akakage", "V_Yukimaru"};
	Record.World.RecordEchoVictory("V_Akakage");
	Record.World.RecordEchoVictory("V_Akakage");
	Record.World.AbyssDeepest = 17;

	const std::vector<uint8> Bytes = SerializeWorld(Record);
	FWorldRecord Loaded;
	DB_CHECK_EQ(DeserializeWorld(Bytes.data(), Bytes.size(), Loaded), ELoadResult::Ok);
	DB_CHECK_EQ(Loaded.World.Cycle, 2);
	DB_CHECK(Loaded.World.RememberedBosses == Record.World.RememberedBosses);
	DB_CHECK_EQ(Loaded.World.GetEchoRank("V_Akakage"), 2);
	DB_CHECK_EQ(Loaded.World.AbyssDeepest, 17);
	DB_CHECK(SerializeWorld(Loaded) == Bytes);
}

DB_TEST(Endgame_LootFavoursRareItems)
{
	FLootTable Table;
	Table.Id = "LT_Test";
	Table.Entries = {{"Katana_Basic", 10, 1, 1}, {"Nodachi_Demon", 10, 1, 1}};
	Table.NothingWeight = 20;
	Table.CurrencyMin = 10;
	Table.CurrencyMax = 20;

	const FLootTable Same = ApplyEndgameLoot(Table, 0.f, TestCatalog());
	DB_CHECK_EQ(Same.Entries[0].Weight, 10);
	DB_CHECK_EQ(Same.NothingWeight, 20);

	const FLootTable Bonus = ApplyEndgameLoot(Table, 0.3f, TestCatalog());
	// Common katana keeps its share, the demonic nodachi (step 5) grows 2.5x, "nothing" shrinks.
	DB_CHECK_EQ(Bonus.Entries[0].Weight, 1000);
	DB_CHECK_EQ(Bonus.Entries[1].Weight, 2500);
	DB_CHECK_EQ(Bonus.NothingWeight, 1400);
	DB_CHECK_EQ(Bonus.CurrencyMin, 16);
	DB_CHECK_EQ(Bonus.CurrencyMax, 32);
	DB_CHECK(Bonus.Rolls == Table.Rolls);
}
