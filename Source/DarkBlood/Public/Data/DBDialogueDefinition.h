// Data asset for an NPC conversation (graph of nodes and choices). Logic lives in DarkBlood::Rules (Dialogue.h);
// this asset holds the authored text and converts ids/conditions/effects for the rules core.
#pragma once

#include "Core/DBTypes.h"
#include "Engine/DataAsset.h"

#include "DarkBloodRules/Dialogue.h"

#include "DBDialogueDefinition.generated.h"

UENUM(BlueprintType)
enum class EDBDialogueCondition : uint8
{
	HasStoryFlag,
	LacksStoryFlag,
	QuestStatusIs,
	QuestStatusIsNot,
};

UENUM(BlueprintType)
enum class EDBDialogueEffect : uint8
{
	SetStoryFlag,
	StartQuest,
	TurnInQuest,
	ReportTalk,
};

USTRUCT(BlueprintType)
struct FDBDialogueCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	EDBDialogueCondition Kind = EDBDialogueCondition::HasStoryFlag;

	/** Story flag or quest id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	EDBQuestStatus Status = EDBQuestStatus::Inactive;
};

USTRUCT(BlueprintType)
struct FDBDialogueEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	EDBDialogueEffect Kind = EDBDialogueEffect::SetStoryFlag;

	/** Story flag, quest id or NPC id (ReportTalk). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName Id;
};

USTRUCT(BlueprintType)
struct FDBDialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText Text;

	/** None = the choice ends the conversation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NextNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDBDialogueCondition> Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDBDialogueEffect> Effects;
};

USTRUCT(BlueprintType)
struct FDBDialogueNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (MultiLine = true))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (TitleProperty = "Text"))
	TArray<FDBDialogueChoice> Choices;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDBDialogueEffect> OnEnter;
};

USTRUCT(BlueprintType)
struct FDBDialogueEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FDBDialogueCondition> Conditions;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBDialogueDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	DarkBlood::Rules::FDialogueDefinition ToRules() const;
	const FDBDialogueNode* FindNode(FName NodeId) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue")
	FName DialogueId;

	/** Checked top to bottom; the first entry whose conditions pass opens the conversation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue", meta = (TitleProperty = "NodeId"))
	TArray<FDBDialogueEntry> Entries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue", meta = (TitleProperty = "NodeId"))
	TArray<FDBDialogueNode> Nodes;
};
