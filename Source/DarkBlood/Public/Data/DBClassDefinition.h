// Data asset describing a playable class (Krieger, Schattenlaeufer, Magier, Moench, ... - extendable).
#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/SkillTree.h"
#include "DarkBloodRules/Stats.h"

#include "DBClassDefinition.generated.h"

class UDBAbilitySet;
class UDBGameplayAbility;

USTRUCT(BlueprintType)
struct FDBStatBlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float Strength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float Dexterity = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float Intelligence = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float Spirit = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float Vitality = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats") float Endurance = 0.f;

	DarkBlood::Rules::FPrimaryStats ToRules() const;
};

USTRUCT(BlueprintType)
struct FDBSkillNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText DisplayName;

	/** Describe the mechanic the node changes - no "+2 % damage" filler nodes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = 1))
	int32 CostPerRank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = 1))
	int32 MaxRank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = 1))
	int32 RequiredLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TArray<FName> Prerequisites;

	/** Ability granted when the first rank is unlocked (optional; upgrades are read by the ability itself). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UDBGameplayAbility> GrantedAbility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FGameplayTag InputTag;

	/** Position in the skill tree UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|UI")
	FVector2D UIPosition = FVector2D::ZeroVector;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBClassDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	DarkBlood::Rules::FClassGrowth GetGrowth() const;
	DarkBlood::Rules::FSkillTreeDefinition BuildSkillTree() const;
	const FDBSkillNode* FindSkillNode(FName NodeId) const;

	/** Stable id stored in save data and item class restrictions (e.g. "Warrior"). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class")
	FName ClassId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class")
	FGameplayTag ClassTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	FDBStatBlock BaseStats;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	FDBStatBlock StatsPerLevel;

	/** Abilities and effects every member of the class starts with. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UDBAbilitySet> BaseAbilitySet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TArray<FDBSkillNode> SkillTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Start")
	TArray<FDBItemGrant> StartingItems;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Start")
	int64 StartingCurrency = 50;
};
