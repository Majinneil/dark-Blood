#include "TestFramework.h"

#include "DarkBloodRules/Region.h"

using namespace DarkBlood::Rules;

DB_TEST(Region_LiberationEmptiesTheDayAndThinsTheNight)
{
	FRegionState Region;
	Region.Kind = ERegionKind::VassalRegion;
	DB_CHECK_EQ(GetRegionalPackBudget(Region, false), 2);
	DB_CHECK_EQ(GetRegionalPackBudget(Region, true), 3);
	Region.Control = ERegionControl::Contested;
	DB_CHECK_EQ(GetRegionalPackBudget(Region, false), 1);
	DB_CHECK_EQ(GetRegionalPackBudget(Region, true), 2);
	Region.Control = ERegionControl::Liberated;
	Region.DemonInfluence = 0.6f;
	DB_CHECK_EQ(GetRegionalPackBudget(Region, false), 0);
	DB_CHECK_EQ(GetRegionalPackBudget(Region, true), 1);
	Region.DemonInfluence = 0.1f; // days later: the nights are safe too
	DB_CHECK_EQ(GetRegionalPackBudget(Region, true), 0);
}

DB_TEST(Region_SafeAndFinalRegions)
{
	FRegionState Capital;
	Capital.Kind = ERegionKind::Capital;
	DB_CHECK_EQ(GetRegionalPackBudget(Capital, true), 0);
	FRegionState Paradise;
	Paradise.Kind = ERegionKind::Epilogue;
	DB_CHECK_EQ(GetRegionalPackBudget(Paradise, true), 0);
	FRegionState End;
	End.Kind = ERegionKind::FinalRegion;
	DB_CHECK_EQ(GetRegionalPackBudget(End, false), 2);
	FRegionalSpawnRules Tight;
	Tight.MaxPacksPerPlayer = 1;
	DB_CHECK_EQ(GetRegionalPackBudget(End, true, Tight), 1);
}

DB_TEST(Region_DemonsGrowWithTheRegion)
{
	for (uint32 Seed = 0; Seed < 200; ++Seed)
	{
		const int32 Level = GetRegionalDemonLevel(11, 17, Seed, false);
		DB_CHECK(Level >= 11 && Level <= 17);
	}
	DB_CHECK_EQ(GetRegionalDemonLevel(11, 17, 7, true), GetRegionalDemonLevel(11, 17, 7, false) + 3);
	DB_CHECK_EQ(GetRegionalDemonLevel(9, 3, 1, false), 9); // inverted band: the lower bound wins
	DB_CHECK(GetRegionalStatMultiplier(3) == 1.f);
	DB_CHECK(GetRegionalStatMultiplier(1) == 1.f);
	DB_CHECK(GetRegionalStatMultiplier(53) > 6.9f && GetRegionalStatMultiplier(53) < 7.1f);
	DB_CHECK(GetRegionalStatMultiplier(20) < GetRegionalStatMultiplier(40));
	DB_CHECK(GetCommanderStrength(1) < 1.f);
}
