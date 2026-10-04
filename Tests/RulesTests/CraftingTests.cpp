#include "TestFramework.h"

#include "DarkBloodRules/Crafting.h"

using namespace DarkBlood::Rules;

namespace
{
	const FItemCatalog& EconomyCatalog()
	{
		static const FItemCatalog Catalog = []
		{
			FItemCatalog C;
			FItemDefinition Katana;
			Katana.Id = "Katana_Forged";
			Katana.Category = EItemCategory::Weapon;
			Katana.EquipSlot = EEquipSlot::MainHand;
			Katana.MaxDurability = 100;
			Katana.BaseValue = 400;
			Katana.Stats.AttackPower = 12.f;
			Katana.Stats.CritChance = 0.03f;
			C.Add(Katana);

			FItemDefinition Helm;
			Helm.Id = "Helm_Iron";
			Helm.Category = EItemCategory::Armor;
			Helm.EquipSlot = EEquipSlot::Head;
			Helm.MaxDurability = 80;
			Helm.BaseValue = 120;
			Helm.Stats.Armor = 15.f;
			Helm.Stats.MaxHealth = 20.f;
			Helm.Stats.Resistances[static_cast<int32>(EDamageType::Fire)] = 0.1f;
			C.Add(Helm);

			FItemDefinition Ore;
			Ore.Id = "Tamahagane";
			Ore.Category = EItemCategory::Material;
			Ore.MaxStack = 99;
			C.Add(Ore);

			FItemDefinition Horn = Ore;
			Horn.Id = "DemonHorn";
			C.Add(Horn);

			FItemDefinition Potion;
			Potion.Id = "HealingDraught";
			Potion.Category = EItemCategory::Potion;
			Potion.MaxStack = 10;
			Potion.Consumable.Heal = 80.f;
			C.Add(Potion);

			FItemDefinition Junk;
			Junk.Id = "Pebble";
			Junk.Category = EItemCategory::Misc;
			Junk.MaxStack = 1;
			C.Add(Junk);
			return C;
		}();
		return Catalog;
	}

	FItemStack Stack(const char* Id, int32 Count, int32 Durability = -1, uint64 Instance = 0)
	{
		FItemStack Result;
		Result.ItemId = Id;
		Result.Count = Count;
		Result.Durability = Durability;
		Result.InstanceId = Instance;
		return Result;
	}

	FRecipe KatanaRecipe()
	{
		FRecipe Recipe;
		Recipe.Id = "Forge_Katana";
		Recipe.OutputItemId = "Katana_Forged";
		Recipe.Inputs = {{"Tamahagane", 5}, {"DemonHorn", 1}};
		Recipe.CurrencyCost = 50;
		Recipe.RequiredLevel = 5;
		Recipe.StationId = "Forge";
		return Recipe;
	}
}

DB_TEST(Economy_EquipmentStatsSumAndBrokenItemsCountZero)
{
	const FItemCatalog& Catalog = EconomyCatalog();
	FEquipment Equipment;
	Equipment.GetMutable(EEquipSlot::MainHand) = Stack("Katana_Forged", 1, 100, 1);
	Equipment.GetMutable(EEquipSlot::Head) = Stack("Helm_Iron", 1, 80, 2);

	FItemStats Stats = ComputeEquipmentStats(Equipment, Catalog);
	DB_CHECK_NEAR(Stats.AttackPower, 12.f, 0.001);
	DB_CHECK_NEAR(Stats.Armor, 15.f, 0.001);
	DB_CHECK_NEAR(Stats.MaxHealth, 20.f, 0.001);
	DB_CHECK_NEAR(Stats.CritChance, 0.03f, 0.0001);
	DB_CHECK_NEAR(Stats.Resistances[static_cast<int32>(EDamageType::Fire)], 0.1f, 0.0001);

	Equipment.GetMutable(EEquipSlot::Head).Durability = 0; // broken helmet protects nothing
	Stats = ComputeEquipmentStats(Equipment, Catalog);
	DB_CHECK_NEAR(Stats.Armor, 0.f, 0.001);
	DB_CHECK_NEAR(Stats.AttackPower, 12.f, 0.001);
	DB_CHECK(ComputeEquipmentStats(FEquipment(), Catalog).IsZero());
}

