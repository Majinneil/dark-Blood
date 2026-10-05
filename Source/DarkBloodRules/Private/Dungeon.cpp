#include "DarkBloodRules/Dungeon.h"

#include <algorithm>
#include <cstdlib>
#include <queue>

namespace DarkBlood::Rules
{
	namespace
	{
		/** Small deterministic random stream (splitmix64). */
		class FDungeonRandom
		{
		public:
			explicit FDungeonRandom(uint32 Seed) : State(static_cast<uint64>(Seed) * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull) {}

			uint64 Next()
			{
				uint64 X = (State += 0x9E3779B97F4A7C15ull);
				X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
				X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
				return X ^ (X >> 31);
			}

			/** Uniform in [Min, Max]. */
			int32 Range(int32 Min, int32 Max)
			{
				return Max <= Min ? Min : Min + static_cast<int32>(Next() % static_cast<uint64>(Max - Min + 1));
			}

		private:
			uint64 State;
		};

		bool Overlaps(const FDungeonRoom& A, const FDungeonRoom& B, int32 Margin)
		{
			return A.X - Margin < B.X + B.W && B.X - Margin < A.X + A.W && A.Y - Margin < B.Y + B.H && B.Y - Margin < A.Y + A.H;
		}

		int32 Distance(const FDungeonRoom& A, const FDungeonRoom& B)
		{
			return std::abs(A.CenterX() - B.CenterX()) + std::abs(A.CenterY() - B.CenterY());
		}

		void Carve(FDungeonLayout& Layout, int32 X, int32 Y, EDungeonCell Cell)
		{
			EDungeonCell& Target = Layout.Cells[static_cast<size_t>(Y * Layout.Size + X)];
			if (Target == EDungeonCell::Empty)
			{
				Target = Cell;
			}
		}

		/** L-shaped corridor between two room centers. */
		void CarveCorridor(FDungeonLayout& Layout, const FDungeonRoom& A, const FDungeonRoom& B, bool bHorizontalFirst)
		{
			int32 X = A.CenterX();
			int32 Y = A.CenterY();
			const int32 TargetX = B.CenterX();
			const int32 TargetY = B.CenterY();
			auto StepX = [&]() { while (X != TargetX) { X += TargetX > X ? 1 : -1; Carve(Layout, X, Y, EDungeonCell::Corridor); } };
			auto StepY = [&]() { while (Y != TargetY) { Y += TargetY > Y ? 1 : -1; Carve(Layout, X, Y, EDungeonCell::Corridor); } };
			if (bHorizontalFirst)
			{
				StepX();
				StepY();
			}
			else
			{
				StepY();
				StepX();
			}
		}

		std::vector<int32> RoomDepths(const FDungeonLayout& Layout, int32 Start)
		{
			std::vector<int32> Depth(Layout.Rooms.size(), -1);
			if (Start < 0)
			{
				return Depth;
			}
			std::queue<int32> Open;
			Depth[static_cast<size_t>(Start)] = 0;
			Open.push(Start);
			while (!Open.empty())
			{
				const int32 Room = Open.front();
				Open.pop();
				for (const std::pair<int32, int32>& Link : Layout.Links)
				{
					const int32 Other = Link.first == Room ? Link.second : Link.second == Room ? Link.first : -1;
					if (Other >= 0 && Depth[static_cast<size_t>(Other)] < 0)
					{
						Depth[static_cast<size_t>(Other)] = Depth[static_cast<size_t>(Room)] + 1;
						Open.push(Other);
					}
				}
			}
			return Depth;
		}
	}

	int32 FDungeonLayout::FindRoom(EDungeonRoomKind Kind) const
	{
		for (size_t Index = 0; Index < Rooms.size(); ++Index)
		{
			if (Rooms[Index].Kind == Kind)
			{
				return static_cast<int32>(Index);
			}
		}
		return -1;
	}

