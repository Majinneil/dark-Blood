#include "Data/DBItemDefinition.h"

#include "Core/DBRulesBridge.h"

const FPrimaryAssetType UDBItemDefinition::AssetType(TEXT("DBItem"));

FPrimaryAssetId UDBItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, ItemId.IsNone() ? GetFName() : ItemId);
}

DarkBlood::Rules::FItemDefinition UDBItemDefinition::ToRules() const
{
	namespace R = DarkBlood::Rules;

	R::FItemDefinition Out;
	Out.Id = DBBridge::ToStd(ItemId);
	Out.Category = DBBridge::CastEnum<R::EItemCategory>(Category);
	Out.Rarity = DBBridge::CastEnum<R::EItemRarity>(Rarity);
	Out.MaxStack = FMath::Max(1, MaxStack);
	Out.ItemLevel = ItemLevel;
	Out.RequiredLevel = RequiredLevel;
	Out.MaxDurability = MaxDurability;
	Out.EquipSlot = DBBridge::CastEnum<R::EEquipSlot>(EquipSlot);
	for (const FName& ClassId : AllowedClasses)
	{
		Out.AllowedClasses.push_back(DBBridge::ToStd(ClassId));
	}
	Out.bIsBag = bIsBag;
	Out.Bag.Kind = DBBridge::CastEnum<R::EBagKind>(BagKind);
	Out.Bag.Capacity = BagCapacity;
	Out.Bag.AcceptedCategories = BagKind == EDBBagKind::General ? R::AllCategories
		: static_cast<R::FCategoryMask>(BagAcceptedCategories) & R::AllCategories;
	return Out;
}