DB_TEST(Economy_LootIsDeterministicWeightedAndGuaranteed)
{
	FLootTable Table;
	Table.Id = "LesserDemon";
	Table.Guaranteed = {Stack("DemonHorn", 1)};
	Table.Entries = {{"Tamahagane", 3, 1, 3}, {"HealingDraught", 1, 1, 1}};
	Table.Rolls = 2;
	Table.NothingWeight = 1;
	Table.CurrencyMin = 5;
	Table.CurrencyMax = 15;
	DB_CHECK(ValidateLootTable(Table, EconomyCatalog()));

	FLootRandom A(1234), B(1234);
	const FLootResult First = RollLoot(Table, A);
	const FLootResult Second = RollLoot(Table, B);
	DB_CHECK(First.Items == Second.Items); // same seed, same loot
	DB_CHECK_EQ(First.Currency, Second.Currency);
	DB_CHECK(First.Currency >= 5 && First.Currency <= 15);
	DB_CHECK(!First.Items.empty() && First.Items[0].ItemId == "DemonHorn"); // guaranteed first

	// Many rolls: weights hold (Tamahagane ~3x as often as the potion, "nothing" possible) and counts stay in range.
	FLootRandom Random(99);
	int32 Ore = 0, Potions = 0;
	for (int32 Run = 0; Run < 2000; ++Run)
	{
		for (const FItemStack& Item : RollLoot(Table, Random).Items)
		{
			if (Item.ItemId == "Tamahagane")
			{
				DB_CHECK(Item.Count >= 1 && Item.Count <= 6); // two rolls of 1-3 merged
				++Ore;
			}
			else if (Item.ItemId == "HealingDraught")
			{
				++Potions;
			}
		}
	}
	DB_CHECK(Ore > Potions * 2 && Ore < Potions * 4);

	FLootTable Broken = Table;
	Broken.Entries.push_back({"NoSuchItem", 1, 1, 1});
	DB_CHECK(!ValidateLootTable(Broken, EconomyCatalog()));
}

DB_TEST(Economy_CraftingIsTransactional)
{
	const FItemCatalog& Catalog = EconomyCatalog();
	const FRecipe Recipe = KatanaRecipe();
	FInventory Inventory(3);
	int64 Mon = 100;
	DB_CHECK_EQ(Inventory.Add(Catalog, Stack("Tamahagane", 6)).Result, EInventoryResult::Ok);

	DB_CHECK_EQ(CanCraft(Recipe, Inventory, Catalog, Mon, 10, "Forge"), ECraftResult::MissingIngredients);
	DB_CHECK_EQ(Inventory.Add(Catalog, Stack("DemonHorn", 1)).Result, EInventoryResult::Ok);
	DB_CHECK_EQ(CanCraft(Recipe, Inventory, Catalog, Mon, 10, ""), ECraftResult::WrongStation);
	DB_CHECK_EQ(CanCraft(Recipe, Inventory, Catalog, Mon, 3, "Forge"), ECraftResult::LevelTooLow);
	DB_CHECK_EQ(CanCraft(Recipe, Inventory, Catalog, 10, 10, "Forge"), ECraftResult::NotEnoughCurrency);

	// Fill the last slot: the horn's slot frees up when it is consumed, so crafting still fits.
	DB_CHECK_EQ(Inventory.Add(Catalog, Stack("Pebble", 1)).Result, EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.FreeSlots(), 0);
	DB_CHECK_EQ(Craft(Recipe, Inventory, Mon, Catalog, 10, "Forge", 777), ECraftResult::Ok);
	DB_CHECK_EQ(Mon, int64(50));
	DB_CHECK_EQ(Inventory.CountItem("Tamahagane"), 1);
	DB_CHECK_EQ(Inventory.CountItem("DemonHorn"), 0);
	DB_CHECK_EQ(Inventory.CountItem("Katana_Forged"), 1);

	// No space at all: nothing is consumed, no Mon spent.
	FInventory Full(2);
	int64 Rich = 1000;
	DB_CHECK_EQ(Full.Add(Catalog, Stack("Tamahagane", 99)).Result, EInventoryResult::Ok);
	DB_CHECK_EQ(Full.Add(Catalog, Stack("Tamahagane", 99)).Result, EInventoryResult::Ok);
	FRecipe OreOnly = Recipe;
	OreOnly.Inputs = {{"Tamahagane", 5}};
	DB_CHECK_EQ(Craft(OreOnly, Full, Rich, Catalog, 10, "Forge", 1), ECraftResult::NoSpace);
	DB_CHECK_EQ(Full.CountItem("Tamahagane"), 198);
	DB_CHECK_EQ(Rich, int64(1000));
}

