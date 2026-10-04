#include "TestFramework.h"

#include "DarkBloodRules/Records.h"
#include "DarkBloodRules/Settlement.h"
#include "DarkBloodRules/WorldState.h"

using namespace DarkBlood::Rules;

namespace
{
	int32 CountEvents(const std::vector<FSettlementEvent>& Events, ESettlementEventKind Kind)
	{
		int32 Count = 0;
		for (const FSettlementEvent& Event : Events)
		{
			Count += Event.Kind == Kind ? 1 : 0;
		}
		return Count;
	}

	bool SameState(const FSettlementState& A, const FSettlementState& B)
	{
		return A.Children == B.Children && A.Adults == B.Adults && A.Elders == B.Elders && A.Guards == B.Guards &&
			   A.Stocks.Food == B.Stocks.Food && A.Stocks.Money == B.Stocks.Money && A.Prosperity == B.Prosperity &&
			   A.Security == B.Security && A.Threat == B.Threat && A.Projects.size() == B.Projects.size() && A.HungryHours == B.HungryHours;
	}
}

DB_TEST(Settlement_TimeStepIndependent)
{
	FSettlementState OneStep = MakeSettlement("Dorf", "Region01", 120, false, 77);
	FSettlementState Hourly = OneStep;
	FSettlementState Uneven = OneStep;
	const std::vector<FSettlementEvent> Big = AdvanceSettlement(OneStep, 240.0, 0.6f);
	std::vector<FSettlementEvent> Small;
	for (int32 Hour = 0; Hour < 240; ++Hour)
	{
		const std::vector<FSettlementEvent> Events = AdvanceSettlement(Hourly, 1.0, 0.6f);
		Small.insert(Small.end(), Events.begin(), Events.end());
	}
	// Odd fractions carry over and must give the same result.
	for (int32 Step = 0; Step < 960; ++Step)
	{
		AdvanceSettlement(Uneven, 0.25, 0.6f);
	}
	DB_CHECK(SameState(OneStep, Hourly));
	DB_CHECK(SameState(OneStep, Uneven));
	DB_CHECK_EQ(Big.size(), Small.size());
}

DB_TEST(Settlement_FedVillageGrowsAndStarvingVillageShrinks)
{
	FSettlementState Fed = MakeSettlement("Reisdorf", "Region09", 150, false, 5);
	AdvanceSettlement(Fed, 24.0 * 60.0, 0.05f);
	DB_CHECK(Fed.GetPopulation() > 150);
	DB_CHECK_EQ(Fed.HungryHours, 0);

	FSettlementState Hungry = MakeSettlement("Grenzposten", "Region14", 150, false, 5);
	Hungry.Stocks.Food = 0.0;
	if (FSettlementBuilding* Farms = Hungry.FindBuilding(ESettlementBuilding::Farms))
	{
		Farms->Condition = 0.f; // fields burned
	}
	Hungry.Projects.push_back({ESettlementProject::Repair, ESettlementBuilding::Farms, 0.0, 1.0e9}); // never rebuilt
	const std::vector<FSettlementEvent> Events = AdvanceSettlement(Hungry, 24.0 * 20.0, 0.05f);
	DB_CHECK(Hungry.GetPopulation() < 150);
	DB_CHECK(CountEvents(Events, ESettlementEventKind::Starvation) > 0);
}

DB_TEST(Settlement_OccupiedVillagesHoldOutLiberatedOnesThrive)
{
	// Balance: under a vassal a village gets by (no famine within a month), once liberated it builds a surplus.
	for (uint32 Seed = 1; Seed <= 5; ++Seed)
	{
		FSettlementState Occupied = MakeSettlement("Dorf", "Region01", 140, false, Seed);
		FSettlementState Liberated = Occupied;
		const std::vector<FSettlementEvent> A = AdvanceSettlement(Occupied, 24.0 * 30.0, 0.85f);
		AdvanceSettlement(Liberated, 24.0 * 30.0, 0.05f);
		DB_CHECK(Occupied.GetPopulation() > 110);
		DB_CHECK(Liberated.Stocks.Food > Occupied.Stocks.Food);
		DB_CHECK(Liberated.Prosperity > Occupied.Prosperity);
		DB_CHECK(Liberated.GetPopulation() > 140);
	}
}

