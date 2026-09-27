// DARK BLOOD - Rules Core: item definitions and item stacks.
#pragma once

#include "DarkBloodRules/RulesCore.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace DarkBlood::Rules
{
	enum class EItemCategory : uint8
	{
		Weapon,
		Armor,
		Bag,
		Material,
		Food,
		Potion,
		Scroll,
		Magic,
		Consumable,
		Quest,
		Misc,
		Count
	};

	using FCategoryMask = uint32;

	constexpr FCategoryMask CategoryBit(EItemCategory Category)
	{
		return FCategoryMask(1u) << static_cast<uint32>(Category);
	}

	constexpr FCategoryMask AllCategories = (FCategoryMask(1u) << static_cast<uint32>(EItemCategory::Count)) - 1u;

	enum class EItemRarity : uint8
	{
		Common,    // gewöhnlich
		Uncommon,  // ungewöhnlich
		Rare,      // selten
		Epic,      // episch
		Legendary, // legendär
		Demonic,   // dämonisch
	};

	enum class EEquipSlot : uint8
	{
		None,
		MainHand,
		OffHand,
		Head,
		Chest,
		Hands,
		Legs,
		Feet,
		Accessory1,
		Accessory2,
		Count
	};

	/** Bags occupy dedicated bag slots in the inventory (see FInventory), not equipment slots. */
	enum class EBagKind : uint8
	{
		General,    // Reisetasche / Rucksack
		Materials,  // Materialtasche
		Provisions, // Provianttasche
		Scrolls,    // Schriftrollentasche
		Loot,       // Beutetasche
		Count
	};

	struct FBagSpec
	{
		EBagKind Kind = EBagKind::General;
		int32 Capacity = 0;
		/** Categories this bag accepts. General bags accept everything except other bags' restrictions. */
		FCategoryMask AcceptedCategories = AllCategories;
	};

	struct FItemDefinition
	{
		std::string Id;
		EItemCategory Category = EItemCategory::Misc;
		EItemRarity Rarity = EItemRarity::Common;
		int32 MaxStack = 1;
		int32 ItemLevel = 0;
		int32 RequiredLevel = 1;
		int32 MaxDurability = 0; // 0 = no durability
		EEquipSlot EquipSlot = EEquipSlot::None;
		/** Class ids allowed to equip; empty = every class. */
		std::vector<std::string> AllowedClasses;
		bool bIsBag = false;
		FBagSpec Bag;
	};

	/** Lookup of item definitions. In UE this is filled from item data assets. */
	class FItemCatalog
	{
	public:
		DARKBLOODRULES_API bool Add(FItemDefinition Definition);
		DARKBLOODRULES_API const FItemDefinition* Find(std::string_view Id) const;
		size_t Num() const { return Definitions.size(); }

	private:
		std::unordered_map<std::string, FItemDefinition> Definitions;
	};

	struct FItemStack
	{
		std::string ItemId;
		int32 Count = 0;
		/** Non-zero for unique item instances (e.g. forged weapons). Instances never stack. */
		uint64 InstanceId = 0;
		/** Current durability; -1 when the item has none. */
		int32 Durability = -1;

		bool IsEmpty() const { return Count <= 0 || ItemId.empty(); }
		DARKBLOODRULES_API bool CanStackWith(const FItemStack& Other) const;
		bool operator==(const FItemStack& Other) const
		{
			return ItemId == Other.ItemId && Count == Other.Count && InstanceId == Other.InstanceId &&
				Durability == Other.Durability;
		}
	};
}
