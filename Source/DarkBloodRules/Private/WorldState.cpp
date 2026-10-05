#include "DarkBloodRules/WorldState.h"

#include <algorithm>
#include <cmath>

namespace DarkBlood::Rules
{
	float FWorldClock::GetTimeOfDay() const
	{
		const double Hours = std::fmod(std::max(0.0, TotalHours), 24.0);
		return static_cast<float>(Hours);
	}

	int32 FWorldClock::GetDay() const
	{
		return 1 + static_cast<int32>(std::floor(std::max(0.0, TotalHours) / 24.0));
	}

	bool FWorldClock::IsNight(const FWorldClockRules& Rules) const
	{
		const float Hour = GetTimeOfDay();
		return Hour < Rules.DawnHour || Hour >= Rules.DuskHour;
	}

	FRegionState& FWorldState::AddRegion(std::string RegionId, ERegionKind Kind)
	{
		if (FRegionState* Existing = FindRegion(RegionId))
		{
			Existing->Kind = Kind;
			return *Existing;
		}
		FRegionState& Region = Regions.emplace_back();
		Region.RegionId = std::move(RegionId);
		Region.Kind = Kind;
		if (Kind == ERegionKind::Capital || Kind == ERegionKind::Epilogue)
		{
			Region.Control = ERegionControl::Liberated;
			Region.DemonInfluence = 0.f;
		}
		return Region;
	}

	FRegionState* FWorldState::FindRegion(std::string_view RegionId)
	{
		for (FRegionState& Region : Regions)
		{
			if (Region.RegionId == RegionId)
			{
				return &Region;
			}
		}
		return nullptr;
	}

	const FRegionState* FWorldState::FindRegion(std::string_view RegionId) const
	{
		return const_cast<FWorldState*>(this)->FindRegion(RegionId);
	}

	void FWorldState::RecordBossDefeat(std::string_view BossId)
	{
		if (!BossId.empty())
		{
			DefeatedBosses.emplace(BossId);
		}
	}

	bool FWorldState::MarkMidBossDefeated(std::string_view RegionId, const FRegionRecoveryRules& Rules)
	{
		FRegionState* Region = FindRegion(RegionId);
		if (!Region || Region->bMidBossDefeated)
		{
			return false;
		}
		Region->bMidBossDefeated = true;
		if (Region->Control == ERegionControl::Occupied)
		{
			Region->Control = ERegionControl::Contested;
			Region->DemonInfluence = std::min(Region->DemonInfluence, Rules.ContestedInfluence);
		}
		return true;
	}

	bool FWorldState::MarkVassalDefeated(std::string_view RegionId)
	{
		FRegionState* Region = FindRegion(RegionId);
		if (!Region || Region->Kind != ERegionKind::VassalRegion || Region->bVassalDefeated)
		{
			return false;
		}
		Region->bVassalDefeated = true;
		Region->Control = ERegionControl::Liberated;
		Region->LiberatedAtHours = Clock.TotalHours;
		return true;
	}

	int32 FWorldState::CountDefeatedVassals() const
	{
		return static_cast<int32>(std::count_if(Regions.begin(), Regions.end(), [](const FRegionState& Region)
			{ return Region.Kind == ERegionKind::VassalRegion && Region.bVassalDefeated; }));
	}

	bool FWorldState::IsFinalRegionOpen() const
	{
		return CountDefeatedVassals() >= NumVassalRegions;
	}

	void FWorldState::Advance(double GameHours, const FRegionRecoveryRules& Rules)
	{
		if (GameHours <= 0.0)
		{
			return;
		}
		Clock.TotalHours += GameHours;

		// Exponential decay is frame-rate independent: splitting Advance() calls yields the same result.
		const double Days = GameHours / 24.0;
		const double Keep = std::pow(1.0 - std::clamp(static_cast<double>(Rules.DailyInfluenceDecay), 0.0, 1.0), Days);
		for (FRegionState& Region : Regions)
		{
			if (Region.Control != ERegionControl::Liberated || Region.DemonInfluence <= Rules.LiberatedInfluenceFloor)
			{
				continue;
			}
			const double Above = static_cast<double>(Region.DemonInfluence - Rules.LiberatedInfluenceFloor);
			Region.DemonInfluence = Rules.LiberatedInfluenceFloor + static_cast<float>(Above * Keep);
		}
	}

	const FDungeonProgress* FWorldState::FindDungeon(std::string_view DungeonId) const
	{
		for (const FDungeonProgress& Dungeon : Dungeons)
		{
			if (Dungeon.DungeonId == DungeonId)
			{
				return &Dungeon;
			}
		}
		return nullptr;
	}

	void FWorldState::MarkDungeonCleared(std::string_view DungeonId)
	{
		FDungeonProgress* Progress = const_cast<FDungeonProgress*>(FindDungeon(DungeonId));
		if (!Progress)
		{
			Dungeons.push_back(FDungeonProgress{std::string(DungeonId), 0, -1.0});
			Progress = &Dungeons.back();
		}
		++Progress->TimesCleared;
		Progress->ClearedAtHours = Clock.TotalHours;
	}

	bool FWorldState::IsDungeonCleared(std::string_view DungeonId, const FDungeonRules& Rules) const
	{
		const FDungeonProgress* Progress = FindDungeon(DungeonId);
		return Progress && DarkBlood::Rules::IsDungeonCleared(*Progress, Clock.TotalHours, Rules);
	}

	FSettlementState* FWorldState::FindSettlement(std::string_view SettlementId)
	{
		for (FSettlementState& Settlement : Settlements)
		{
			if (Settlement.SettlementId == SettlementId)
			{
				return &Settlement;
			}
		}
		return nullptr;
	}

	FSettlementState& FWorldState::AddSettlement(FSettlementState Settlement)
	{
		Settlement.SimulatedHours = Clock.TotalHours;
		Settlements.push_back(std::move(Settlement));
		return Settlements.back();
	}

	void FWorldState::AdvanceSettlements(std::vector<FSettlementEvent>& OutEvents, const FSettlementRules& Rules)
	{
		for (FSettlementState& Settlement : Settlements)
		{
			const double Behind = Clock.TotalHours - Settlement.SimulatedHours;
			if (Behind < 1.0)
			{
				continue; // only whole hours are simulated
			}
			const FRegionState* Region = FindRegion(Settlement.RegionId);
			const float Influence = Region ? Region->DemonInfluence : 0.5f;
			std::vector<FSettlementEvent> Events = AdvanceSettlement(Settlement, Behind, Influence, 0.0, Rules);
			OutEvents.insert(OutEvents.end(), Events.begin(), Events.end());
		}
	}
}
