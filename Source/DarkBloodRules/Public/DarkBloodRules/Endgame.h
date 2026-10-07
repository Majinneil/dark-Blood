// Endgame (Phase 16): New Game+ cycles, the Hall of Echoes (vassal rematches) and the Abyss (an endless dungeon).
// Pure rules - the game layer reads the scales when it spawns enemies, rolls loot and grants experience.
#pragma once

#include "DarkBloodRules/Crafting.h"
#include "DarkBloodRules/Items.h"
#include "DarkBloodRules/RulesCore.h"

#include <string>

namespace DarkBlood::Rules
{
	/** Multipliers of one New Game+ cycle (cycle 0 = the first playthrough). */
	struct FEndgameScale
	{
		float EnemyHealth = 1.f;
		float EnemyDamage = 1.f;
		float Experience = 1.f;
		/** Added to the chance of each rarity step when loot is rolled (0.08 = +8 % per step). */
		float RarityBonus = 0.f;
		/** Level added to every enemy and boss of the cycle. */
		int32 EnemyLevelBonus = 0;
	};

	struct FCycleRules
	{
		float EnemyHealthPerCycle = 0.6f;
		float EnemyDamagePerCycle = 0.3f;
		float ExperiencePerCycle = 0.5f;
		float RarityBonusPerCycle = 0.06f;
		int32 LevelsPerCycle = 10;
		/** Cycles beyond this keep the scale of the last one (NG+7 is the summit). */
		int32 MaxScaledCycle = 7;
	};

	DARKBLOODRULES_API FEndgameScale GetCycleScale(int32 Cycle, const FCycleRules& Rules = FCycleRules());

	/** The Hall of Echoes: a remembered vassal fought again, stronger with every echo already defeated. */
	struct FEchoRules
	{
		float HealthPerRank = 0.35f;
		float DamagePerRank = 0.2f;
		int32 MaxRank = 10;
	};

	/** Scale of a rematch: the world's cycle scale times the echo rank (rank 1 = first rematch). */
	DARKBLOODRULES_API FEndgameScale GetEchoScale(int32 Cycle, int32 EchoRank, const FEchoRules& Rules = FEchoRules(), const FCycleRules& CycleRules = FCycleRules());

	/** One floor of the Abyss. */
	struct FAbyssFloor
	{
		int32 Depth = 1;
		/** Seed for the dungeon generator: the same floor always has the same layout. */
		uint32 Seed = 0;
		int32 Rooms = 5;
		int32 EnemiesPerRoom = 3;
		float EnemyHealth = 1.f;
		float EnemyDamage = 1.f;
		int32 EnemyLevelBonus = 0;
		/** Every fifth floor ends in a guardian fight. */
		bool bGuardianFloor = false;
		float RarityBonus = 0.f;
	};

	struct FAbyssRules
	{
		int32 BaseRooms = 4;
		int32 MaxRooms = 9;
		int32 BaseEnemiesPerRoom = 2;
		int32 MaxEnemiesPerRoom = 6;
		float HealthPerFloor = 0.12f;
		float DamagePerFloor = 0.07f;
		float RarityBonusPerFloor = 0.01f;
		float MaxRarityBonus = 0.3f;
		int32 GuardianEvery = 5;
	};

	DARKBLOODRULES_API FAbyssFloor GetAbyssFloor(int32 Depth, int32 Cycle, const FAbyssRules& Rules = FAbyssRules(), const FCycleRules& CycleRules = FCycleRules());

	/**
	 * Loot of the endgame: Bonus (FEndgameScale::RarityBonus) multiplies the weight of every entry by
	 * 1 + Bonus * rarity step (rare and better items climb), shrinks the chance of nothing and adds currency.
	 */
	DARKBLOODRULES_API FLootTable ApplyEndgameLoot(const FLootTable& Table, float Bonus, const FItemCatalog& Catalog);

	/** Display text of a cycle ("", "NG+", "NG+2" ...). */
	DARKBLOODRULES_API std::string GetCycleLabel(int32 Cycle);
}