	int32 FDungeonLayout::FindRoomAt(int32 X, int32 Y) const
	{
		for (size_t Index = 0; Index < Rooms.size(); ++Index)
		{
			if (Rooms[Index].Contains(X, Y))
			{
				return static_cast<int32>(Index);
			}
		}
		return -1;
	}

	FDungeonLayout GenerateDungeon(uint32 Seed, const FDungeonParams& Params)
	{
		FDungeonRandom Random(Seed);
		FDungeonLayout Layout;
		Layout.Seed = Seed;
		Layout.Size = std::max(Params.GridSize, Params.MaxRoomSize * 3 + 4);
		Layout.Cells.assign(static_cast<size_t>(Layout.Size * Layout.Size), EDungeonCell::Empty);

		// Rooms: random rectangles two cells apart.
		const int32 Wanted = std::max(2, Params.RoomCount);
		for (int32 Attempt = 0; Attempt < Wanted * 80 && static_cast<int32>(Layout.Rooms.size()) < Wanted; ++Attempt)
		{
			FDungeonRoom Room;
			Room.W = Random.Range(Params.MinRoomSize, Params.MaxRoomSize);
			Room.H = Random.Range(Params.MinRoomSize, Params.MaxRoomSize);
			Room.X = Random.Range(1, Layout.Size - Room.W - 1);
			Room.Y = Random.Range(1, Layout.Size - Room.H - 1);
			const bool bFree = std::none_of(Layout.Rooms.begin(), Layout.Rooms.end(), [&Room](const FDungeonRoom& Other) { return Overlaps(Room, Other, 2); });
			if (bFree)
			{
				Layout.Rooms.push_back(Room);
			}
		}
		const int32 Count = static_cast<int32>(Layout.Rooms.size());

		// Corridors: a minimum spanning tree over the room centers, then a few short extra links for loops.
		std::vector<bool> InTree(static_cast<size_t>(Count), false);
		InTree[0] = true;
		for (int32 Added = 1; Added < Count; ++Added)
		{
			int32 BestFrom = -1;
			int32 BestTo = -1;
			int32 BestDistance = 1 << 30;
			for (int32 From = 0; From < Count; ++From)
			{
				for (int32 To = 0; To < Count; ++To)
				{
					if (InTree[static_cast<size_t>(From)] && !InTree[static_cast<size_t>(To)])
					{
						const int32 D = Distance(Layout.Rooms[static_cast<size_t>(From)], Layout.Rooms[static_cast<size_t>(To)]);
						if (D < BestDistance)
						{
							BestDistance = D;
							BestFrom = From;
							BestTo = To;
						}
					}
				}
			}
			InTree[static_cast<size_t>(BestTo)] = true;
			Layout.Links.emplace_back(BestFrom, BestTo);
		}
		for (int32 Extra = 0; Extra < Params.ExtraLinks && Count > 3; ++Extra)
		{
			const int32 From = Random.Range(0, Count - 1);
			int32 Best = -1;
			int32 BestDistance = 1 << 30;
			for (int32 To = 0; To < Count; ++To)
			{
				const bool bLinked = std::any_of(Layout.Links.begin(), Layout.Links.end(), [From, To](const std::pair<int32, int32>& Link)
				{
					return (Link.first == From && Link.second == To) || (Link.first == To && Link.second == From);
				});
				const int32 D = Distance(Layout.Rooms[static_cast<size_t>(From)], Layout.Rooms[static_cast<size_t>(To)]);
				if (To != From && !bLinked && D < BestDistance)
				{
					BestDistance = D;
					Best = To;
				}
			}
			if (Best >= 0)
			{
				Layout.Links.emplace_back(From, Best);
			}
		}

		// Carve rooms first, corridors around them.
		for (const FDungeonRoom& Room : Layout.Rooms)
		{
			for (int32 Y = Room.Y; Y < Room.Y + Room.H; ++Y)
			{
				for (int32 X = Room.X; X < Room.X + Room.W; ++X)
				{
					Carve(Layout, X, Y, EDungeonCell::Room);
				}
			}
		}
		for (const std::pair<int32, int32>& Link : Layout.Links)
		{
			CarveCorridor(Layout, Layout.Rooms[static_cast<size_t>(Link.first)], Layout.Rooms[static_cast<size_t>(Link.second)], (Random.Next() & 1) != 0);
		}

		// Roles: entrance near the grid corner, the guardian farthest away, treasure in dead ends, a rest shrine halfway.
		int32 Entrance = 0;
		for (int32 Index = 1; Index < Count; ++Index)
		{
			const FDungeonRoom& Room = Layout.Rooms[static_cast<size_t>(Index)];
			const FDungeonRoom& Best = Layout.Rooms[static_cast<size_t>(Entrance)];
			if (Room.CenterX() + Room.CenterY() < Best.CenterX() + Best.CenterY())
			{
				Entrance = Index;
			}
		}
		const std::vector<int32> Depth = RoomDepths(Layout, Entrance);
		int32 Boss = Entrance == 0 ? 1 : 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FDungeonRoom& Room = Layout.Rooms[static_cast<size_t>(Index)];
			Room.Depth = Depth[static_cast<size_t>(Index)];
			const FDungeonRoom& Best = Layout.Rooms[static_cast<size_t>(Boss)];
			if (Index != Entrance && (Room.Depth > Depth[static_cast<size_t>(Boss)] ||
										 (Room.Depth == Depth[static_cast<size_t>(Boss)] && Room.W * Room.H > Best.W * Best.H)))
			{
				Boss = Index;
			}
		}
		for (FDungeonRoom& Room : Layout.Rooms)
		{
			Room.Kind = EDungeonRoomKind::Combat;
		}
		Layout.Rooms[static_cast<size_t>(Entrance)].Kind = EDungeonRoomKind::Entrance;
		Layout.Rooms[static_cast<size_t>(Boss)].Kind = EDungeonRoomKind::Boss;
		const int32 BossDepth = Layout.Rooms[static_cast<size_t>(Boss)].Depth;
		int32 Treasures = 0;
		for (int32 Index = 0; Index < Count && Treasures < 2; ++Index)
		{
			FDungeonRoom& Room = Layout.Rooms[static_cast<size_t>(Index)];
			const int32 Degree = static_cast<int32>(std::count_if(Layout.Links.begin(), Layout.Links.end(), [Index](const std::pair<int32, int32>& Link)
			{
				return Link.first == Index || Link.second == Index;
			}));
			if (Room.Kind == EDungeonRoomKind::Combat && Degree == 1)
			{
				Room.Kind = EDungeonRoomKind::Treasure;
				++Treasures;
			}
		}
		int32 RestRoom = -1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FDungeonRoom& Room = Layout.Rooms[static_cast<size_t>(Index)];
			if (Room.Kind == EDungeonRoomKind::Combat &&
				(RestRoom < 0 || std::abs(Room.Depth - BossDepth / 2) < std::abs(Layout.Rooms[static_cast<size_t>(RestRoom)].Depth - BossDepth / 2)))
			{
				RestRoom = Index;
			}
		}
		if (RestRoom >= 0 && Count > 4)
		{
			Layout.Rooms[static_cast<size_t>(RestRoom)].Kind = EDungeonRoomKind::Rest;
		}
		int32 Traps = 1 + Params.Difficulty / 2;
		for (int32 Attempt = 0; Attempt < Count * 4 && Traps > 0; ++Attempt)
		{
			FDungeonRoom& Room = Layout.Rooms[static_cast<size_t>(Random.Range(0, Count - 1))];
			if (Room.Kind == EDungeonRoomKind::Combat && Room.Depth > 0)
			{
				Room.Kind = EDungeonRoomKind::Trap;
				--Traps;
			}
		}
		for (FDungeonRoom& Room : Layout.Rooms)
		{
			switch (Room.Kind)
			{
			case EDungeonRoomKind::Combat: Room.Enemies = std::min(8, 2 + Params.Difficulty / 2 + Room.Depth / 3); break;
			case EDungeonRoomKind::Trap: Room.Enemies = 1; break;
			case EDungeonRoomKind::Boss: Room.Enemies = 1 + Params.Difficulty / 2; break;
			default: Room.Enemies = 0; break;
			}
		}
		return Layout;
	}

	bool ValidateDungeon(const FDungeonLayout& Layout, std::string* OutError)
	{
		auto Fail = [OutError](const char* Message)
		{
			if (OutError)
			{
				*OutError = Message;
			}
			return false;
		};
		const int32 Count = static_cast<int32>(Layout.Rooms.size());
		if (Count < 2 || Layout.Cells.size() != static_cast<size_t>(Layout.Size * Layout.Size))
		{
			return Fail("too small");
		}
		int32 Entrances = 0;
		int32 Bosses = 0;
		int32 MaxDepth = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FDungeonRoom& Room = Layout.Rooms[static_cast<size_t>(Index)];
			Entrances += Room.Kind == EDungeonRoomKind::Entrance ? 1 : 0;
			Bosses += Room.Kind == EDungeonRoomKind::Boss ? 1 : 0;
			MaxDepth = std::max(MaxDepth, Room.Depth);
			for (int32 Other = Index + 1; Other < Count; ++Other)
			{
				if (Overlaps(Room, Layout.Rooms[static_cast<size_t>(Other)], 0))
				{
					return Fail("rooms overlap");
				}
			}
		}
		if (Entrances != 1 || Bosses != 1)
		{
			return Fail("needs one entrance and one boss room");
		}
		const FDungeonRoom& Boss = Layout.Rooms[static_cast<size_t>(Layout.FindRoom(EDungeonRoomKind::Boss))];
		if (Boss.Depth != MaxDepth)
		{
			return Fail("boss room is not the farthest");
		}
		// Every room must be walkable from the entrance over carved cells.
		const FDungeonRoom& Start = Layout.Rooms[static_cast<size_t>(Layout.FindRoom(EDungeonRoomKind::Entrance))];
		std::vector<bool> Seen(Layout.Cells.size(), false);
		std::queue<std::pair<int32, int32>> Open;
		Open.emplace(Start.CenterX(), Start.CenterY());
		Seen[static_cast<size_t>(Start.CenterY() * Layout.Size + Start.CenterX())] = true;
		while (!Open.empty())
		{
			const auto [X, Y] = Open.front();
			Open.pop();
			const int32 Steps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
			for (const auto& Step : Steps)
			{
				const int32 NX = X + Step[0];
				const int32 NY = Y + Step[1];
				if (Layout.GetCell(NX, NY) != EDungeonCell::Empty && !Seen[static_cast<size_t>(NY * Layout.Size + NX)])
				{
					Seen[static_cast<size_t>(NY * Layout.Size + NX)] = true;
					Open.emplace(NX, NY);
				}
			}
		}
		for (const FDungeonRoom& Room : Layout.Rooms)
		{
			if (!Seen[static_cast<size_t>(Room.CenterY() * Layout.Size + Room.CenterX())])
			{
				return Fail("unreachable room");
			}
		}
		return true;
	}

	bool IsDungeonCleared(const FDungeonProgress& Progress, double NowHours, const FDungeonRules& Rules)
	{
		return Progress.ClearedAtHours >= 0.0 && NowHours - Progress.ClearedAtHours < Rules.RespawnHours;
	}

	const char* ToString(EDungeonRoomKind Kind)
	{
		switch (Kind)
		{
		case EDungeonRoomKind::Entrance: return "Entrance";
		case EDungeonRoomKind::Combat: return "Combat";
		case EDungeonRoomKind::Treasure: return "Treasure";
		case EDungeonRoomKind::Trap: return "Trap";
		case EDungeonRoomKind::Rest: return "Rest";
		case EDungeonRoomKind::Boss: return "Boss";
		}
		return "?";
	}
}
