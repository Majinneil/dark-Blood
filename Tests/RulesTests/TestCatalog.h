// Shared test fixtures. Item ids mirror the development content in docs/CONTENT_PIPELINE.md.
#pragma once

#include "DarkBloodRules/Items.h"

inline const DarkBlood::Rules::FItemCatalog& TestCatalog()
{
	using namespace DarkBlood::Rules;
	static const FItemCatalog Catalog = []
	{
		FItemCatalog C;

		FItemDefinition Katana;
		Katana.Id = "Katana_Basic";
		Katana.Category = EItemCategory::Weapon;
		Katana.ItemLevel = 5;
		Katana.MaxDurability = 100;
		Katana.EquipSlot = EEquipSlot::MainHand;
		Katana.AllowedClasses = {"Warrior", "Shadowrunner"};
		C.Add(Katana);

		FItemDefinition Nodachi = Katana;
		Nodachi.Id = "Nodachi_Demon";
		Nodachi.Rarity = EItemRarity::Demonic;
		Nodachi.ItemLevel = 40;
		Nodachi.RequiredLevel = 35;
		Nodachi.MaxDurability = 200;
		Nodachi.AllowedClasses = {"Warrior"};
		C.Add(Nodachi);

		FItemDefinition Ore;
		Ore.Id = "Tamahagane";
		Ore.Category = EItemCategory::Material;
		Ore.MaxStack = 99;
		C.Add(Ore);

		FItemDefinition Food;
		Food.Id = "RiceBall";
		Food.Category = EItemCategory::Food;
		Food.MaxStack = 20;
		C.Add(Food);

		FItemDefinition Seal;
		Seal.Id = "KingsSeal";
		Seal.Category = EItemCategory::Quest;
		C.Add(Seal);

		auto MakeBag = [&C](const char* Id, EBagKind Kind, int32 Capacity, FCategoryMask Mask)
		{
			FItemDefinition Bag;
			Bag.Id = Id;
			Bag.bIsBag = true;
			Bag.Bag.Kind = Kind;
			Bag.Bag.Capacity = Capacity;
			Bag.Bag.AcceptedCategories = Mask;
			C.Add(Bag);
		};
		MakeBag("Bag_Small", EBagKind::General, 9, AllCategories);
		MakeBag("Bag_Adventurer", EBagKind::General, 18, AllCategories);
		MakeBag("Bag_LargeBackpack", EBagKind::General, 27, AllCategories);
		MakeBag("Bag_Materials", EBagKind::Materials, 12, CategoryBit(EItemCategory::Material));
		return C;
	}();
	return Catalog;
}
