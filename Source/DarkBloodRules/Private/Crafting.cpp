#include "DarkBloodRules/Crafting.h"

#include <algorithm>
#include <cmath>

namespace DarkBlood::Rules
{
	// ---- Random ---------------------------------------------------------------------------------

	FLootRandom::FLootRandom(uint64 Seed)
		: State(Seed != 0 ? Seed : 0x9E3779B97F4A7C15ull)
	{
	}

	uint32 FLootRandom::Next()
	{
		State ^= State >> 12;
		State ^= State << 25;
		State ^= State >> 27;
		return static_cast<uint32>((State * 0x2545F4914F6CDD1Dull) >> 32);
	}

	int32 FLootRandom::Range(int32 Min, int32 Max)
	{
		if (Max <= Min)
		{
			return Min;
		}
		const uint32 Span = static_cast<uint32>(Max - Min) + 1u;
		return Min + static_cast<int32>(Next() % Span);
	}

	// ---- Loot -----------------------------------------------------------------------------------

	namespace
	{
		void AddMerged(std::vector<FItemStack>& Items, const std::string& ItemId, int32 Count)
		{
			if (Count <= 0 || ItemId.empty())
			{
				return;
			}
			for (FItemStack& Existing : Items)
			{
				if (Existing.ItemId == ItemId)
				{
					Existing.Count += Count;
					return;
				}
			}
			FItemStack Stack;
			Stack.ItemId = ItemId;
			Stack.Count = Count;
			Items.push_back(Stack);
		}
	}

	FLootResult RollLoot(const FLootTable& Table, FLootRandom& Random)
	{
		FLootResult Result;
		for (const FItemStack& Guaranteed : Table.Guaranteed)
		{
			AddMerged(Result.Items, Guaranteed.ItemId, Guaranteed.Count);
		}

		int32 TotalWeight = std::max(0, Table.NothingWeight);
		for (const FLootEntry& Entry : Table.Entries)
		{
			TotalWeight += std::max(0, Entry.Weight);
		}
		for (int32 Roll = 0; Roll < Table.Rolls && TotalWeight > 0; ++Roll)
		{
			int32 Pick = Random.Range(0, TotalWeight - 1);
			if (Pick < Table.NothingWeight)
			{
				continue;
			}
			Pick -= std::max(0, Table.NothingWeight);
			for (const FLootEntry& Entry : Table.Entries)
			{
				const int32 Weight = std::max(0, Entry.Weight);
				if (Pick < Weight)
				{
					AddMerged(Result.Items, Entry.ItemId, Random.Range(Entry.MinCount, std::max(Entry.MinCount, Entry.MaxCount)));
					break;
				}
				Pick -= Weight;
			}
		}

		if (Table.CurrencyMax > 0)
		{
			const int64 Min = std::max<int64>(0, Table.CurrencyMin);
			const int64 Max = std::max(Min, Table.CurrencyMax);
			Result.Currency = Min + static_cast<int64>(Random.Next() % static_cast<uint64>(Max - Min + 1));
		}
		return Result;
	}

	bool ValidateLootTable(const FLootTable& Table, const FItemCatalog& Catalog)
	{
		if (Table.Rolls < 0 || Table.NothingWeight < 0 || Table.CurrencyMin < 0 || Table.CurrencyMax < Table.CurrencyMin)
		{
			return false;
		}
		for (const FItemStack& Guaranteed : Table.Guaranteed)
		{
			if (Guaranteed.Count <= 0 || !Catalog.Find(Guaranteed.ItemId))
			{
				return false;
			}
		}
		for (const FLootEntry& Entry : Table.Entries)
		{
			if (Entry.Weight < 0 || Entry.MinCount <= 0 || Entry.MaxCount < Entry.MinCount || !Catalog.Find(Entry.ItemId))
			{
				return false;
			}
		}
		return true;
	}

	// ---- Crafting -------------------------------------------------------------------------------

	const char* ToString(ECraftResult Result)
	{
		switch (Result)
		{
		case ECraftResult::Ok: return "Ok";
		case ECraftResult::InvalidRecipe: return "InvalidRecipe";
		case ECraftResult::WrongStation: return "WrongStation";
		case ECraftResult::LevelTooLow: return "LevelTooLow";
		case ECraftResult::MissingIngredients: return "MissingIngredients";
		case ECraftResult::NotEnoughCurrency: return "NotEnoughCurrency";
		case ECraftResult::NoSpace: return "NoSpace";
		case ECraftResult::NothingToRepair: return "NothingToRepair";
		}
		return "Unknown";
	}

