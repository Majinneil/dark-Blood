// Data assets for the item economy: crafting recipes (DBRecipe) and loot tables (DBLootTable).
// Rules live in DarkBlood::Rules (Crafting.h); these assets only hold authored data.
#pragma once

#include "Core/DBTypes.h"
#include "Engine/DataAsset.h"

#include "DarkBloodRules/Crafting.h"

#include "DBEconomyDefinitions.generated.h"

USTRUCT(BlueprintType)
struct FDBRecipeIngredient
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 1))
	int32 Count = 1;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBRecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	DarkBlood::Rules::FRecipe ToRules() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FName RecipeId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FName OutputItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 1))
	int32 OutputCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (TitleProperty = "ItemId"))
	TArray<FDBRecipeIngredient> Inputs;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 0))
	int64 CurrencyCost = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 1))
	int32 RequiredLevel = 1;

	/** Station id (e.g. "Forge"); None = craftable anywhere. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FName StationId;
};

USTRUCT(BlueprintType)
struct FDBLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 1))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 1))
	int32 MaxCount = 1;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBLootTableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	DarkBlood::Rules::FLootTable ToRules() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	FName LootTableId;

	/** Always dropped (boss and story rewards are never RNG-only). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (TitleProperty = "ItemId"))
	TArray<FDBItemGrant> Guaranteed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (TitleProperty = "ItemId"))
	TArray<FDBLootEntry> Entries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	int32 Rolls = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	int32 NothingWeight = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	int64 CurrencyMin = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0))
	int64 CurrencyMax = 0;
};
