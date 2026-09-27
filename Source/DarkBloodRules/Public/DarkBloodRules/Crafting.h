// DARK BLOOD - Rules Core: loot tables, crafting, repair and consumables.
// Everything here is transactional: a failed operation leaves inventory and currency untouched, and
// nothing is ever destroyed because a bag is full. Randomness comes from an explicit, seedable stream.
#pragma once

#include "DarkBloodRules/Equipment.h"
#include "DarkBloodRules/Inventory.h"

#include <string>
#include <vector>

namespace DarkBlood::Rules
{
	// ---- Random stream --------------------------------------------------------------------------

	/** Small deterministic generator (xorshift64*); the server seeds it, tests use fixed seeds. */
	class FLootRandom
	{
	public:
		DARKBLOODRULES_API explicit FLootRandom(uint64 Seed);
		DARKBLOODRULES_API uint32 Next();
		/** Uniform integer in [Min, Max] (inclusive). */
		DARKBLOODRULES_API int32 Range(int32 Min, int32 Max);

	private:
		uint64 State;
	};

	// ---- Loot -----------------------------------------------------------------------------------

	struct FLootEntry
	{
		std::string ItemId;
		int32 Weight = 1;
		int32 MinCount = 1;
		int32 MaxCount = 1;
	};

	struct FLootTable
	{
		std::string Id;
		/** Always dropped (bosses, story rewards: never RNG-only). */
		std::vector<FItemStack> Guaranteed;
		std::vector<FLootEntry> Entries;
		/** Independent weighted rolls on Entries. */
		int32 Rolls = 1;
		/** Weight of "nothing" in each roll. */
		int32 NothingWeight = 0;
		int64 CurrencyMin = 0;
		int64 CurrencyMax = 0;
	};

	struct FLootResult
	{
		/** Merged by item id (Count summed). */
		std::vector<FItemStack> Items;
		int64 Currency = 0;
	};

	DARKBLOODRULES_API FLootResult RollLoot(const FLootTable& Table, FLootRandom& Random);

	/** True if every referenced item exists and weights/counts are sane. */
	DARKBLOODRULES_API bool ValidateLootTable(const FLootTable& Table, const FItemCatalog& Catalog);

	// ---- Crafting -------------------------------------------------------------------------------

	struct FRecipeIngredient
	{
		std::string ItemId;
		int32 Count = 1;
	};

	struct FRecipe
	{
		std::string Id;
		std::string OutputItemId;
		int32 OutputCount = 1;
		std::vector<FRecipeIngredient> Inputs;
		int64 CurrencyCost = 0;
		int32 RequiredLevel = 1;
		/** Station required (e.g. "Forge"); empty = craftable anywhere. */
		std::string StationId;
	};

	enum class ECraftResult : uint8
	{
		Ok,
		InvalidRecipe,
		WrongStation,
		LevelTooLow,
		MissingIngredients,
		NotEnoughCurrency,
		NoSpace,
		NothingToRepair,
	};

	DARKBLOODRULES_API const char* ToString(ECraftResult Result);

	DARKBLOODRULES_API ECraftResult CanCraft(const FRecipe& Recipe, const FInventory& Inventory, const FItemCatalog& Catalog, int64 Currency,
		int32 Level, const std::string& StationId);

	/**
	 * Crafts once. Removes the inputs and the currency, adds the output (items with durability become unique
	 * instances with full durability and NewInstanceId). All-or-nothing.
	 */
	DARKBLOODRULES_API ECraftResult Craft(const FRecipe& Recipe, FInventory& Inventory, int64& Currency, const FItemCatalog& Catalog,
		int32 Level, const std::string& StationId, uint64 NewInstanceId);

	// ---- Repair ---------------------------------------------------------------------------------

	/** Mon to fully repair an item: missing durability fraction x max(10, 25 % of its value), rounded up. */
	DARKBLOODRULES_API int64 RepairCost(const FItemDefinition& Definition, int32 CurrentDurability);

	/** Repairs every damaged equipped item the player can afford, cheapest first. Returns Mon spent. */
	DARKBLOODRULES_API int64 RepairEquipment(FEquipment& Equipment, int64& Currency, const FItemCatalog& Catalog, ECraftResult* OutResult = nullptr);

	// ---- Consumables ----------------------------------------------------------------------------

	/** Uses one item from Slot. Returns the effect to apply (empty if the item is not a consumable). */
	DARKBLOODRULES_API FConsumableEffect ConsumeItem(FInventory& Inventory, const FItemCatalog& Catalog, FSlotRef Slot);
}
