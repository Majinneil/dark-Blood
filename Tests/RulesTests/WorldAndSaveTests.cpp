#include "TestFramework.h"

#include "TestCatalog.h"

#include "DarkBloodRules/Archive.h"
#include "DarkBloodRules/Records.h"

using namespace DarkBlood::Rules;

namespace
{
	FQuestDatabase MakeQuests()
	{
		FQuestDatabase Database;

		FQuestDefinition Intro;
		Intro.Id = "MQ01_TheKingsSummons";
		Intro.Category = EQuestCategory::Main;
		Intro.Scope = EQuestScope::Shared;
		Intro.bSequential = true;
		Intro.Objectives = {
			{"TalkKing", EObjectiveKind::Talk, "NPC_King", 1, false},
			{"KillOni", EObjectiveKind::Kill, "Oni_Grunt", 3, false},
			{"Bonus", EObjectiveKind::Collect, "RiceBall", 2, true},
		};
		Intro.Reward.Xp = 500;
		Intro.Reward.SkillPoints = 1;
		Intro.Reward.StoryFlags = {"Story.IntroDone"};
		Database.Add(Intro);

		FQuestDefinition Bounty;
		Bounty.Id = "BTY_BloodHound";
		Bounty.Category = EQuestCategory::Bounty;
		Bounty.RequiredStoryFlags = {"Story.IntroDone"};
		Bounty.bAutoComplete = false;
		Bounty.Objectives = {{"Kill", EObjectiveKind::Kill, "BloodHound", 1, false}};
		Database.Add(Bounty);
		return Database;
	}
}

DB_TEST(Quest_SequentialProgressAndCompletion)
{
	const FQuestDatabase Database = MakeQuests();
	FQuestLog Log;
	DB_CHECK_EQ(Log.Start(Database, "MQ01_TheKingsSummons", {}), EQuestResult::Ok);
	DB_CHECK_EQ(Log.Start(Database, "MQ01_TheKingsSummons", {}), EQuestResult::AlreadyActive);

	// Kills before talking to the king do not count (sequential).
	DB_CHECK(Log.ApplyEvent(Database, {EObjectiveKind::Kill, "Oni_Grunt", 1}).empty());
	DB_CHECK_EQ(Log.ApplyEvent(Database, {EObjectiveKind::Talk, "NPC_King", 1}).size(), size_t(1));
	Log.ApplyEvent(Database, {EObjectiveKind::Kill, "Oni_Grunt", 2});
	const std::vector<FQuestUpdate> Final = Log.ApplyEvent(Database, {EObjectiveKind::Kill, "Oni_Grunt", 5});
	DB_CHECK(!Final.empty());
	DB_CHECK_EQ(Final.back().NewStatus, EQuestStatus::Completed); // optional objective not required
	DB_CHECK_EQ(Log.Find("MQ01_TheKingsSummons")->ObjectiveCounts[1], 3); // clamped
	DB_CHECK_EQ(Log.Abandon(Database, "MQ01_TheKingsSummons"), EQuestResult::CannotAbandon);
	DB_CHECK_EQ(Log.Start(Database, "MQ01_TheKingsSummons", {}), EQuestResult::AlreadyCompleted);
}

DB_TEST(Quest_StoryFlagsAndTurnIn)
{
	const FQuestDatabase Database = MakeQuests();
	FQuestLog Log;
	FStoryFlags Flags;
	DB_CHECK_EQ(Log.Start(Database, "BTY_BloodHound", Flags), EQuestResult::MissingStoryFlag);
	Flags.insert("Story.IntroDone");
	DB_CHECK_EQ(Log.Start(Database, "BTY_BloodHound", Flags), EQuestResult::Ok);
	DB_CHECK_EQ(Log.TurnIn(Database, "BTY_BloodHound"), EQuestResult::NotReady);
	Log.ApplyEvent(Database, {EObjectiveKind::Kill, "BloodHound", 1});
	DB_CHECK_EQ(Log.Find("BTY_BloodHound")->Status, EQuestStatus::ReadyToTurnIn);
	DB_CHECK_EQ(Log.TurnIn(Database, "BTY_BloodHound"), EQuestResult::Ok);
	DB_CHECK(Log.IsCompleted("BTY_BloodHound"));
}

