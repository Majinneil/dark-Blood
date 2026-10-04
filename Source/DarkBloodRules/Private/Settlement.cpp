#include "DarkBloodRules/Settlement.h"

#include <algorithm>
#include <cmath>

namespace DarkBlood::Rules
{
	namespace
	{
		constexpr double HoursPerDay = 24.0;
		constexpr double DayFraction = 1.0 / HoursPerDay;

		/** Uniform [0, 1) from the settlement seed, the hour index and a salt (splitmix64): independent of step size. */
		double HourRandom(uint32 Seed, int64 Hour, uint32 Salt)
		{
			uint64 X = (static_cast<uint64>(Seed) << 32) ^ static_cast<uint64>(Hour) * 0x9E3779B97F4A7C15ull ^ (static_cast<uint64>(Salt) << 17);
			X += 0x9E3779B97F4A7C15ull;
			X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
			X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
			X ^= X >> 31;
			return static_cast<double>(X >> 11) * (1.0 / 9007199254740992.0);
		}

		/** Whole events from an expected (fractional) count: floor(expected + random). */
		int32 RoundRandom(double Expected, double Random)
		{
			return Expected <= 0.0 ? 0 : static_cast<int32>(std::floor(Expected + Random));
		}

		float Clamp01(double Value)
		{
			return static_cast<float>(std::clamp(Value, 0.0, 1.0));
		}

		bool IsNightHour(double TimeOfDay)
		{
			return TimeOfDay >= 19.5 || TimeOfDay < 5.5;
		}

		float Condition(const FSettlementState& State, ESettlementBuilding Type)
		{
			const FSettlementBuilding* Building = State.FindBuilding(Type);
			return Building ? Building->Condition : 0.f;
		}

		int32 Level(const FSettlementState& State, ESettlementBuilding Type)
		{
			const FSettlementBuilding* Building = State.FindBuilding(Type);
			return Building ? Building->Level : 0;
		}

		/** Removes up to Count residents, elders first; story settlements keep their minimum. Returns how many died. */
		int32 RemoveResidents(FSettlementState& State, int32 Count, const FSettlementRules& Rules, bool bChildrenLast = true)
		{
			if (State.bStoryProtected)
			{
				Count = std::min(Count, std::max(0, State.GetPopulation() - Rules.ProtectedMinPopulation));
			}
			int32 Removed = 0;
			auto Take = [&](int32& Cohort)
			{
				const int32 N = std::min(Cohort, Count - Removed);
				Cohort -= N;
				Removed += N;
			};
			Take(State.Elders);
			Take(State.Adults);
			if (bChildrenLast)
			{
				Take(State.Children);
			}
			State.Guards = std::min(State.Guards, State.Adults);
			return Removed;
		}

		void Emit(std::vector<FSettlementEvent>& Out, const FSettlementState& State, ESettlementEventKind Kind, int64 Hour, int32 Amount,
			ESettlementBuilding Building = ESettlementBuilding::Houses)
		{
			FSettlementEvent Event;
			Event.Kind = Kind;
			Event.SettlementId = State.SettlementId;
			Event.AtHours = static_cast<double>(Hour);
			Event.Amount = Amount;
			Event.Building = Building;
			Out.push_back(Event);
		}