DB_TEST(Economy_RepairAndConsumables)
{
	const FItemCatalog& Catalog = EconomyCatalog();
	const FItemDefinition* Katana = Catalog.Find("Katana_Forged");
	DB_CHECK_EQ(RepairCost(*Katana, 100), int64(0));
	DB_CHECK_EQ(RepairCost(*Katana, 50), int64(50)); // half of 25 % of 400
	DB_CHECK_EQ(RepairCost(*Katana, 0), int64(100));
	DB_CHECK_EQ(RepairCost(*Catalog.Find("Helm_Iron"), 79), int64(1)); // minimum price 10 -> ceil(0.125)

	FEquipment Equipment;
	Equipment.GetMutable(EEquipSlot::MainHand) = Stack("Katana_Forged", 1, 50, 1);
	Equipment.GetMutable(EEquipSlot::Head) = Stack("Helm_Iron", 1, 40, 2);
	int64 Mon = 60; // enough for the helmet (15) but not both (15 + 50)
	ECraftResult Result = ECraftResult::InvalidRecipe;
	DB_CHECK_EQ(RepairEquipment(Equipment, Mon, Catalog, &Result), int64(15));
	DB_CHECK_EQ(Result, ECraftResult::Ok);
	DB_CHECK_EQ(Equipment.Get(EEquipSlot::Head).Durability, 80);
	DB_CHECK_EQ(Equipment.Get(EEquipSlot::MainHand).Durability, 50);
	DB_CHECK_EQ(Mon, int64(45));
	Mon = 1000;
	RepairEquipment(Equipment, Mon, Catalog, &Result);
	RepairEquipment(Equipment, Mon, Catalog, &Result);
	DB_CHECK_EQ(Result, ECraftResult::NothingToRepair);

	FInventory Inventory(5);
	DB_CHECK_EQ(Inventory.Add(Catalog, Stack("HealingDraught", 2)).Result, EInventoryResult::Ok);
	DB_CHECK_EQ(Inventory.Add(Catalog, Stack("Tamahagane", 1)).Result, EInventoryResult::Ok);
	const FConsumableEffect Effect = ConsumeItem(Inventory, Catalog, {0, 0});
	DB_CHECK_NEAR(Effect.Heal, 80.f, 0.001);
	DB_CHECK_EQ(Inventory.CountItem("HealingDraught"), 1);
	DB_CHECK(ConsumeItem(Inventory, Catalog, {0, 1}).IsEmpty()); // ore is not edible
	DB_CHECK_EQ(Inventory.CountItem("Tamahagane"), 1);
	DB_CHECK(ConsumeItem(Inventory, Catalog, {0, 4}).IsEmpty()); // empty slot
}