DB_TEST(World_VassalsLiberationAndFinalRegion)
{
	FWorldState World;
	World.AddRegion("Capital", ERegionKind::Capital);
	for (int Index = 1; Index <= NumVassalRegions; ++Index)
	{
		World.AddRegion("Region" + std::to_string(Index), ERegionKind::VassalRegion);
	}
	World.AddRegion("TheEnd", ERegionKind::FinalRegion);

	DB_CHECK_EQ(World.FindRegion("Capital")->DemonInfluence, 0.f);
	DB_CHECK(World.MarkMidBossDefeated("Region1"));
	DB_CHECK_EQ(World.FindRegion("Region1")->Control, ERegionControl::Contested);
	DB_CHECK(World.MarkVassalDefeated("Region1"));
	DB_CHECK(!World.MarkVassalDefeated("Region1")); // co-op double kill is idempotent
	DB_CHECK(!World.MarkVassalDefeated("Capital"));
	DB_CHECK(!World.IsFinalRegionOpen());

	for (int Index = 2; Index <= NumVassalRegions; ++Index)
	{
		World.MarkVassalDefeated("Region" + std::to_string(Index));
	}
	DB_CHECK_EQ(World.CountDefeatedVassals(), NumVassalRegions);
	DB_CHECK(World.IsFinalRegionOpen());
}

DB_TEST(World_RecoveryIsTimeStepIndependent)
{
	FWorldState A;
	FWorldState B;
	for (FWorldState* World : {&A, &B})
	{
		World->AddRegion("Frost", ERegionKind::VassalRegion);
		World->MarkVassalDefeated("Frost");
	}
	A.Advance(48.0);
	for (int Step = 0; Step < 480; ++Step)
	{
		B.Advance(0.1);
	}
	DB_CHECK_NEAR(A.FindRegion("Frost")->DemonInfluence, B.FindRegion("Frost")->DemonInfluence, 1e-4);
	DB_CHECK(A.FindRegion("Frost")->DemonInfluence < 1.f);
	DB_CHECK(A.FindRegion("Frost")->DemonInfluence > 0.05f);
	DB_CHECK_EQ(A.Clock.GetDay(), 3);
}

DB_TEST(World_ClockDayNight)
{
	FWorldClock Clock;
	DB_CHECK(!Clock.IsNight()); // 08:00
	Clock.TotalHours = 22.0;
	DB_CHECK(Clock.IsNight());
	Clock.TotalHours = 24.0 + 4.0;
	DB_CHECK(Clock.IsNight());
	DB_CHECK_EQ(Clock.GetDay(), 2);
}

