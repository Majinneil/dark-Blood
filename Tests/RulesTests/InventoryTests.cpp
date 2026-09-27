#include "TestFramework.h"

#include "TestCatalog.h"

#include "DarkBloodRules/Equipment.h"

#include <algorithm>
#include <map>

using namespace DarkBlood::Rules;

namespace
{
	/** Item id -> total count, including equipped bags and quest items. Used to prove nothing vanished. */
	std::map<std::string, int> Census(const FInventory& Inventory)
	{
		std::map<std::string, int> Counts;
		for (const FItemStack& Stack : Inventory.CollectAllItems())
		{
			Counts[Stack.ItemId] += Stack.Count;
		}
		return Counts;
	}

	FSlotRef FindItem(const FInventory& Inventory, const std::string& ItemId)
	{
		for (int32 Section = 0; Section < Inventory.NumSections(); ++Section)
		{
			for (int32 Index = 0; Index < Inventory.GetSection(Section).Capacity(); ++Index)
			{
				if (Inventory.GetSection(Section).Slots[Index].ItemId == ItemId)
				{
					return {Section, Index};
				}
			}
		}
		return {-1, -1};
	}
}

DB_TEST(Inventory_AddStacksAndRespectsCapacity)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(2);

	DB_CHECK_EQ(Inventory.Add(Catalog, {"Tamahagane", 150}).Result, EInventoryResult::Ok); // 99 + 51
	DB_CHECK_EQ(Inventory.FreeSlots(), 0);
	DB_CHECK_EQ(Inventory.CountItem("Tamahagane"), 150);

	DB_CHECK_EQ(Inventory.Add(Catalog, {"Tamahagane", 48}).Result, EInventoryResult::Ok); // merges into 51
	DB_CHECK_EQ(Inventory.Add(Catalog, {"Tamahagane", 1}).Result, EInventoryResult::InsufficientSpace);
	DB_CHECK_EQ(Inventory.CountItem("Tamahagane"), 198); // all-or-nothing left state untouched

	const FAddResult Partial = Inventory.Add(Catalog, {"Tamahagane", 5}, EAddMode::Partial);
	DB_CHECK_EQ(Partial.Added, 0); // both slots already hold full stacks (99 + 99)
	DB_CHECK_EQ(Partial.Remaining, 5);
	DB_CHECK_EQ(Inventory.Add(Catalog, {"DoesNotExist", 1}).Result, EInventoryResult::UnknownItem);
}

DB_TEST(Inventory_QuestItemsNeverBlockSlots)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(0);
	DB_CHECK_EQ(Inventory.Add(Catalog, {"KingsSeal", 1}).Result, EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.CountItem("KingsSeal"), 1);
	DB_CHECK_EQ(Inventory.Remove("KingsSeal", 1), EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.CountItem("KingsSeal"), 0);
}

DB_TEST(Inventory_UniqueInstancesNeverStack)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(3);
	DB_CHECK_EQ(Inventory.Add(Catalog, {"Katana_Basic", 1, 1001, 100}).Result, EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.Add(Catalog, {"Katana_Basic", 1, 1002, 100}).Result, EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.FreeSlots(), 1);
	DB_CHECK_EQ(Inventory.Add(Catalog, {"Katana_Basic", 2, 1003, 100}).Result, EInventoryResult::InvalidCount);
}

DB_TEST(Inventory_RemoveIsAllOrNothing)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(5);
	Inventory.Add(Catalog, {"RiceBall", 15});
	DB_CHECK_EQ(Inventory.Remove("RiceBall", 16), EInventoryResult::NotEnoughItems);
	DB_CHECK_EQ(Inventory.CountItem("RiceBall"), 15);
	DB_CHECK_EQ(Inventory.Remove("RiceBall", 12), EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.CountItem("RiceBall"), 3);
	DB_CHECK_EQ(Inventory.Remove("RiceBall", 0), EInventoryResult::InvalidCount);
}

