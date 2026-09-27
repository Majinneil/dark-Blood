#include "Data/DBEconomyDefinitions.h"

#include "Core/DBRulesBridge.h"

namespace R = DarkBlood::Rules;

const FPrimaryAssetType UDBRecipeDefinition::AssetType(TEXT("DBRecipe"));
const FPrimaryAssetType UDBLootTableDefinition::AssetType(TEXT("DBLootTable"));

FPrimaryAssetId UDBRecipeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, RecipeId.IsNone() ? GetFName() : RecipeId);
}

R::FRecipe UDBRecipeDefinition::ToRules() const
{
	R::FRecipe Out;
	Out.Id = DBBridge::ToStd(RecipeId);
	Out.OutputItemId = DBBridge::ToStd(OutputItemId);
	Out.OutputCount = OutputCount;
	for (const FDBRecipeIngredient& Input : Inputs)
	{
		Out.Inputs.push_back({DBBridge::ToStd(Input.ItemId), Input.Count});
	}
	Out.CurrencyCost = CurrencyCost;
	Out.RequiredLevel = RequiredLevel;
	Out.StationId = StationId.IsNone() ? std::string() : DBBridge::ToStd(StationId);
	return Out;
}

FPrimaryAssetId UDBLootTableDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, LootTableId.IsNone() ? GetFName() : LootTableId);
}

R::FLootTable UDBLootTableDefinition::ToRules() const
{
	R::FLootTable Out;
	Out.Id = DBBridge::ToStd(LootTableId);
	for (const FDBItemGrant& Grant : Guaranteed)
	{
		Out.Guaranteed.push_back(DBBridge::MakeStack(Grant.ItemId, Grant.Count));
	}
	for (const FDBLootEntry& Entry : Entries)
	{
		Out.Entries.push_back({DBBridge::ToStd(Entry.ItemId), Entry.Weight, Entry.MinCount, Entry.MaxCount});
	}
	Out.Rolls = Rolls;
	Out.NothingWeight = NothingWeight;
	Out.CurrencyMin = CurrencyMin;
	Out.CurrencyMax = CurrencyMax;
	return Out;
}
