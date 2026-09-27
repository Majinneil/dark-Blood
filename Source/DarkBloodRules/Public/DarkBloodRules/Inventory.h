// DARK BLOOD - Rules Core: slot-based inventory with bags.
//
// Layout:
//   Section 0                        base pouch (fixed capacity, accepts everything)
//   Section 1 + EBagKind::General    equipped travel bag / backpack (+9 / +18 / +27 ...)
//   Section 1 + EBagKind::<Special>  equipped special bag (materials, provisions, scrolls, loot)
//   Quest items                      separate, unlimited list; they never occupy or block slots
//
// Invariant: no operation may ever destroy an item implicitly. Every multi-step
// operation (bag swaps, equipping) runs on a copy and is committed only on success.
#pragma once

#include "DarkBloodRules/Items.h"

#include <vector>

namespace DarkBlood::Rules
{
	enum class EInventoryResult : uint8
	{
		Ok,
		InvalidSlot,
		EmptySlot,
		InvalidCount,
		UnknownItem,
		NotABag,
		NotEquippable,
		WrongEquipSlot,
		LevelTooLow,
		ClassNotAllowed,
		CategoryNotAccepted,
		InsufficientSpace,
		NotEnoughItems,
		SlotOccupied,
	};

	DARKBLOODRULES_API const char* ToString(EInventoryResult Result);

	struct FSlotRef
	{
		int32 Section = 0;
		int32 Index = 0;
	};

	struct FInventorySection
	{
		EBagKind Kind = EBagKind::General;
		bool bIsBasePouch = false;
		/** The bag item providing this section's slots. Empty for the base pouch or when no bag is equipped. */
		FItemStack BagItem;
		FCategoryMask AcceptedCategories = AllCategories;
		/** Empty stacks represent free slots. */
		std::vector<FItemStack> Slots;

		int32 Capacity() const { return static_cast<int32>(Slots.size()); }
		bool Accepts(EItemCategory Category) const { return (AcceptedCategories & CategoryBit(Category)) != 0; }
	};

	enum class EAddMode : uint8
	{
		AllOrNothing,
		Partial,
	};

	struct FAddResult
	{
		EInventoryResult Result = EInventoryResult::Ok;
		int32 Added = 0;
		int32 Remaining = 0;
	};

	class FInventory
	{
	public:
		static constexpr int32 BasePouchSection = 0;
		static constexpr int32 DefaultBasePouchCapacity = 20;

		DARKBLOODRULES_API explicit FInventory(int32 BasePouchCapacity = DefaultBasePouchCapacity);

		static constexpr int32 SectionForBag(EBagKind Kind) { return 1 + static_cast<int32>(Kind); }

		int32 NumSections() const { return static_cast<int32>(Sections.size()); }
		const FInventorySection& GetSection(int32 Section) const { return Sections[Section]; }
		const std::vector<FItemStack>& GetQuestItems() const { return QuestItems; }

		DARKBLOODRULES_API const FItemStack* GetSlot(FSlotRef Ref) const;
		DARKBLOODRULES_API int32 TotalCapacity() const;
		DARKBLOODRULES_API int32 FreeSlots() const;
		DARKBLOODRULES_API int32 CountItem(std::string_view ItemId) const;

		/** Adds items. Merges into existing stacks first, then fills special bags, the general bag and the base pouch. */
		DARKBLOODRULES_API FAddResult Add(const FItemCatalog& Catalog, const FItemStack& Stack, EAddMode Mode = EAddMode::AllOrNothing);

		/** Removes Count items of ItemId across all stacks. All or nothing. */
		DARKBLOODRULES_API EInventoryResult Remove(std::string_view ItemId, int32 Count);

		/** Removes Count items from one slot (Count <= 0 removes the whole stack). */
		DARKBLOODRULES_API EInventoryResult RemoveAt(FSlotRef Ref, int32 Count, FItemStack* OutRemoved = nullptr);

		/** Moves Count items (<= 0: whole stack). Merges, moves into free slots or swaps whole stacks. */
		DARKBLOODRULES_API EInventoryResult Move(const FItemCatalog& Catalog, FSlotRef From, FSlotRef To, int32 Count = 0);

		/**
		 * Equips the bag in the given slot into the bag slot of its kind. Items of the previous bag are
		 * relocated (preferring the new bag) and the previous bag is returned to the inventory.
		 * Fails without any change if anything would not fit.
		 */
		DARKBLOODRULES_API EInventoryResult EquipBag(const FItemCatalog& Catalog, FSlotRef BagSlot);

		/** Removes the bag of the given kind, relocating its content. Fails without change if not everything fits. */
		DARKBLOODRULES_API EInventoryResult UnequipBag(const FItemCatalog& Catalog, EBagKind Kind);

		/** Every item stack including quest items and equipped bags (for audits and tests). */
		DARKBLOODRULES_API std::vector<FItemStack> CollectAllItems() const;

		/** Raw restore used by deserialization. Validates structure only. */
		DARKBLOODRULES_API bool RestoreRaw(std::vector<FInventorySection> InSections, std::vector<FItemStack> InQuestItems);

	private:
		FItemDefinition ResolveDefinition(const FItemCatalog& Catalog, const FItemStack& Stack) const;
		bool IsValidRef(FSlotRef Ref) const;
		/** Places a whole stack, trying PreferredSection first. Returns number of items that did not fit. */
		int32 Place(const FItemCatalog& Catalog, FItemStack Stack, int32 PreferredSection);
		int32 MergeIntoSection(int32 Section, FItemStack& Stack, int32 MaxStack);
		int32 FillEmptyInSection(int32 Section, FItemStack& Stack, int32 MaxStack);
		std::vector<int32> PlacementOrder(EItemCategory Category, int32 PreferredSection) const;

		std::vector<FInventorySection> Sections;
		std::vector<FItemStack> QuestItems;
	};
}