		void ResolveAttack(FSettlementState& State, int64 Hour, const FSettlementRules& Rules, std::vector<FSettlementEvent>& Out)
		{
			const double Strength = State.Threat * (0.5 + HourRandom(State.Seed, Hour, 11));
			const double Defense = State.Security * (0.6 + 0.8 * HourRandom(State.Seed, Hour, 12));
			if (Defense >= Strength)
			{
				const int32 Lost = RoundRandom(State.Guards * 0.04, HourRandom(State.Seed, Hour, 13));
				State.Guards -= Lost;
				State.Adults -= Lost;
				Emit(Out, State, ESettlementEventKind::AttackRepelled, Hour, Lost);
				return;
			}
			const double Severity = std::min(1.0, Strength - Defense);
			// The demons strike one building and the people and stores around it.
			const int32 Index = static_cast<int32>(HourRandom(State.Seed, Hour, 14) * static_cast<double>(State.Buildings.size()));
			FSettlementBuilding& Hit = State.Buildings[static_cast<size_t>(std::clamp(Index, 0, static_cast<int32>(State.Buildings.size()) - 1))];
			const float MinCondition = State.bStoryProtected ? Rules.ProtectedMinCondition : 0.f;
			Hit.Condition = std::max(MinCondition, Hit.Condition - static_cast<float>(0.15 + 0.35 * Severity));
			const int32 Casualties = RemoveResidents(State, RoundRandom(State.GetPopulation() * 0.03 * Severity, HourRandom(State.Seed, Hour, 15)), Rules);
			const double Stolen = 0.1 + 0.2 * Severity;
			State.Stocks.Food *= 1.0 - Stolen;
			State.Stocks.Money *= 1.0 - Stolen;
			State.Prosperity = Clamp01(State.Prosperity - 0.05 * Severity);
			Emit(Out, State, ESettlementEventKind::DemonAttack, Hour, Casualties, Hit.Type);
		}

		void DailyDecisions(FSettlementState& State, int64 Hour, float DemonInfluence, const FSettlementRules& Rules, std::vector<FSettlementEvent>& Out)
		{
			const int32 HourOfDay = static_cast<int32>(Hour % 24);
			const int32 Population = State.GetPopulation();
			if (HourOfDay == 6)
			{
				// Recruit guards while the treasury and the workforce allow it (at most a few per day).
				const int32 Wanted = static_cast<int32>(std::ceil(Population * Rules.GuardsPerResident));
				const int32 Affordable = static_cast<int32>(State.Stocks.Money / Rules.GuardCost);
				const int32 SpareWorkers = State.GetWorkers() - static_cast<int32>(Population * 0.3);
				const int32 Recruits = std::min({Wanted - State.Guards, Affordable, SpareWorkers, 3});
				if (Recruits > 0)
				{
					State.Guards += Recruits;
					State.Stocks.Money -= Recruits * Rules.GuardCost;
					Emit(Out, State, ESettlementEventKind::GuardsRecruited, Hour, Recruits);
				}
			}
			else if (HourOfDay == 7 && State.Projects.empty())
			{
				// Repairs first, then upgrades: walls under threat, farms when food is short, otherwise homes and trade.
				const FSettlementBuilding* Damaged = nullptr;
				for (const FSettlementBuilding& Building : State.Buildings)
				{
					if (Building.Condition < 0.7f && (!Damaged || Building.Condition < Damaged->Condition))
					{
						Damaged = &Building;
					}
				}
				FSettlementProjectState Project;
				if (Damaged)
				{
					Project.Kind = ESettlementProject::Repair;
					Project.Target = Damaged->Type;
					Project.WorkNeeded = Rules.RepairWorkPerLevel * Damaged->Level * (1.0 - Damaged->Condition);
				}
				else if (State.Prosperity > 0.6f && State.Stocks.Wood >= Rules.UpgradeWood && State.Stocks.Stone >= Rules.UpgradeStone &&
						 State.Stocks.Money >= Rules.UpgradeMoney)
				{
					const double FoodDays = Population > 0 ? State.Stocks.Food / (Population * Rules.FoodPerPersonDay) : 99.0;
					ESettlementBuilding Target = State.Threat > 0.5f ? ESettlementBuilding::Walls
												 : FoodDays < 4.0 ? ESettlementBuilding::Farms
												 : Level(State, ESettlementBuilding::Houses) <= Level(State, ESettlementBuilding::Market) ? ESettlementBuilding::Houses
																									: ESettlementBuilding::Market;
					const int32 TargetLevel = Level(State, Target);
					Project.Kind = ESettlementProject::Upgrade;
					Project.Target = Target;
					Project.WorkNeeded = Rules.UpgradeWorkPerLevel * TargetLevel;
					State.Stocks.Wood -= Rules.UpgradeWood * TargetLevel;
					State.Stocks.Stone -= Rules.UpgradeStone * TargetLevel;
					State.Stocks.Money -= Rules.UpgradeMoney * TargetLevel;
					State.Stocks.Wood = std::max(0.0, State.Stocks.Wood);
					State.Stocks.Stone = std::max(0.0, State.Stocks.Stone);
					State.Stocks.Money = std::max(0.0, State.Stocks.Money);
				}
				if (Project.WorkNeeded > 0.0)
				{
					State.Projects.push_back(Project);
					Emit(Out, State, ESettlementEventKind::ProjectStarted, Hour, static_cast<int32>(Project.Kind), Project.Target);
				}
			}
			else if (HourOfDay == 12 && DemonInfluence < 0.3f && State.Prosperity > 0.55f && State.HungryHours == 0)
			{
				// Liberated, safe and fed: people return.
				const int32 Refugees = RoundRandom(Rules.RefugeesPerDay * (1.0 - DemonInfluence), HourRandom(State.Seed, Hour, 21));
				if (Refugees > 0)
				{
					State.Adults += Refugees;
					Emit(Out, State, ESettlementEventKind::RefugeesArrived, Hour, Refugees);
				}
			}
		}