DB_TEST(Inventory_MoveMergesSplitsAndSwaps)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(4);
	Inventory.Add(Catalog, {"RiceBall", 10});
	Inventory.Add(Catalog, {"Tamahagane", 5});

	DB_CHECK_EQ(Inventory.Move(Catalog, {0, 0}, {0, 2}, 4), EInventoryResult::Ok); // split
	DB_CHECK_EQ(Inventory.GetSlot({0, 0})->Count, 6);
	DB_CHECK_EQ(Inventory.GetSlot({0, 2})->Count, 4);
	DB_CHECK_EQ(Inventory.Move(Catalog, {0, 2}, {0, 0}), EInventoryResult::Ok); // merge back
	DB_CHECK_EQ(Inventory.GetSlot({0, 0})->Count, 10);
	DB_CHECK(Inventory.GetSlot({0, 2})->IsEmpty());

	DB_CHECK_EQ(Inventory.Move(Catalog, {0, 0}, {0, 1}), EInventoryResult::Ok); // swap
	DB_CHECK_EQ(Inventory.GetSlot({0, 0})->ItemId, std::string("Tamahagane"));
	DB_CHECK_EQ(Inventory.GetSlot({0, 1})->ItemId, std::string("RiceBall"));
	DB_CHECK_EQ(Inventory.Move(Catalog, {0, 0}, {0, 1}, 2), EInventoryResult::SlotOccupied); // partial swap refused
	DB_CHECK_EQ(Inventory.Move(Catalog, {0, 0}, {0, 99}), EInventoryResult::InvalidSlot);
}

DB_TEST(Bags_EquipAddsSlotsAndUpgradeKeepsItems)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(4);
	Inventory.Add(Catalog, {"Bag_Small", 1});
	DB_CHECK_EQ(Inventory.EquipBag(Catalog, FindItem(Inventory, "Bag_Small")), EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.TotalCapacity(), 4 + 9);

	for (int Index = 0; Index < 10; ++Index)
	{
		DB_CHECK_EQ(Inventory.Add(Catalog, {"Katana_Basic", 1, uint64(2000 + Index), 100}).Result, EInventoryResult::Ok);
	}
	Inventory.Add(Catalog, {"Bag_Adventurer", 1});
	const auto Before = Census(Inventory);

	DB_CHECK_EQ(Inventory.EquipBag(Catalog, FindItem(Inventory, "Bag_Adventurer")), EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.TotalCapacity(), 4 + 18);
	DB_CHECK(Census(Inventory) == Before); // old bag returned, all katanas kept
	DB_CHECK_EQ(Inventory.GetSection(FInventory::SectionForBag(EBagKind::General)).BagItem.ItemId, std::string("Bag_Adventurer"));
	DB_CHECK_EQ(Inventory.CountItem("Bag_Small"), 1);
}

DB_TEST(Bags_DowngradeRefusedWhenItemsWouldNotFit)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(2);
	Inventory.Add(Catalog, {"Bag_Adventurer", 1});
	DB_CHECK_EQ(Inventory.EquipBag(Catalog, {0, 0}), EInventoryResult::Ok);

	// Fill 2 base + 18 bag slots, leaving no room, then try to switch to a 9-slot bag.
	for (int Index = 0; Index < 19; ++Index)
	{
		DB_CHECK_EQ(Inventory.Add(Catalog, {"Katana_Basic", 1, uint64(3000 + Index), 100}).Result, EInventoryResult::Ok);
	}
	Inventory.Add(Catalog, {"Bag_Small", 1});
	DB_CHECK_EQ(Inventory.FreeSlots(), 0);

	const auto Before = Census(Inventory);
	const FInventory Snapshot = Inventory;
	DB_CHECK_EQ(Inventory.EquipBag(Catalog, FindItem(Inventory, "Bag_Small")), EInventoryResult::InsufficientSpace);
	DB_CHECK(Census(Inventory) == Before);
	DB_CHECK_EQ(Inventory.TotalCapacity(), Snapshot.TotalCapacity());
	DB_CHECK_EQ(Inventory.UnequipBag(Catalog, EBagKind::General), EInventoryResult::InsufficientSpace);
	DB_CHECK(Census(Inventory) == Before);

	// After freeing enough space the swap succeeds and still loses nothing.
	for (int Index = 0; Index < 10; ++Index)
	{
		DB_CHECK_EQ(Inventory.Remove("Katana_Basic", 1), EInventoryResult::Ok);
	}
	const auto BeforeSwap = Census(Inventory);
	DB_CHECK_EQ(Inventory.EquipBag(Catalog, FindItem(Inventory, "Bag_Small")), EInventoryResult::Ok);
	DB_CHECK(Census(Inventory) == BeforeSwap);
	DB_CHECK_EQ(Inventory.TotalCapacity(), 2 + 9);
}

