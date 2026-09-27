// DARK BLOOD - Rules Core: equipped gear and the death penalty.
#pragma once

#include "DarkBloodRules/Inventory.h"

#include <array>

namespace DarkBlood::Rules
{
	struct FEquipContext
	{
		int32 CharacterLevel = 1;
		std::string ClassId;
	};

	class FEquipment
	{
	public:
		static constexpr int32 NumSlots = static_cast<int32>(EEquipSlot::Count);

		const FItemStack& Get(EEquipSlot Slot) const { return Slots[static_cast<size_t>(Slot)]; }
		FItemStack& GetMutable(EEquipSlot Slot) { return Slots[static_cast<size_t>(Slot)]; }

		/** Average item level over all gear slots (empty slots count as 0). */
		DARKBLOODRULES_API float ComputeGearScore(const FItemCatalog& Catalog) const;

	private:
		std::array<FItemStack, static_cast<size_t>(EEquipSlot::Count)> Slots{};
	};

	/** True if an item authored for DefinitionSlot may be placed into TargetSlot. */
	DARKBLOODRULES_API bool IsCompatibleEquipSlot(EEquipSlot DefinitionSlot, EEquipSlot TargetSlot);

	/** Sum of the stat bonuses of all equipped items. Broken items (durability 0) contribute nothing. */
	DARKBLOODRULES_API FItemStats ComputeEquipmentStats(const FEquipment& Equipment, const FItemCatalog& Catalog);

	DARKBLOODRULES_API EInventoryResult CanEquip(const FItemDefinition& Definition, EEquipSlot TargetSlot, const FEquipContext& Context);

	/** Moves one item from the inventory into an equipment slot; a previously equipped item goes back into the inventory. Atomic. */
	DARKBLOODRULES_API EInventoryResult EquipFromInventory(FInventory& Inventory, FEquipment& Equipment, const FItemCatalog& Catalog,
		FSlotRef From, EEquipSlot TargetSlot, const FEquipContext& Context);

	/** Moves an equipped item back into the inventory. Fails without change if there is no space. */
	DARKBLOODRULES_API EInventoryResult UnequipToInventory(FInventory& Inventory, FEquipment& Equipment, const FItemCatalog& Catalog,
		EEquipSlot Slot);

	struct FDeathPenaltyRules
	{
		float CurrencyLossFraction = 0.05f;
		int64 MaxCurrencyLoss = 500;
		/** Percentage points of max durability lost on equipped gear. */
		float DurabilityLossFraction = 0.05f;
	};

	struct FDeathPenaltyResult
	{
		int64 CurrencyLost = 0;
		int32 ItemsDamaged = 0;
	};

	/**
	 * Applies the moderate death penalty. By design the inventory is NOT touched:
	 * players keep weapons, armor, bags, materials, boss loot, quest items and food.
	 */
	DARKBLOODRULES_API FDeathPenaltyResult ApplyDeathPenalty(int64& Currency, FEquipment& Equipment, const FItemCatalog& Catalog,
		const FDeathPenaltyRules& Rules = FDeathPenaltyRules());
}