		void SimulateHour(FSettlementState& State, int64 Hour, float DemonInfluence, double StartTimeOfDay, const FSettlementRules& Rules,
			std::vector<FSettlementEvent>& Out)
		{
			const double TimeOfDay = std::fmod(StartTimeOfDay + static_cast<double>(Hour), HoursPerDay);
			const bool bNight = IsNightHour(TimeOfDay);
			const int32 Population = State.GetPopulation();
			const double ProsperityFactor = 0.75 + 0.5 * State.Prosperity;
			const double ThreatFactor = 1.0 - State.Threat * Rules.ThreatProductionLoss;
			const double Workers = static_cast<double>(State.GetWorkers());

			// Production and consumption.
			FSettlementStocks& Stocks = State.Stocks;
			const double Farmers = Workers * Rules.FarmerShare;
			const double Crafters = Workers - Farmers;
			Stocks.Food += Farmers * Rules.FoodPerFarmerDay * DayFraction * Condition(State, ESettlementBuilding::Farms) *
						   (0.8 + 0.2 * Level(State, ESettlementBuilding::Farms)) * ProsperityFactor * ThreatFactor;
			Stocks.Food -= Population * Rules.FoodPerPersonDay * DayFraction;
			const double CraftFactor = DayFraction * (0.5 + 0.5 * Condition(State, ESettlementBuilding::Workshops)) * ThreatFactor;
			Stocks.Wood += Crafters * Rules.WoodPerWorkerDay * CraftFactor;
			Stocks.Stone += Crafters * Rules.StonePerWorkerDay * CraftFactor;
			Stocks.Ore += Crafters * Rules.OrePerWorkerDay * CraftFactor;
			Stocks.Money += Population * Rules.MoneyPerPersonDay * DayFraction * State.Prosperity * ThreatFactor *
							(0.6 + 0.4 * Condition(State, ESettlementBuilding::Market)) * (0.8 + 0.2 * Level(State, ESettlementBuilding::Market));
			if (Stocks.Food < 0.0)
			{
				Stocks.Food = 0.0;
				++State.HungryHours;
			}
			else
			{
				State.HungryHours = std::max(0, State.HungryHours - 1);
			}

			// Population: births with food, old age, growing up, starvation.
			const bool bFed = State.HungryHours == 0;
			const int32 Births = bFed ? RoundRandom(State.Adults * 0.5 * Rules.BirthsPerFamilyDay * DayFraction * ProsperityFactor, HourRandom(State.Seed, Hour, 1)) : 0;
			const int32 Grown = std::min(State.Children, RoundRandom(State.Children * Rules.ChildToAdultPerDay * DayFraction, HourRandom(State.Seed, Hour, 2)));
			const int32 Aged = std::min(State.Adults - State.Guards, RoundRandom(State.Adults * Rules.AdultToElderPerDay * DayFraction, HourRandom(State.Seed, Hour, 3)));
			State.Children += Births - Grown;
			State.Adults += Grown - std::max(0, Aged);
			State.Elders += std::max(0, Aged);
			if (Births > 0)
			{
				Emit(Out, State, ESettlementEventKind::Births, Hour, Births);
			}
			int32 Deaths = 0;
			const int32 ElderDeaths = RoundRandom(State.Elders * Rules.ElderDeathsPerDay * DayFraction, HourRandom(State.Seed, Hour, 4));
			if (ElderDeaths > 0)
			{
				Deaths += RemoveResidents(State, std::min(ElderDeaths, State.Elders), Rules);
			}
			if (Deaths > 0)
			{
				Emit(Out, State, ESettlementEventKind::Deaths, Hour, Deaths);
			}
			if (State.HungryHours > 24)
			{
				const int32 Starved = RemoveResidents(State, RoundRandom(State.GetPopulation() * Rules.StarvationDeathsPerDay * DayFraction,
					HourRandom(State.Seed, Hour, 5)), Rules);
				if (Starved > 0)
				{
					State.Prosperity = Clamp01(State.Prosperity - 0.01);
					Emit(Out, State, ESettlementEventKind::Starvation, Hour, Starved);
				}
			}

			// Threat and security follow the region, the walls and the guards.
			const double Walls = Condition(State, ESettlementBuilding::Walls) * std::min(1.0, Level(State, ESettlementBuilding::Walls) / 2.0);
			State.Threat = Clamp01(DemonInfluence * (1.0 - 0.3 * Walls));
			const int32 PopulationNow = std::max(1, State.GetPopulation());
			const double GuardCoverage = std::min(1.0, State.Guards / (PopulationNow * Rules.GuardsPerResident));
			const double SecurityTarget = 0.65 * GuardCoverage + 0.25 * Walls + 0.1 * Condition(State, ESettlementBuilding::Barracks);
			State.Security = Clamp01(State.Security + (SecurityTarget - State.Security) * 0.1);

			// Demon attacks: mostly at night, rarer with good security.
			const double AttackChance = Rules.AttackChancePerNightHour * State.Threat * (1.0 - 0.8 * State.Security) * (bNight ? 1.0 : Rules.DayAttackFactor);
			if (State.GetPopulation() > 0 && HourRandom(State.Seed, Hour, 10) < AttackChance)
			{
				ResolveAttack(State, Hour, Rules, Out);
			}

			// Building projects advance with daytime work.
			if (!State.Projects.empty() && !bNight)
			{
				FSettlementProjectState& Project = State.Projects.front();
				Project.Progress += static_cast<double>(State.GetWorkers()) * 0.25;
				if (Project.Progress >= Project.WorkNeeded)
				{
					if (FSettlementBuilding* Building = State.FindBuilding(Project.Target))
					{
						if (Project.Kind == ESettlementProject::Repair)
						{
							Building->Condition = 1.f;
						}
						else
						{
							++Building->Level;
						}
					}
					Emit(Out, State, ESettlementEventKind::ProjectCompleted, Hour, static_cast<int32>(Project.Kind), Project.Target);
					State.Projects.erase(State.Projects.begin());
				}
			}

			// Prosperity drifts towards what food, safety and intact buildings support.
			double ConditionSum = 0.0;
			for (const FSettlementBuilding& Building : State.Buildings)
			{
				ConditionSum += Building.Condition;
			}
			const double AverageCondition = State.Buildings.empty() ? 0.0 : ConditionSum / static_cast<double>(State.Buildings.size());
			const double FoodDays = Stocks.Food / (PopulationNow * Rules.FoodPerPersonDay);
			const double ProsperityTarget = 0.25 + 0.3 * std::min(1.0, FoodDays / 3.0) + 0.25 * State.Security + 0.2 * AverageCondition - 0.35 * State.Threat;
			State.Prosperity = Clamp01(State.Prosperity + (ProsperityTarget - State.Prosperity) * 0.02);

			DailyDecisions(State, Hour, DemonInfluence, Rules, Out);
		}
	}