DB_TEST(Save_CharacterRoundTrip)
{
	const FItemCatalog& Catalog = TestCatalog();
	FCharacterAppearance Look;
	Look.BodyType = EBodyType::TypeB;
	Look.HairColor = 0xAA2233FFu;
	Look.Scars = {2, 7};
	Look.Morphs["JawWidth"] = -0.25f;

	FCharacterRecord Record = MakeNewCharacter("7f1c-guid", "  Jin   Akagi ", "Warrior", Look);
	DB_CHECK_EQ(Record.Name, std::string("Jin Akagi"));
	GrantXp(Record.Progression, 5000, FProgressionRules());
	Record.Currency = 1234;
	Record.Inventory.Add(Catalog, {"Bag_Small", 1});
	Record.Inventory.EquipBag(Catalog, {0, 0});
	Record.Inventory.Add(Catalog, {"Tamahagane", 120});
	Record.Inventory.Add(Catalog, {"KingsSeal", 1});
	Record.Inventory.Add(Catalog, {"Katana_Basic", 1, 77, 90});
	EquipFromInventory(Record.Inventory, Record.Equipment, Catalog, {0, 2}, EEquipSlot::MainHand,
		{Record.Progression.Level, Record.ClassId});
	Record.Skills.Ranks["Iaido"] = 1;
	Record.Progression.TotalSkillPointsEarned += 1; // earned by a boss, spent on Iaido
	Record.DiscoveredRegions = {"Capital", "Region1"};
	Record.Titles = {"Title.HeroOfTime"};
	Record.RespawnPointId = "Tavern_Capital_RedLantern";
	Record.PlayTimeSeconds = 3600;
	Record.PendingDeliveries.push_back({"Tamahagane", 7});

	const std::vector<uint8> Bytes = SerializeCharacter(Record);
	FCharacterRecord Loaded;
	DB_CHECK_EQ(DeserializeCharacter(Bytes.data(), Bytes.size(), Loaded), ELoadResult::Ok);
	DB_CHECK_EQ(Loaded.Name, Record.Name);
	DB_CHECK_EQ(Loaded.Appearance.HairColor, Look.HairColor);
	DB_CHECK(Loaded.Appearance.Morphs == Look.Morphs);
	DB_CHECK_EQ(Loaded.Progression.Level, Record.Progression.Level);
	DB_CHECK_EQ(Loaded.Progression.TotalXp, Record.Progression.TotalXp);
	DB_CHECK_EQ(Loaded.Currency, int64(1234));
	DB_CHECK(Loaded.Inventory.CollectAllItems() == Record.Inventory.CollectAllItems());
	DB_CHECK_EQ(Loaded.Inventory.TotalCapacity(), Record.Inventory.TotalCapacity());
	DB_CHECK(Loaded.Equipment.Get(EEquipSlot::MainHand) == Record.Equipment.Get(EEquipSlot::MainHand));
	DB_CHECK(Loaded.DiscoveredRegions == Record.DiscoveredRegions);
	DB_CHECK_EQ(Loaded.Skills.GetRank("Iaido"), 1);
	DB_CHECK(Loaded.PendingDeliveries == Record.PendingDeliveries);
	DB_CHECK(SerializeCharacter(Loaded) == Bytes); // stable byte-for-byte

	const std::vector<std::string> Issues = ValidateCharacterRecord(Loaded, Catalog);
	for (const std::string& Text : Issues)
	{
		std::printf("    issue: %s\n", Text.c_str());
	}
	DB_CHECK(Issues.empty());
}

DB_TEST(Save_DetectsCorruptionAndFutureVersions)
{
	FCharacterRecord Record = MakeNewCharacter("id", "Jin", "Monk", {});
	std::vector<uint8> Bytes = SerializeCharacter(Record);
	FCharacterRecord Loaded;

	std::vector<uint8> Flipped = Bytes;
	Flipped.back() ^= 0x01;
	DB_CHECK_EQ(DeserializeCharacter(Flipped.data(), Flipped.size(), Loaded), ELoadResult::ChecksumMismatch);

	std::vector<uint8> Truncated(Bytes.begin(), Bytes.end() - 3);
	DB_CHECK_EQ(DeserializeCharacter(Truncated.data(), Truncated.size(), Loaded), ELoadResult::Corrupt);

	std::vector<uint8> Future = Bytes;
	Future[4] = 99; // version field
	DB_CHECK_EQ(DeserializeCharacter(Future.data(), Future.size(), Loaded), ELoadResult::UnsupportedVersion);

	const std::vector<uint8> World = SerializeWorld({});
	DB_CHECK_EQ(DeserializeCharacter(World.data(), World.size(), Loaded), ELoadResult::BadMagic);
	DB_CHECK_EQ(DeserializeCharacter(nullptr, 0, Loaded), ELoadResult::BadMagic);

	// Random garbage with a valid header must not crash or be accepted.
	for (uint32 Seed = 1; Seed < 200; ++Seed)
	{
		std::vector<uint8> Garbage = Bytes;
		uint32 State = Seed * 2654435761u;
		for (size_t Index = 16; Index < Garbage.size(); ++Index)
		{
			State = State * 1664525u + 1013904223u;
			Garbage[Index] = static_cast<uint8>(State >> 24);
		}
		const uint32 Crc = Crc32(Garbage.data() + 16, Garbage.size() - 16);
		for (int Byte = 0; Byte < 4; ++Byte)
		{
			Garbage[12 + Byte] = static_cast<uint8>(Crc >> (8 * Byte));
		}
		FCharacterRecord Fuzzed;
		const ELoadResult Result = DeserializeCharacter(Garbage.data(), Garbage.size(), Fuzzed);
		DB_CHECK(Result == ELoadResult::Corrupt || Result == ELoadResult::Ok);
	}
}

