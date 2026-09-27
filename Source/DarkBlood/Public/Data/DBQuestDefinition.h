// Data asset for quests. Quest flow logic lives in the rules core; presentation (dialogue,
// markers, cinematics) is attached by content through the ids referenced here.
#pragma once

#include "Engine/DataAsset.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/Quest.h"

#include "DBQuestDefinition.generated.h"

USTRUCT(BlueprintType)
struct FDBQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	EDBObjectiveKind Kind = EDBObjectiveKind::Custom;

	/** Enemy id, item id, location id, NPC id, interactable id or scripted event id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName Target;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (ClampMin = 1))
	int32 Required = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bOptional = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText Description;
};

USTRUCT(BlueprintType)
struct FDBQuestReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	int64 Xp = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	int64 Currency = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	int32 SkillPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<FDBItemGrant> Items;

	/** World story flags set on completion (e.g. "Story.IntroDone"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<FName> StoryFlags;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	DarkBlood::Rules::FQuestDefinition ToRules() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	EDBQuestCategory Category = EDBQuestCategory::Side;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	EDBQuestScope Scope = EDBQuestScope::Personal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FName RegionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirements")
	TArray<FName> RequiredStoryFlags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirements")
	TArray<FName> PrerequisiteQuests;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objectives")
	TArray<FDBQuestObjective> Objectives;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objectives")
	bool bSequential = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Objectives")
	bool bAutoComplete = true;

	/** Ignored for main quests, which can never be abandoned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	bool bCanAbandon = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward")
	FDBQuestReward Reward;
};