	ECraftResult CanCraft(const FRecipe& Recipe, const FInventory& Inventory, const FItemCatalog& Catalog, int64 Currency, int32 Level,
		const std::string& StationId)
	{
		if (Recipe.OutputCount <= 0 || !Catalog.Find(Recipe.OutputItemId))
		{
			return ECraftResult::InvalidRecipe;
		}
		if (!Recipe.StationId.empty() && Recipe.StationId != StationId)
		{
			return ECraftResult::WrongStation;
		}
		if (Level < Recipe.RequiredLevel)
		{
			return ECraftResult::LevelTooLow;
		}
		for (const FRecipeIngredient& Input : Recipe.Inputs)
		{
			if (Input.Count <= 0 || Inventory.CountItem(Input.ItemId) < Input.Count)
			{
				return ECraftResult::MissingIngredients;
			}
		}
		if (Currency < Recipe.CurrencyCost)
		{
			return ECraftResult::NotEnoughCurrency;
		}
		return ECraftResult::Ok;
	}

	ECraftResult Craft(const FRecipe& Recipe, FInventory& Inventory, int64& Currency, const FItemCatalog& Catalog, int32 Level,
		const std::string& StationId, uint64 NewInstanceId)
	{
		const ECraftResult Check = CanCraft(Recipe, Inventory, Catalog, Currency, Level, StationId);
		if (Check != ECraftResult::Ok)
		{
			return Check;
		}
		// Work on a copy: consuming the inputs may be what frees the space for the output.
		FInventory Working = Inventory;
		for (const FRecipeIngredient& Input : Recipe.Inputs)
		{
			if (Working.Remove(Input.ItemId, Input.Count) != EInventoryResult::Ok)
			{
				return ECraftResult::MissingIngredients;
			}
		}
		const FItemDefinition* Output = Catalog.Find(Recipe.OutputItemId);
		FItemStack Stack;
		Stack.ItemId = Recipe.OutputItemId;
		Stack.Count = Recipe.OutputCount;
		if (Output->MaxDurability > 0)
		{
			Stack.Count = 1; // unique forged instance
			Stack.InstanceId = NewInstanceId;
			Stack.Durability = Output->MaxDurability;
		}
		if (Working.Add(Catalog, Stack, EAddMode::AllOrNothing).Result != EInventoryResult::Ok)
		{
			return ECraftResult::NoSpace;
		}
		Inventory = std::move(Working);
		Currency -= Recipe.CurrencyCost;
		return ECraftResult::Ok;
	}

	// ---- Repair ---------------------------------------------------------------------------------

	int64 RepairCost(const FItemDefinition& Definition, int32 CurrentDurability)
	{
		if (Definition.MaxDurability <= 0 || CurrentDurability >= Definition.MaxDurability)
		{
			return 0;
		}
		const double Missing = static_cast<double>(Definition.MaxDurability - std::max(0, CurrentDurability)) / Definition.MaxDurability;
		const double FullPrice = std::max(10.0, Definition.BaseValue * 0.25);
		return static_cast<int64>(std::ceil(Missing * FullPrice));
	}

	int64 RepairEquipment(FEquipment& Equipment, int64& Currency, const FItemCatalog& Catalog, ECraftResult* OutResult)
	{
		struct FCandidate
		{
			EEquipSlot Slot;
			int64 Cost;
		};
		std::vector<FCandidate> Damaged;
		for (int32 Index = 1; Index < FEquipment::NumSlots; ++Index)
		{
			const EEquipSlot Slot = static_cast<EEquipSlot>(Index);
			const FItemStack& Stack = Equipment.Get(Slot);
			const FItemDefinition* Definition = Stack.IsEmpty() ? nullptr : Catalog.Find(Stack.ItemId);
			if (Definition)
			{
				if (const int64 Cost = RepairCost(*Definition, Stack.Durability); Cost > 0)
				{
					Damaged.push_back({Slot, Cost});
				}
			}
		}
		if (Damaged.empty())
		{
			if (OutResult)
			{
				*OutResult = ECraftResult::NothingToRepair;
			}
			return 0;
		}
		std::sort(Damaged.begin(), Damaged.end(), [](const FCandidate& A, const FCandidate& B) { return A.Cost < B.Cost; });

		int64 Spent = 0;
		for (const FCandidate& Candidate : Damaged)
		{
			if (Currency < Candidate.Cost)
			{
				continue;
			}
			FItemStack& Stack = Equipment.GetMutable(Candidate.Slot);
			Stack.Durability = Catalog.Find(Stack.ItemId)->MaxDurability;
			Currency -= Candidate.Cost;
			Spent += Candidate.Cost;
		}
		if (OutResult)
		{
			*OutResult = Spent > 0 ? ECraftResult::Ok : ECraftResult::NotEnoughCurrency;
		}
		return Spent;
	}

	// ---- Consumables ----------------------------------------------------------------------------

	FConsumableEffect ConsumeItem(FInventory& Inventory, const FItemCatalog& Catalog, FSlotRef Slot)
	{
		const FItemStack* Stack = Inventory.GetSlot(Slot);
		const FItemDefinition* Definition = Stack && !Stack->IsEmpty() ? Catalog.Find(Stack->ItemId) : nullptr;
		if (!Definition || Definition->Consumable.IsEmpty())
		{
			return FConsumableEffect();
		}
		const FConsumableEffect Effect = Definition->Consumable;
		if (Inventory.RemoveAt(Slot, 1) != EInventoryResult::Ok)
		{
			return FConsumableEffect();
		}
		return Effect;
	}
}