DB_TEST(Save_ServerRejectsTamperedUploads)
{
	const FItemCatalog& Catalog = TestCatalog();
	FCharacterRecord Record = MakeNewCharacter("id", "Jin Akagi", "Warrior", {});
	Record.Inventory.Add(Catalog, {"Katana_Basic", 1, 5, 100});
	Record.Inventory.Add(Catalog, {"Katana_Basic", 1, 6, 100});
	DB_CHECK(ValidateCharacterRecord(Record, Catalog).empty());

	FCharacterRecord Duped = Record;
	std::vector<FInventorySection> Sections;
	for (int32 Index = 0; Index < Duped.Inventory.NumSections(); ++Index)
	{
		Sections.push_back(Duped.Inventory.GetSection(Index));
	}
	Sections[0].Slots[1].InstanceId = 5; // duplicated unique item
	Duped.Inventory.RestoreRaw(Sections, {});
	DB_CHECK(!ValidateCharacterRecord(Duped, Catalog).empty());

	FCharacterRecord Rich = Record;
	Rich.Currency = -1;
	DB_CHECK(!ValidateCharacterRecord(Rich, Catalog).empty());

	FCharacterRecord BadName = Record;
	BadName.Name = "xX_Player12345_Xx";
	DB_CHECK(!ValidateCharacterRecord(BadName, Catalog).empty());

	FCharacterRecord Cheater = Record;
	Cheater.Progression.Level = 99;
	DB_CHECK(!ValidateCharacterRecord(Cheater, Catalog).empty());

	FSkillTreeDefinition Tree;
	Tree.Nodes.push_back({"Iaido", 1, 1, 1, {}});
	FRecordValidationRules Rules;
	Rules.SkillTree = &Tree;
	FCharacterRecord FreeSkills = Record;
	FreeSkills.Skills.Ranks["Iaido"] = 1; // never earned a point
	DB_CHECK(!ValidateCharacterRecord(FreeSkills, Catalog, Rules).empty());
}

DB_TEST(Save_WorldRoundTrip)
{
	FWorldRecord Record;
	Record.WorldId = "world-1";
	Record.World.AddRegion("Capital", ERegionKind::Capital);
	Record.World.AddRegion("Blood", ERegionKind::VassalRegion);
	Record.World.MarkVassalDefeated("Blood");
	Record.World.Advance(30.0);
	Record.World.StoryFlags = {"Story.IntroDone"};
	Record.World.RecordBossDefeat("Vassal_Blood");

	const FQuestDatabase Database = MakeQuests();
	Record.World.SharedQuests.Start(Database, "MQ01_TheKingsSummons", {});
	Record.World.SharedQuests.ApplyEvent(Database, {EObjectiveKind::Talk, "NPC_King", 1});

	const std::vector<uint8> Bytes = SerializeWorld(Record);
	FWorldRecord Loaded;
	DB_CHECK_EQ(DeserializeWorld(Bytes.data(), Bytes.size(), Loaded), ELoadResult::Ok);
	DB_CHECK_EQ(Loaded.WorldId, Record.WorldId);
	DB_CHECK_NEAR(Loaded.World.Clock.TotalHours, Record.World.Clock.TotalHours, 1e-9);
	DB_CHECK(Loaded.World.FindRegion("Blood")->bVassalDefeated);
	DB_CHECK_NEAR(Loaded.World.FindRegion("Blood")->DemonInfluence, Record.World.FindRegion("Blood")->DemonInfluence, 1e-6);
	DB_CHECK(Loaded.World.DefeatedBosses.count("Vassal_Blood") == 1);
	Loaded.World.SharedQuests.Reconcile(Database);
	DB_CHECK_EQ(Loaded.World.SharedQuests.Find("MQ01_TheKingsSummons")->ObjectiveCounts[0], 1);
	DB_CHECK(SerializeWorld(Loaded) == Bytes);
}
