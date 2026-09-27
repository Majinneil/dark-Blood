// Data asset for any item. Gameplay data and visual references are kept apart so
// development placeholders can be swapped for final assets without touching gameplay.
#pragma once

#include "Engine/DataAsset.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/Items.h"

#include "DBItemDefinition.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UTexture2D;

/** Equipment stat bonuses (see DarkBlood::Rules::FItemStats). Resistances are fractions (0.1 = 10 %). */
USTRUCT(BlueprintType)
struct FDBItemStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float AttackPower = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float SpellPower = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float Armor = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float MaxHealth = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float MaxStamina = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float MaxMana = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float CritChance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistances") float FireResistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistances") float FrostResistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistances") float LightningResistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistances") float ShadowResistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistances") float PoisonResistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistances") float SpiritResistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistances") float DarkBloodResistance = 0.f;

	DarkBlood::Rules::FItemStats ToRules() const;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	DarkBlood::Rules::FItemDefinition ToRules() const;

	// ---- Gameplay -------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EDBItemCategory Category = EDBItemCategory::Misc;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EDBItemRarity Rarity = EDBItemRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 MaxStack = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 0))
	int32 ItemLevel = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 RequiredLevel = 1;

	/** 0 = item has no durability. Items with durability are unique instances. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 0))
	int32 MaxDurability = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 BaseValue = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EDBEquipSlot EquipSlot = EDBEquipSlot::None;

	/** Class ids that may equip this item; empty = all classes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TArray<FName> AllowedClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FDBItemStats Stats;

	/** Consumables: restored when used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable") float HealAmount = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable") float StaminaAmount = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable") float ManaAmount = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bag")
	bool bIsBag = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bag", meta = (EditCondition = "bIsBag"))
	EDBBagKind BagKind = EDBBagKind::General;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bag", meta = (EditCondition = "bIsBag", ClampMin = 0))
	int32 BagCapacity = 9;

	/** Categories a special bag accepts (ignored for general bags, which accept everything). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bag",
		meta = (EditCondition = "bIsBag", Bitmask, BitmaskEnum = "/Script/DarkBlood.EDBItemCategory"))
	int32 BagAcceptedCategories = 0;

	// ---- Visuals (soft references, loaded on demand) --------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<USkeletalMesh> EquippedMesh;

	/** True while the visuals are development placeholders (shown in the debug UI). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	bool bUsesPlaceholderVisuals = true;
};
