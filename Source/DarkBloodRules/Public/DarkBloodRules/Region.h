// DARK BLOOD - Rules Core: region life (Phase 11, docs/REGIONS.md). How many demon packs roam around a player in a
// region, how strong they are, and how strong a region's demon commander (mid-boss) is. Danger grows region by region
// (progression through danger, not locks); liberating a region empties it by day and thins its nights.
#pragma once

#include "DarkBloodRules/RulesCore.h"
#include "DarkBloodRules/WorldState.h"

namespace DarkBlood::Rules
{
	struct FRegionalSpawnRules
	{
		/** Demons per pack. */
		int32 PackSize = 3;
		/** Packs never exceed this per player (frame budget). */
		int32 MaxPacksPerPlayer = 3;
		/** Liberated regions keep a night pack while the demon influence is above this. */
		float LingeringInfluence = 0.2f;
		/** Levels added per point of stat growth (see GetRegionalStatMultiplier). */
		float StatGrowthPerLevel = 0.12f;
		/** Level of the base lesser demon the multiplier starts from. */
		int32 BaseDemonLevel = 3;
	};

	/** Demon packs that roam around one player in this region (capital and paradise: none). */
	DARKBLOODRULES_API int32 GetRegionalPackBudget(const FRegionState& Region, bool bNight, const FRegionalSpawnRules& Rules = {});

	/** Level of a regional demon: inside the region's recommended band, deterministic for a seed; elites +3. */
	DARKBLOODRULES_API int32 GetRegionalDemonLevel(int32 RecommendedMin, int32 RecommendedMax, uint32 Seed, bool bElite);

	/** Health / attack / armor multiplier of a demon of this level against the base lesser demon (never below 1). */
	DARKBLOODRULES_API float GetRegionalStatMultiplier(int32 Level, const FRegionalSpawnRules& Rules = {});

	/** Stat multiplier of a region's commander (mid-boss) relative to its vassal: 45 % of the vassal at the region's
	 *  lower band, so the commander is the warm-up for the vassal. */
	DARKBLOODRULES_API float GetCommanderStrength(int32 VassalOrder);

	DARKBLOODRULES_API const char* ToString(ERegionControl Control);
}
