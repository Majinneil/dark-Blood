// DARK BLOOD - Rules Core: dungeon layouts and dungeon progress (Phase 9). A seed and a few parameters give the same
// dungeon on every machine: rooms on a grid, connected by corridors (spanning tree + a few loops), with an entrance,
// fights, traps, a rest shrine, treasure in dead ends and the guardian's room farthest from the entrance.
#pragma once

#include "DarkBloodRules/RulesCore.h"

#include <string>
#include <utility>
#include <vector>

namespace DarkBlood::Rules
{
	enum class EDungeonRoomKind : uint8
	{
		Entrance,
		Combat,
		Treasure,
		Trap,
		Rest,
		Boss,
	};

	enum class EDungeonCell : uint8
	{
		Empty,
		Room,
		Corridor,
	};

	struct FDungeonRoom
	{
		int32 X = 0;
		int32 Y = 0;
		int32 W = 0;
		int32 H = 0;
		EDungeonRoomKind Kind = EDungeonRoomKind::Combat;
		/** Rooms between the entrance and this one. */
		int32 Depth = 0;
		/** Enemies that wake when a player enters (combat and boss rooms). */
		int32 Enemies = 0;

		int32 CenterX() const { return X + W / 2; }
		int32 CenterY() const { return Y + H / 2; }
		bool Contains(int32 CellX, int32 CellY) const { return CellX >= X && CellX < X + W && CellY >= Y && CellY < Y + H; }
	};

	struct FDungeonParams
	{
		int32 RoomCount = 10;
		/** Square grid side in cells. */
		int32 GridSize = 40;
		int32 MinRoomSize = 3;
		int32 MaxRoomSize = 5;
		/** 1 (easy) .. 5 (deadly): enemies per room and traps. */
		int32 Difficulty = 1;
		/** Corridors added beyond the spanning tree (circular routes). */
		int32 ExtraLinks = 2;
	};

	struct FDungeonLayout
	{
		uint32 Seed = 0;
		int32 Size = 0;
		std::vector<EDungeonCell> Cells;
		std::vector<FDungeonRoom> Rooms;
		/** Corridors between rooms (indices into Rooms). */
		std::vector<std::pair<int32, int32>> Links;

		EDungeonCell GetCell(int32 X, int32 Y) const
		{
			return X < 0 || Y < 0 || X >= Size || Y >= Size ? EDungeonCell::Empty : Cells[static_cast<size_t>(Y * Size + X)];
		}
		DARKBLOODRULES_API int32 FindRoom(EDungeonRoomKind Kind) const;
		/** Room containing a cell, or -1 (corridor / outside). */
		DARKBLOODRULES_API int32 FindRoomAt(int32 X, int32 Y) const;
	};

	DARKBLOODRULES_API FDungeonLayout GenerateDungeon(uint32 Seed, const FDungeonParams& Params = FDungeonParams());

	/** Checks the invariants: one entrance, one boss room farthest away, every room reachable, no overlaps. */
	DARKBLOODRULES_API bool ValidateDungeon(const FDungeonLayout& Layout, std::string* OutError = nullptr);

	/** Saved per world: a cleared dungeon stays empty for a while, then demons return. */
	struct FDungeonProgress
	{
		std::string DungeonId;
		int32 TimesCleared = 0;
		/** World hour of the last clear; -1 = never. */
		double ClearedAtHours = -1.0;
	};

	struct FDungeonRules
	{
		/** Game hours until a cleared dungeon is occupied again (3 days). */
		double RespawnHours = 72.0;
	};

	DARKBLOODRULES_API bool IsDungeonCleared(const FDungeonProgress& Progress, double NowHours, const FDungeonRules& Rules = FDungeonRules());

	DARKBLOODRULES_API const char* ToString(EDungeonRoomKind Kind);
}