	const FSettlementBuilding* FSettlementState::FindBuilding(ESettlementBuilding Type) const
	{
		for (const FSettlementBuilding& Building : Buildings)
		{
			if (Building.Type == Type)
			{
				return &Building;
			}
		}
		return nullptr;
	}

	FSettlementBuilding* FSettlementState::FindBuilding(ESettlementBuilding Type)
	{
		return const_cast<FSettlementBuilding*>(static_cast<const FSettlementState*>(this)->FindBuilding(Type));
	}

	FSettlementState MakeSettlement(std::string SettlementId, std::string RegionId, int32 Residents, bool bStoryProtected, uint32 Seed)
	{
		FSettlementState State;
		State.SettlementId = std::move(SettlementId);
		State.RegionId = std::move(RegionId);
		State.Children = Residents / 4;
		State.Elders = Residents * 15 / 100;
		State.Adults = Residents - State.Children - State.Elders;
		State.Guards = State.Adults / 10;
		State.Stocks.Food = Residents * 5.0;
		State.Stocks.Wood = Residents * 0.5;
		State.Stocks.Stone = Residents * 0.3;
		State.Stocks.Money = Residents * 2.0;
		State.bStoryProtected = bStoryProtected;
		State.Seed = Seed == 0 ? 1u : Seed;
		for (int32 Type = 0; Type < static_cast<int32>(ESettlementBuilding::Count); ++Type)
		{
			FSettlementBuilding Building;
			Building.Type = static_cast<ESettlementBuilding>(Type);
			State.Buildings.push_back(Building);
		}
		return State;
	}

