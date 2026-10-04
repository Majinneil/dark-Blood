// DARK BLOOD - Rules Core: abstract settlement simulation (Phase 7, see docs/SETTLEMENT_SIMULATION.md).
// Population cohorts, stocks, prosperity, security, demon threat, buildings, building projects and demon attacks.
// Deterministic and time-step independent: the state advances in whole game hours with a random stream derived from
// the settlement seed and the hour index, so one 48 h step equals 48 single hours. The abstract state is the only truth
// for numbers; villagers in the world are a presentation of it.
#pragma once

#include "DarkBloodRules/RulesCore.h"

#include <string>
#include <vector>

namespace DarkBlood::Rules
{
	struct FSettlementStocks
	{
		double Food = 0.0;
		double Wood = 0.0;
		double Stone = 0.0;
		double Ore = 0.0;
		/** Mon in the settlement treasury. */
		double Money = 0.0;
	};

	enum class ESettlementBuilding : uint8
	{
		Houses,
		Farms,
		Workshops,
		Market,
		Walls,
		Barracks,
		Temple,
		Count
	};

	struct FSettlementBuilding
	{
		ESettlementBuilding Type = ESettlementBuilding::Houses;
		/** 0 = ruin, 1 = intact. */
		float Condition = 1.f;
		int32 Level = 1;
	};

	enum class ESettlementProject : uint8
	{
		Repair,
		Upgrade,
	};

	struct FSettlementProjectState
	{
		ESettlementProject Kind = ESettlementProject::Repair;
		ESettlementBuilding Target = ESettlementBuilding::Houses;
		/** Worker hours done and needed. */
		double Progress = 0.0;
		double WorkNeeded = 0.0;
	};

	struct FSettlementState
	{
		std::string SettlementId;
		std::string RegionId;
		int32 Children = 0;
		int32 Adults = 0;
		int32 Elders = 0;
		/** Adults serving as guards (part of Adults). */
		int32 Guards = 0;
		FSettlementStocks Stocks;
		/** 0..1 */
		float Prosperity = 0.5f;
		float Security = 0.5f;
		float Threat = 0.5f;
		std::vector<FSettlementBuilding> Buildings;
		std::vector<FSettlementProjectState> Projects;
		/** Story locations can be damaged but never emptied or destroyed. */
		bool bStoryProtected = false;
		/** Simulated game hours since creation (the settlement's own clock; whole hours are simulated). */
		double SimulatedHours = 0.0;
		uint32 Seed = 1;
		/** Hours without food (starvation pressure). */
		int32 HungryHours = 0;

		int32 GetPopulation() const { return Children + Adults + Elders; }
		int32 GetWorkers() const { return Adults > Guards ? Adults - Guards : 0; }
		DARKBLOODRULES_API const FSettlementBuilding* FindBuilding(ESettlementBuilding Type) const;
		DARKBLOODRULES_API FSettlementBuilding* FindBuilding(ESettlementBuilding Type);
	};

	enum class ESettlementEventKind : uint8
	{
		Births,
		Deaths,
		Starvation,
		DemonAttack,
		AttackRepelled,
		ProjectStarted,
		ProjectCompleted,
		GuardsRecruited,
		RefugeesArrived,
	};

	struct FSettlementEvent
	{
		ESettlementEventKind Kind = ESettlementEventKind::Births;
		std::string SettlementId;
		/** Game hour (settlement clock) the event happened in. */
		double AtHours = 0.0;
		/** People / damage percent / guards, depending on the kind. */
		int32 Amount = 0;
		ESettlementBuilding Building = ESettlementBuilding::Houses;
	};

	struct FSettlementRules
	{
		/** Food per person per game day. */
		double FoodPerPersonDay = 1.0;
		/** Food per farm worker per game day at average prosperity (scaled by farm condition). Balanced so that an occupied
		 *  settlement (threat ~0.85) just feeds itself and a liberated one builds a surplus. */
		double FoodPerFarmerDay = 4.5;
		/** Share of workers on the fields. */
		double FarmerShare = 0.55;
		/** Share of production lost at threat 1 (fields left alone, roads unsafe). */
		double ThreatProductionLoss = 0.25;
		double WoodPerWorkerDay = 0.15;
		double StonePerWorkerDay = 0.08;
		double OrePerWorkerDay = 0.03;
		/** Mon per person per game day at prosperity 1 (trade, taxes). */
		double MoneyPerPersonDay = 0.6;
		/** Births per family (2 adults) per game day with enough food. */
		double BirthsPerFamilyDay = 0.004;
		double ElderDeathsPerDay = 0.006;
		double ChildToAdultPerDay = 0.004;
		double AdultToElderPerDay = 0.0015;
		/** Starvation deaths per hungry day as a share of the population. */
		double StarvationDeathsPerDay = 0.02;
		/** Guards wanted per resident for full security. */
		double GuardsPerResident = 0.08;
		/** Mon to recruit one guard. */
		double GuardCost = 25.0;
		/** Chance per night hour at threat 1 and security 0. */
		double AttackChancePerNightHour = 0.08;
		/** Day hours are this much safer. */
		double DayAttackFactor = 0.15;
		/** Refugees per game day in liberated, prosperous regions. */
		double RefugeesPerDay = 1.5;
		/** Worker hours per building level for a repair of a ruin / an upgrade. */
		double RepairWorkPerLevel = 600.0;
		double UpgradeWorkPerLevel = 1500.0;
		double UpgradeWood = 40.0;
		double UpgradeStone = 30.0;
		double UpgradeMoney = 120.0;
		/** Never less than this many residents in a story-protected settlement; buildings keep this condition. */
		int32 ProtectedMinPopulation = 12;
		float ProtectedMinCondition = 0.25f;
	};

	/** Builds the starting state of a settlement of the given size (residents). */
	DARKBLOODRULES_API FSettlementState MakeSettlement(std::string SettlementId, std::string RegionId, int32 Residents, bool bStoryProtected,
		uint32 Seed);

	/**
	 * Advances a settlement by GameHours (whole hours are simulated; the remainder carries over). DemonInfluence is the
	 * region's value (0..1). StartTimeOfDay is the hour of day (0..24) at the settlement clock's hour 0, so night hours line
	 * up with the world clock. Returns what happened.
	 */
	DARKBLOODRULES_API std::vector<FSettlementEvent> AdvanceSettlement(FSettlementState& State, double GameHours, float DemonInfluence,
		double StartTimeOfDay = 8.0, const FSettlementRules& Rules = FSettlementRules());

	DARKBLOODRULES_API const char* ToString(ESettlementEventKind Kind);
	DARKBLOODRULES_API const char* ToString(ESettlementBuilding Building);
}
