#include "DarkBloodRules/Region.h"

#include <algorithm>

namespace DarkBlood::Rules
{
	int32 GetRegionalPackBudget(const FRegionState& Region, bool bNight, const FRegionalSpawnRules& Rules)
	{
		int32 Packs = 0;
		switch (Region.Kind)
		{
		case ERegionKind::Capital:
		case ERegionKind::Epilogue:
			return 0;
		case ERegionKind::FinalRegion:
			// Freed when the demon king falls (PurifyWorld).
			Packs = Region.Control == ERegionControl::Liberated ? 0 : (bNight ? 3 : 2);
			break;
		case ERegionKind::VassalRegion:
			switch (Region.Control)
			{
			case ERegionControl::Occupied:
				Packs = bNight ? 3 : 2;
				break;
			case ERegionControl::Contested:
				Packs = bNight ? 2 : 1;
				break;
			case ERegionControl::Liberated:
				Packs = bNight && Region.DemonInfluence > Rules.LingeringInfluence ? 1 : 0;
				break;
			}
			break;
		}
		return std::clamp(Packs, 0, Rules.MaxPacksPerPlayer);
	}

	int32 GetRegionalDemonLevel(int32 RecommendedMin, int32 RecommendedMax, uint32 Seed, bool bElite)
	{
		const int32 Low = std::max(1, RecommendedMin);
		const int32 High = std::max(Low, RecommendedMax);
		// Small integer hash: spread seeds evenly over the band.
		uint32 Hash = Seed * 2654435761u;
		Hash ^= Hash >> 16;
		const int32 Level = Low + static_cast<int32>(Hash % static_cast<uint32>(High - Low + 1));
		return std::min(100, Level + (bElite ? 3 : 0));
	}

	float GetRegionalStatMultiplier(int32 Level, const FRegionalSpawnRules& Rules)
	{
		return std::max(1.f, 1.f + static_cast<float>(Level - Rules.BaseDemonLevel) * Rules.StatGrowthPerLevel);
	}

	float GetCommanderStrength(int32 VassalOrder)
	{
		(void)VassalOrder;
		return 0.45f;
	}

	const char* ToString(ERegionControl Control)
	{
		switch (Control)
		{
		case ERegionControl::Occupied: return "occupied";
		case ERegionControl::Contested: return "contested";
		case ERegionControl::Liberated: return "liberated";
		}
		return "?";
	}
}