DB_TEST(Settlement_LiberationReducesAttacks)
{
	int32 OccupiedAttacks = 0;
	int32 LiberatedAttacks = 0;
	for (uint32 Seed = 1; Seed <= 10; ++Seed)
	{
		FSettlementState Occupied = MakeSettlement("Dorf", "Region01", 100, false, Seed);
		FSettlementState Liberated = Occupied;
		const std::vector<FSettlementEvent> A = AdvanceSettlement(Occupied, 24.0 * 30.0, 1.0f);
		const std::vector<FSettlementEvent> B = AdvanceSettlement(Liberated, 24.0 * 30.0, 0.05f);
		OccupiedAttacks += CountEvents(A, ESettlementEventKind::DemonAttack) + CountEvents(A, ESettlementEventKind::AttackRepelled);
		LiberatedAttacks += CountEvents(B, ESettlementEventKind::DemonAttack) + CountEvents(B, ESettlementEventKind::AttackRepelled);
	}
	DB_CHECK(OccupiedAttacks > 10);
	DB_CHECK(LiberatedAttacks * 5 < OccupiedAttacks);
}

DB_TEST(Settlement_StoryProtectedNeverEmptiedOrDestroyed)
{
	FSettlementRules Rules;
	Rules.AttackChancePerNightHour = 1.0; // an attack every night hour
	FSettlementState Capital = MakeSettlement("Hauptstadt", "Capital", 40, true, 9);
	Capital.Guards = 0;
	Capital.Security = 0.f;
	AdvanceSettlement(Capital, 24.0 * 60.0, 1.0f, 0.0, Rules);
	DB_CHECK(Capital.GetPopulation() >= Rules.ProtectedMinPopulation);
	for (const FSettlementBuilding& Building : Capital.Buildings)
	{
		DB_CHECK(Building.Condition >= Rules.ProtectedMinCondition);
	}
}

DB_TEST(Settlement_RepairsDamageAndRefugeesReturn)
{
	FSettlementState Village = MakeSettlement("Fischerdorf", "Region07", 120, false, 3);
	FSettlementBuilding* Houses = Village.FindBuilding(ESettlementBuilding::Houses);
	Houses->Condition = 0.2f;
	const std::vector<FSettlementEvent> Events = AdvanceSettlement(Village, 24.0 * 20.0, 0.02f);
	DB_CHECK(CountEvents(Events, ESettlementEventKind::ProjectStarted) >= 1);
	DB_CHECK(CountEvents(Events, ESettlementEventKind::ProjectCompleted) >= 1);
	DB_CHECK(Village.FindBuilding(ESettlementBuilding::Houses)->Condition > 0.9f);
	DB_CHECK(CountEvents(Events, ESettlementEventKind::RefugeesArrived) >= 1);
}

DB_TEST(Settlement_WorldAdvanceAndSaveRoundTrip)
{
	FWorldRecord Record;
	Record.WorldId = "World0";
	Record.World.AddRegion("Region01", ERegionKind::VassalRegion).DemonInfluence = 0.8f;
	Record.World.AddSettlement(MakeSettlement("Dorf", "Region01", 90, false, 42));
	Record.World.Advance(24.0 * 3.0);
	std::vector<FSettlementEvent> Events;
	Record.World.AdvanceSettlements(Events);
	const FSettlementState* Dorf = Record.World.FindSettlement("Dorf");
	DB_CHECK(Dorf != nullptr);
	DB_CHECK(Dorf && Dorf->SimulatedHours == Record.World.Clock.TotalHours);

	const std::vector<uint8> Bytes = SerializeWorld(Record);
	FWorldRecord Loaded;
	DB_CHECK(DeserializeWorld(Bytes.data(), Bytes.size(), Loaded) == ELoadResult::Ok);
	DB_CHECK_EQ(Loaded.World.Settlements.size(), size_t(1));
	DB_CHECK(SerializeWorld(Loaded) == Bytes);
	DB_CHECK(Dorf && SameState(*Dorf, Loaded.World.Settlements[0]));
}
