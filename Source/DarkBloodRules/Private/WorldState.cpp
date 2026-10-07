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

	bool FWorldState::MarkVassalDefeated(std::string_view RegionId, std::string_view VassalId)
	{
		FRegionState* Region = FindRegion(RegionId);
		const bool bHasVassals = Region && (Region->Kind == ERegionKind::VassalRegion || Region->Kind == ERegionKind::FinalRegion);
		if (!bHasVassals || Region->bVassalDefeated)
		{
			return false;
		}
		if (!VassalId.empty())
		{
			if (DefeatedBosses.find(VassalId) != DefeatedBosses.end())
			{
				return false;
			}
			DefeatedBosses.emplace(VassalId);
		}
		Region->VassalsDefeated = VassalId.empty() ? std::max(1, Region->VassalCount) : std::min(Region->VassalsDefeated + 1, std::max(1, Region->VassalCount));
		if (Region->VassalsDefeated >= Region->VassalCount)
		{
			Region->bVassalDefeated = true;
			if (Region->Kind == ERegionKind::VassalRegion)
			{
				Region->Control = ERegionControl::Liberated;
				Region->LiberatedAtHours = Clock.TotalHours;
			}
		}
		return true;
	}

	int32 FWorldState::CountDefeatedVassals() const
	{
		int32 Count = 0;
		for (const FRegionState& Region : Regions)
		{
			Count += Region.Kind == ERegionKind::VassalRegion || Region.Kind == ERegionKind::FinalRegion ? Region.VassalsDefeated : 0;
		}
		return Count;
	}

	bool FWorldState::IsFinalRegionOpen() const
	{
		const auto Liberated = std::count_if(Regions.begin(), Regions.end(), [](const FRegionState& Region)
			{ return Region.Kind == ERegionKind::VassalRegion && Region.bVassalDefeated; });
		return Liberated >= NumVassalRegions;
	}

	bool FWorldState::IsDemonKingReachable() const
	{
		const auto Final = std::find_if(Regions.begin(), Regions.end(), [](const FRegionState& Region) { return Region.Kind == ERegionKind::FinalRegion; });
		return IsFinalRegionOpen() && Final != Regions.end() && Final->bVassalDefeated;
	}

	void FWorldState::PurifyWorld()
	{
		for (FRegionState& Region : Regions)
		{
			if (Region.Kind == ERegionKind::VassalRegion || Region.Kind == ERegionKind::FinalRegion)
			{
				if (Region.Control != ERegionControl::Liberated)
				{
					Region.LiberatedAtHours = Clock.TotalHours;
				}
				Region.Control = ERegionControl::Liberated;
				Region.DemonInfluence = 0.f;
			}
		}
		StoryFlags.insert("Story.WorldPurified");
	}

	bool FWorldState::IsPurified() const
	{
		return StoryFlags.find("Story.WorldPurified") != StoryFlags.end();
	}

	bool FWorldState::CanBeginNewCycle() const
	{
		return IsPurified();
	}

	bool FWorldState::BeginNewCycle()
	{
		if (!CanBeginNewCycle())
		{
			return false;
		}
		RememberedBosses.insert(DefeatedBosses.begin(), DefeatedBosses.end());
		for (FRegionState& Region : Regions)
		{
			if (Region.Kind == ERegionKind::VassalRegion || Region.Kind == ERegionKind::FinalRegion)
			{
				Region.Control = ERegionControl::Occupied;
				Region.DemonInfluence = 1.f;
				Region.bMidBossDefeated = false;
				Region.bVassalDefeated = false;
				Region.VassalsDefeated = 0;
				Region.LiberatedAtHours = -1.0;
			}
		}
		DefeatedBosses.clear();
		StoryFlags.clear();
		SharedQuests = FQuestLog();
		Dungeons.clear();
		++Cycle;
		return true;
	}

	int32 FWorldState::RecordEchoVictory(std::string_view BossId)
	{
		const auto Found = EchoRanks.find(BossId);
		if (Found == EchoRanks.end())
		{
			EchoRanks.emplace(std::string(BossId), 1);
			return 1;
		}
		return ++Found->second;
	}

	int32 FWorldState::GetEchoRank(std::string_view BossId) const
	{
		const auto Found = EchoRanks.find(BossId);
		return Found == EchoRanks.end() ? 0 : Found->second;
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
