// DARK BLOOD - Rules Core: shared world state (story progress, regions, vassals, time).
// Owned by the server/host; replicated to clients via the UE world state component.
#pragma once

#include "DarkBloodRules/Quest.h"

#include <string>
#include <string_view>
#include <vector>

namespace DarkBlood::Rules
{
	constexpr int32 NumVassalRegions = 14;

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
		bool bVassalDefeated = false;
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

		DARKBLOODRULES_API FRegionState& AddRegion(std::string RegionId, ERegionKind Kind);
		DARKBLOODRULES_API FRegionState* FindRegion(std::string_view RegionId);
		DARKBLOODRULES_API const FRegionState* FindRegion(std::string_view RegionId) const;
		const std::vector<FRegionState>& GetRegions() const { return Regions; }
		std::vector<FRegionState>& GetRegionsMutable() { return Regions; }

		DARKBLOODRULES_API void RecordBossDefeat(std::string_view BossId);
		DARKBLOODRULES_API bool MarkMidBossDefeated(std::string_view RegionId, const FRegionRecoveryRules& Rules = FRegionRecoveryRules());
		/** Liberates the region. Returns false if unknown or already liberated (idempotent for co-op double kills). */
		DARKBLOODRULES_API bool MarkVassalDefeated(std::string_view RegionId);
		DARKBLOODRULES_API int32 CountDefeatedVassals() const;
		/** DAS ENDE opens once every vassal region is liberated. */
		DARKBLOODRULES_API bool IsFinalRegionOpen() const;

		/** Advances time and liberated-region recovery. Called by the server tick and by offline catch-up. */
		DARKBLOODRULES_API void Advance(double GameHours, const FRegionRecoveryRules& Rules = FRegionRecoveryRules());

	private:
		std::vector<FRegionState> Regions;
	};
}
