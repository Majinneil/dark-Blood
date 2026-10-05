#include "TestFramework.h"

#include "DarkBloodRules/Dungeon.h"
#include "DarkBloodRules/Records.h"
#include "DarkBloodRules/WorldState.h"

#include <algorithm>

using namespace DarkBlood::Rules;

DB_TEST(Dungeon_DeterministicAndValidForManySeeds)
{
	FDungeonParams Params;
	Params.Difficulty = 3;
	const FDungeonLayout A = GenerateDungeon(1234, Params);
	const FDungeonLayout B = GenerateDungeon(1234, Params);
	DB_CHECK(A.Cells == B.Cells);
	DB_CHECK_EQ(A.Rooms.size(), B.Rooms.size());
	DB_CHECK(GenerateDungeon(1235, Params).Cells != A.Cells);

	int32 Invalid = 0;
	for (uint32 Seed = 1; Seed <= 300; ++Seed)
	{
		for (int32 Difficulty = 1; Difficulty <= 5; Difficulty += 2)
		{
			FDungeonParams P;
			P.Difficulty = Difficulty;
			P.RoomCount = 7 + Difficulty;
			std::string Error;
			const FDungeonLayout Layout = GenerateDungeon(Seed, P);
			if (!ValidateDungeon(Layout, &Error) || Layout.Rooms.size() < 6)
			{
				++Invalid;
			}
		}
	}
	DB_CHECK_EQ(Invalid, 0);
}

DB_TEST(Dungeon_RolesFollowTheLayout)
{
	for (uint32 Seed = 10; Seed < 60; ++Seed)
	{
		FDungeonParams Params;
		Params.Difficulty = 4;
		Params.RoomCount = 11;
		const FDungeonLayout Layout = GenerateDungeon(Seed, Params);
		const FDungeonRoom& Entrance = Layout.Rooms[static_cast<size_t>(Layout.FindRoom(EDungeonRoomKind::Entrance))];
		const FDungeonRoom& Boss = Layout.Rooms[static_cast<size_t>(Layout.FindRoom(EDungeonRoomKind::Boss))];
		DB_CHECK_EQ(Entrance.Depth, 0);
		DB_CHECK_EQ(Entrance.Enemies, 0);
		DB_CHECK(Boss.Depth > 0);
		DB_CHECK(Boss.Enemies >= 1);
		DB_CHECK(Layout.FindRoom(EDungeonRoomKind::Trap) >= 0);
		for (int32 Index = 0; Index < static_cast<int32>(Layout.Rooms.size()); ++Index)
		{
			const FDungeonRoom& Room = Layout.Rooms[static_cast<size_t>(Index)];
			if (Room.Kind == EDungeonRoomKind::Treasure)
			{
				// Treasure waits in dead ends.
				const auto Degree = std::count_if(Layout.Links.begin(), Layout.Links.end(), [Index](const std::pair<int32, int32>& Link)
				{
					return Link.first == Index || Link.second == Index;
				});
				DB_CHECK_EQ(Degree, 1);
			}
			// Deeper fights are harder (never easier than the first room's).
			if (Room.Kind == EDungeonRoomKind::Combat)
			{
				DB_CHECK(Room.Enemies >= 2 + Params.Difficulty / 2);
			}
			DB_CHECK(Layout.FindRoomAt(Room.CenterX(), Room.CenterY()) == Index);
		}
	}
}

DB_TEST(Dungeon_ProgressRespawnsAndIsSaved)
{
	FWorldRecord Record;
	Record.WorldId = "World0";
	DB_CHECK(!Record.World.IsDungeonCleared("Shrine"));
	Record.World.MarkDungeonCleared("Shrine");
	DB_CHECK(Record.World.IsDungeonCleared("Shrine"));
	Record.World.Advance(71.0);
	DB_CHECK(Record.World.IsDungeonCleared("Shrine"));
	Record.World.Advance(2.0);
	DB_CHECK(!Record.World.IsDungeonCleared("Shrine")); // demons return after three days
	Record.World.MarkDungeonCleared("Shrine");
	DB_CHECK_EQ(Record.World.FindDungeon("Shrine")->TimesCleared, 2);

	const std::vector<uint8> Bytes = SerializeWorld(Record);
	FWorldRecord Loaded;
	DB_CHECK(DeserializeWorld(Bytes.data(), Bytes.size(), Loaded) == ELoadResult::Ok);
	DB_CHECK(Loaded.World.IsDungeonCleared("Shrine"));
	DB_CHECK_EQ(Loaded.World.FindDungeon("Shrine")->TimesCleared, 2);
	DB_CHECK(SerializeWorld(Loaded) == Bytes);
}