DB_TEST(Bags_SpecialBagsFilterCategories)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(3);
	Inventory.Add(Catalog, {"Bag_Materials", 1});
	DB_CHECK_EQ(Inventory.EquipBag(Catalog, {0, 0}), EInventoryResult::Ok);

	Inventory.Add(Catalog, {"Tamahagane", 10});
	const int32 MaterialSection = FInventory::SectionForBag(EBagKind::Materials);
	DB_CHECK_EQ(Inventory.GetSection(MaterialSection).Slots[0].ItemId, std::string("Tamahagane")); // routed into the material bag

	Inventory.Add(Catalog, {"RiceBall", 1});
	DB_CHECK_EQ(Inventory.Move(Catalog, FindItem(Inventory, "RiceBall"), {MaterialSection, 1}), EInventoryResult::CategoryNotAccepted);
}

DB_TEST(Equipment_EquipSwapUnequipAndRequirements)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(2);
	FEquipment Equipment;
	const FEquipContext Warrior{5, "Warrior"};

	Inventory.Add(Catalog, {"Katana_Basic", 1, 11, 100});
	Inventory.Add(Catalog, {"Nodachi_Demon", 1, 12, 200});
	DB_CHECK_EQ(EquipFromInventory(Inventory, Equipment, Catalog, {0, 0}, EEquipSlot::Head, Warrior), EInventoryResult::WrongEquipSlot);
	DB_CHECK_EQ(EquipFromInventory(Inventory, Equipment, Catalog, {0, 1}, EEquipSlot::MainHand, Warrior), EInventoryResult::LevelTooLow);
	DB_CHECK_EQ(EquipFromInventory(Inventory, Equipment, Catalog, {0, 0}, EEquipSlot::MainHand, {5, "Mage"}), EInventoryResult::ClassNotAllowed);

	DB_CHECK_EQ(EquipFromInventory(Inventory, Equipment, Catalog, {0, 0}, EEquipSlot::MainHand, Warrior), EInventoryResult::Ok);
	DB_CHECK_EQ(Equipment.Get(EEquipSlot::MainHand).InstanceId, uint64(11));
	DB_CHECK(Inventory.GetSlot({0, 0})->IsEmpty());

	const FEquipContext Veteran{40, "Warrior"};
	DB_CHECK_EQ(EquipFromInventory(Inventory, Equipment, Catalog, {0, 1}, EEquipSlot::MainHand, Veteran), EInventoryResult::Ok);
	DB_CHECK_EQ(Equipment.Get(EEquipSlot::MainHand).InstanceId, uint64(12));
	DB_CHECK_EQ(Inventory.CountItem("Katana_Basic"), 1); // swapped back into the inventory

	Inventory.Add(Catalog, {"RiceBall", 1}); // inventory now full (2 slots)
	DB_CHECK_EQ(UnequipToInventory(Inventory, Equipment, Catalog, EEquipSlot::MainHand), EInventoryResult::InsufficientSpace);
	DB_CHECK_EQ(Equipment.Get(EEquipSlot::MainHand).InstanceId, uint64(12)); // still equipped, not lost
}

DB_TEST(Death_KeepsAllItemsAndAppliesModeratePenalty)
{
	const FItemCatalog& Catalog = TestCatalog();
	FInventory Inventory(10);
	FEquipment Equipment;
	Inventory.Add(Catalog, {"Katana_Basic", 1, 21, 100});
	Inventory.Add(Catalog, {"Tamahagane", 40});
	Inventory.Add(Catalog, {"KingsSeal", 1});
	Inventory.Add(Catalog, {"RiceBall", 5});
	EquipFromInventory(Inventory, Equipment, Catalog, FindItem(Inventory, "Katana_Basic"), EEquipSlot::MainHand, {5, "Warrior"});
	const auto Before = Census(Inventory);

	int64 Currency = 1000;
	const FDeathPenaltyResult Result = ApplyDeathPenalty(Currency, Equipment, Catalog);
	DB_CHECK_EQ(Result.CurrencyLost, int64(50));
	DB_CHECK_EQ(Currency, int64(950));
	DB_CHECK_EQ(Result.ItemsDamaged, 1);
	DB_CHECK_EQ(Equipment.Get(EEquipSlot::MainHand).Durability, 95);
	DB_CHECK(Census(Inventory) == Before);

	int64 Rich = 1'000'000;
	DB_CHECK_EQ(ApplyDeathPenalty(Rich, Equipment, Catalog).CurrencyLost, int64(500)); // capped
	int64 Broke = 0;
	DB_CHECK_EQ(ApplyDeathPenalty(Broke, Equipment, Catalog).CurrencyLost, int64(0));
}