	std::vector<FSettlementEvent> AdvanceSettlement(FSettlementState& State, double GameHours, float DemonInfluence, double StartTimeOfDay,
		const FSettlementRules& Rules)
	{
		std::vector<FSettlementEvent> Events;
		if (GameHours <= 0.0)
		{
			return Events;
		}
		const int64 FirstHour = static_cast<int64>(std::floor(State.SimulatedHours));
		State.SimulatedHours += GameHours;
		const int64 LastHour = static_cast<int64>(std::floor(State.SimulatedHours));
		const float Influence = Clamp01(DemonInfluence);
		for (int64 Hour = FirstHour; Hour < LastHour; ++Hour)
		{
			SimulateHour(State, Hour, Influence, StartTimeOfDay, Rules, Events);
		}
		return Events;
	}

	const char* ToString(ESettlementEventKind Kind)
	{
		switch (Kind)
		{
		case ESettlementEventKind::Births: return "Births";
		case ESettlementEventKind::Deaths: return "Deaths";
		case ESettlementEventKind::Starvation: return "Starvation";
		case ESettlementEventKind::DemonAttack: return "DemonAttack";
		case ESettlementEventKind::AttackRepelled: return "AttackRepelled";
		case ESettlementEventKind::ProjectStarted: return "ProjectStarted";
		case ESettlementEventKind::ProjectCompleted: return "ProjectCompleted";
		case ESettlementEventKind::GuardsRecruited: return "GuardsRecruited";
		case ESettlementEventKind::RefugeesArrived: return "RefugeesArrived";
		}
		return "?";
	}

	const char* ToString(ESettlementBuilding Building)
	{
		switch (Building)
		{
		case ESettlementBuilding::Houses: return "Houses";
		case ESettlementBuilding::Farms: return "Farms";
		case ESettlementBuilding::Workshops: return "Workshops";
		case ESettlementBuilding::Market: return "Market";
		case ESettlementBuilding::Walls: return "Walls";
		case ESettlementBuilding::Barracks: return "Barracks";
		case ESettlementBuilding::Temple: return "Temple";
		case ESettlementBuilding::Count: break;
		}
		return "?";
	}
}
