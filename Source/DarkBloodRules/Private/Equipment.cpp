#include "DarkBloodRules/Equipment.h"

#include <algorithm>
#include <cmath>

namespace DarkBlood::Rules
{
	float FEquipment::ComputeGearScore(const FItemCatalog& Catalog) const
	{
		int32 Total = 0;
		int32 GearSlots = 0;
		for (int32 Index = 1; Index < NumSlots; ++Index) // skip EEquipSlot::None
		{
			++GearSlots;
			const FItemStack& Stack = Slots[static_cast<size_t>(Index)];
			if (Stack.IsEmpty())
			{
				continue;
			}
			if (const FItemDefinition* Definition = Catalog.Find(Stack.ItemId))
			{
				Total += Definition->ItemLevel;
			}
		}
		return GearSlots > 0 ? static_cast<float>(Total) / static_cast<float>(GearSlots) : 0.f;
	}

	bool IsCompatibleEquipSlot(EEquipSlot DefinitionSlot, EEquipSlot TargetSlot)
	{
		if (DefinitionSlot == EEquipSlot::None || TargetSlot == EEquipSlot::None || TargetSlot == EEquipSlot::Count)
		{
			return false;
		}
		const bool bDefinitionIsAccessory = DefinitionSlot == EEquipSlot::Accessory1 || DefinitionSlot == EEquipSlot::Accessory2;
		const bool bTargetIsAccessory = TargetSlot == EEquipSlot::Accessory1 || TargetSlot == EEquipSlot::Accessory2;
		if (bDefinitionIsAccessory || bTargetIsAccessory)
		{
			return bDefinitionIsAccessory && bTargetIsAccessory;
		}
		return DefinitionSlot == TargetSlot;
	}

	EInventoryResult CanEquip(const FItemDefinition& Definition, EEquipSlot TargetSlot, const FEquipContext& Context)
	{
		if (Definition.EquipSlot == EEquipSlot::None)
		{
			return EInventoryResult::NotEquippable;
		}
		if (!IsCompatibleEquipSlot(Definition.EquipSlot, TargetSlot))
		{
			return EInventoryResult::WrongEquipSlot;
		}
		if (Context.CharacterLevel < Definition.RequiredLevel)
		{
			return EInventoryResult::LevelTooLow;
		}
		if (!Definition.AllowedClasses.empty() &&
			std::find(Definition.AllowedClasses.begin(), Definition.AllowedClasses.end(), Context.ClassId) ==
				Definition.AllowedClasses.end())
		{
			return EInventoryResult::ClassNotAllowed;
		}
		return EInventoryResult::Ok;
	}

	EInventoryResult EquipFromInventory(FInventory& Inventory, FEquipment& Equipment, const FItemCatalog& Catalog,
		FSlotRef From, EEquipSlot TargetSlot, const FEquipContext& Context)
	{
		const FItemStack* Source = Inventory.GetSlot(From);
		if (!Source)
		{
			return EInventoryResult::InvalidSlot;
		}
		if (Source->IsEmpty())
		{
			return EInventoryResult::EmptySlot;
		}
		const FItemDefinition* Definition = Catalog.Find(Source->ItemId);
		if (!Definition)
		{
			return EInventoryResult::UnknownItem;
		}
		const EInventoryResult Allowed = CanEquip(*Definition, TargetSlot, Context);
		if (Allowed != EInventoryResult::Ok)
		{
			return Allowed;
		}

		FInventory NextInventory = Inventory;
		FItemStack Taken;
		NextInventory.RemoveAt(From, 1, &Taken);

		FItemStack Previous = Equipment.Get(TargetSlot);
		if (!Previous.IsEmpty())
		{
			// Previously equipped items are always known to the catalog unless content was removed;
			// place them directly so they are never lost even if their definition vanished.
			const FAddResult Returned = NextInventory.Add(Catalog, Previous, EAddMode::AllOrNothing);
			if (Returned.Result != EInventoryResult::Ok)
			{
				return Returned.Result == EInventoryResult::UnknownItem ? EInventoryResult::UnknownItem
																		 : EInventoryResult::InsufficientSpace;
			}
		}

		Inventory = std::move(NextInventory);
		Equipment.GetMutable(TargetSlot) = std::move(Taken);
		return EInventoryResult::Ok;
	}

	EInventoryResult UnequipToInventory(FInventory& Inventory, FEquipment& Equipment, const FItemCatalog& Catalog, EEquipSlot Slot)
	{
		if (Slot == EEquipSlot::None || Slot == EEquipSlot::Count)
		{
			return EInventoryResult::InvalidSlot;
		}
		FItemStack& Equipped = Equipment.GetMutable(Slot);
		if (Equipped.IsEmpty())
		{
			return EInventoryResult::EmptySlot;
		}
		const FAddResult Returned = Inventory.Add(Catalog, Equipped, EAddMode::AllOrNothing);
		if (Returned.Result != EInventoryResult::Ok)
		{
			return Returned.Result;
		}
		Equipped = FItemStack();
		return EInventoryResult::Ok;
	}

	FDeathPenaltyResult ApplyDeathPenalty(int64& Currency, FEquipment& Equipment, const FItemCatalog& Catalog,
		const FDeathPenaltyRules& Rules)
	{
		FDeathPenaltyResult Result;
		if (Currency > 0)
		{
			const int64 Loss = static_cast<int64>(std::floor(static_cast<double>(Currency) * Rules.CurrencyLossFraction));
			Result.CurrencyLost = std::clamp<int64>(Loss, 0, std::min(Rules.MaxCurrencyLoss, Currency));
			Currency -= Result.CurrencyLost;
		}

		for (int32 Index = 1; Index < FEquipment::NumSlots; ++Index)
		{
			FItemStack& Stack = Equipment.GetMutable(static_cast<EEquipSlot>(Index));
			if (Stack.IsEmpty() || Stack.Durability < 0)
			{
				continue;
			}
			const FItemDefinition* Definition = Catalog.Find(Stack.ItemId);
			if (!Definition || Definition->MaxDurability <= 0)
			{
				continue;
			}
			const int32 Loss = std::max(1, static_cast<int32>(std::lround(Definition->MaxDurability * Rules.DurabilityLossFraction)));
			const int32 Before = Stack.Durability;
			Stack.Durability = std::max(0, Stack.Durability - Loss);
			Result.ItemsDamaged += Stack.Durability != Before ? 1 : 0;
		}
		return Result;
	}
}
