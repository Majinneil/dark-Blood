#include "DarkBloodRules/Endgame.h"

#include <algorithm>

namespace DarkBlood::Rules
{
	FEndgameScale GetCycleScale(int32 Cycle, const FCycleRules& Rules)
	{
		const float Steps = static_cast<float>(std::clamp(Cycle, 0, std::max(Rules.MaxScaledCycle, 0)));
		FEndgameScale Scale;
		Scale.EnemyHealth = 1.f + Rules.EnemyHealthPerCycle * Steps;
		Scale.EnemyDamage = 1.f + Rules.EnemyDamagePerCycle * Steps;
		Scale.Experience = 1.f + Rules.ExperiencePerCycle * Steps;
		Scale.RarityBonus = Rules.RarityBonusPerCycle * Steps;
		Scale.EnemyLevelBonus = Rules.LevelsPerCycle * static_cast<int32>(Steps);
		return Scale;
	}

	FEndgameScale GetEchoScale(int32 Cycle, int32 EchoRank, const FEchoRules& Rules, const FCycleRules& CycleRules)
	{
		FEndgameScale Scale = GetCycleScale(Cycle, CycleRules);
		const float Rank = static_cast<float>(std::clamp(EchoRank, 1, std::max(Rules.MaxRank, 1)));
		Scale.EnemyHealth *= 1.f + Rules.HealthPerRank * Rank;
		Scale.EnemyDamage *= 1.f + Rules.DamagePerRank * Rank;
		Scale.RarityBonus += 0.02f * Rank;
		Scale.EnemyLevelBonus += static_cast<int32>(Rank) * 2;
		return Scale;
	}

	FAbyssFloor GetAbyssFloor(int32 Depth, int32 Cycle, const FAbyssRules& Rules, const FCycleRules& CycleRules)
	{
		FAbyssFloor Floor;
		Floor.Depth = std::max(Depth, 1);
		const int32 Below = Floor.Depth - 1;
		// Fixed per floor and cycle: a deterministic hash (splitmix-style) of both.
		uint32 Hash = static_cast<uint32>(Floor.Depth) * 0x9E3779B9u ^ static_cast<uint32>(std::max(Cycle, 0) + 1) * 0x85EBCA6Bu;
		Hash ^= Hash >> 16;
		Hash *= 0x7FEB352Du;
		Hash ^= Hash >> 15;
		Floor.Seed = Hash;
		Floor.Rooms = std::min(Rules.BaseRooms + Below / 3, Rules.MaxRooms);
		Floor.EnemiesPerRoom = std::min(Rules.BaseEnemiesPerRoom + Below / 4, Rules.MaxEnemiesPerRoom);
		const FEndgameScale Cycled = GetCycleScale(Cycle, CycleRules);
		Floor.EnemyHealth = Cycled.EnemyHealth * (1.f + Rules.HealthPerFloor * static_cast<float>(Below));
		Floor.EnemyDamage = Cycled.EnemyDamage * (1.f + Rules.DamagePerFloor * static_cast<float>(Below));
		Floor.EnemyLevelBonus = Cycled.EnemyLevelBonus + Below;
		Floor.bGuardianFloor = Rules.GuardianEvery > 0 && Floor.Depth % Rules.GuardianEvery == 0;
		Floor.RarityBonus = Cycled.RarityBonus + std::min(Rules.RarityBonusPerFloor * static_cast<float>(Below), Rules.MaxRarityBonus);
		return Floor;
	}

	FLootTable ApplyEndgameLoot(const FLootTable& Table, float Bonus, const FItemCatalog& Catalog)
	{
		if (Bonus <= 0.f)
		{
			return Table;
		}
		FLootTable Out = Table;
		for (FLootEntry& Entry : Out.Entries)
		{
			const FItemDefinition* Item = Catalog.Find(Entry.ItemId);
			const float Step = Item ? static_cast<float>(static_cast<int32>(Item->Rarity)) : 0.f;
			// Weights stay integers (x100 keeps the ratios of the original table).
			Entry.Weight = std::max(1, static_cast<int32>(static_cast<float>(Entry.Weight) * 100.f * (1.f + Bonus * Step) + 0.5f));
		}
		Out.NothingWeight = static_cast<int32>(static_cast<float>(Table.NothingWeight) * 100.f * std::max(0.f, 1.f - Bonus) + 0.5f);
		Out.CurrencyMin = static_cast<int64>(static_cast<double>(Table.CurrencyMin) * (1.0 + 2.0 * Bonus));
		Out.CurrencyMax = static_cast<int64>(static_cast<double>(Table.CurrencyMax) * (1.0 + 2.0 * Bonus));
		return Out;
	}

	std::string GetCycleLabel(int32 Cycle)
	{
		if (Cycle <= 0)
		{
			return std::string();
		}
		return Cycle == 1 ? std::string("NG+") : "NG+" + std::to_string(Cycle);
	}
}
