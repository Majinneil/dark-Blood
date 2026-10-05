// DARK BLOOD - Rules Core: shared world state (story progress, regions, vassals, time).
// Owned by the server/host; replicated to clients via the UE world state component.
#pragma once

#include "DarkBloodRules/Quest.h"
#include "DarkBloodRules/Dungeon.h"
#include "DarkBloodRules/Settlement.h"

#include <string>
#include <string_view>
#include <vector>

namespace DarkBlood::Rules
{
	/** 16 vassals: one in each of the 14 vassal regions (DAS ENDE opens once they are free) and two guarding DAS ENDE
	 *  itself before the demon king (docs/BOSS_FRAMEWORK.md). */
	constexpr int32 NumVassalRegions = 14;
	constexpr int32 NumFinalRegionVassals = 2;
	constexpr int32 NumVassals = NumVassalRegions + NumFinalRegionVassals;

	enum class ERegionControl : uint8
	{
		Occupied,  // under a vassal's control
		Contested, // mid-boss defeated, humans pushing back
		Liberated, // vassal defeated
	};

	enum class ERegionKind : uint8
	{
		Capital,
		VassalRegion,
		FinalRegion, // DAS ENDE
		Epilogue,    // DAS PARADIES (story only)
	};

	struct FRegionState
	{
		std::string RegionId;
		ERegionKind Kind = ERegionKind::VassalRegion;
		ERegionControl Control = ERegionControl::Occupied;
		/** 0 = no demons, 1 = full demonic control. Drives spawn density and settlement pressure. */
		float DemonInfluence = 1.f;
		bool bMidBossDefeated = false;
		/** All vassals of the region defeated. */
		bool bVassalDefeated = false;
		/** Vassals ruling the region (1; DAS ENDE: 2) and how many of them fell. */
		int32 VassalCount = 1;
		int32 VassalsDefeated = 0;
		/** World time (in game hours) at liberation; -1 while not liberated. */
		double LiberatedAtHours = -1.0;
	};

	struct FWorldClockRules
	{
		/** Game hours that pass per real second (default: 1 game day = 48 real minutes). */
		double GameHoursPerRealSecond = 24.0 / (48.0 * 60.0);
		float DawnHour = 5.5f;
		float DuskHour = 19.5f;
	};

	struct FWorldClock
	{
		/** Total elapsed game hours since the world was created; starts at 08:00 on day 1. */
		double TotalHours = 8.0;

		DARKBLOODRULES_API float GetTimeOfDay() const;
		DARKBLOODRULES_API int32 GetDay() const;
		DARKBLOODRULES_API bool IsNight(const FWorldClockRules& Rules = FWorldClockRules()) const;
	};

	struct FRegionRecoveryRules
	{
		/** Influence after liberation decays towards this floor. */
		float LiberatedInfluenceFloor = 0.05f;
		/** Fraction of the remaining influence removed per game day. */
		float DailyInfluenceDecay = 0.25f;
		float ContestedInfluence = 0.7f;
	};

	class FWorldState
	{
	public:
		FWorldClock Clock;
		FStoryFlags StoryFlags;
		/** Bosses killed at least once (vassals, mid-bosses, world bosses). Ordered for serialization. */
		std::set<std::string, std::less<>> DefeatedBosses;
		FQuestLog SharedQuests;
		/** Abstract simulation of every settlement; their clocks run with the world clock (hour 0 = 00:00, day 1). */
		std::vector<FSettlementState> Settlements;
		/** Clears of each dungeon (cleared dungeons stay empty for a while). */
		std::vector<FDungeonProgress> Dungeons;

		DARKBLOODRULES_API FRegionState& AddRegion(std::string RegionId, ERegionKind Kind);
		DARKBLOODRULES_API FRegionState* FindRegion(std::string_view RegionId);
		DARKBLOODRULES_API const FRegionState* FindRegion(std::string_view RegionId) const;
		const std::vector<FRegionState>& GetRegions() const { return Regions; }
		std::vector<FRegionState>& GetRegionsMutable() { return Regions; }

		DARKBLOODRULES_API void RecordBossDefeat(std::string_view BossId);
		DARKBLOODRULES_API bool MarkMidBossDefeated(std::string_view RegionId, const FRegionRecoveryRules& Rules = FRegionRecoveryRules());
		/**
		 * A vassal of the region fell; the region is liberated once all its vassals fell (DAS ENDE stays the demon king's).
		 * Returns false if the region is unknown, has no vassals, or this vassal / the region was already counted
		 * (idempotent for co-op double kills). Without a VassalId the region counts as one vassal.
		 */
		DARKBLOODRULES_API bool MarkVassalDefeated(std::string_view RegionId, std::string_view VassalId = {});
		DARKBLOODRULES_API int32 CountDefeatedVassals() const;
		/** DAS ENDE opens once every vassal region is liberated. */
		DARKBLOODRULES_API bool IsFinalRegionOpen() const;
		/** The demon king can be fought once DAS ENDE's own vassals fell. */
		DARKBLOODRULES_API bool IsDemonKingReachable() const;

		/** The demon king fell (Phase 15): every vassal region and DAS ENDE liberated, no demon influence left; sets
		 *  Story.WorldPurified. Idempotent. */
		DARKBLOODRULES_API void PurifyWorld();
		DARKBLOODRULES_API bool IsPurified() const;

		/** Advances time and liberated-region recovery. Called by the server tick and by offline catch-up. */
		DARKBLOODRULES_API void Advance(double GameHours, const FRegionRecoveryRules& Rules = FRegionRecoveryRules());

		DARKBLOODRULES_API FSettlementState* FindSettlement(std::string_view SettlementId);
		/** Adds a settlement whose clock starts at the current world time. */
		DARKBLOODRULES_API FSettlementState& AddSettlement(FSettlementState Settlement);
		/** Catches every settlement up to the world clock with its region's demon influence; appends what happened. */
		DARKBLOODRULES_API void AdvanceSettlements(std::vector<FSettlementEvent>& OutEvents, const FSettlementRules& Rules = FSettlementRules());

		DARKBLOODRULES_API const FDungeonProgress* FindDungeon(std::string_view DungeonId) const;
		/** Records a clear at the current world time (counts every clear). */
		DARKBLOODRULES_API void MarkDungeonCleared(std::string_view DungeonId);
		DARKBLOODRULES_API bool IsDungeonCleared(std::string_view DungeonId, const FDungeonRules& Rules = FDungeonRules()) const;

	private:
		std::vector<FRegionState> Regions;
	};
}
