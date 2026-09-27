// DARK BLOOD - Rules Core: NPC dialogue.
// Dialogues are graphs of nodes with choices. Which entry node an NPC opens with, which choices are
// offered and what a choice does (story flags, quests) is decided here, on the server. Clients only
// send the index of the choice they picked; anything not currently offered is refused.
#pragma once

#include "DarkBloodRules/Quest.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace DarkBlood::Rules
{
	enum class EDialogueConditionKind : uint8
	{
		HasStoryFlag,
		LacksStoryFlag,
		/** Quest has exactly the given status (Inactive = never started). */
		QuestStatusIs,
		QuestStatusIsNot,
	};

	struct FDialogueCondition
	{
		EDialogueConditionKind Kind = EDialogueConditionKind::HasStoryFlag;
		std::string Id;
		EQuestStatus Status = EQuestStatus::Inactive;
	};

	enum class EDialogueEffectKind : uint8
	{
		SetStoryFlag,
		StartQuest,
		TurnInQuest,
		/** Reports a Talk quest event with the given NPC id. */
		ReportTalk,
	};

	struct FDialogueEffect
	{
		EDialogueEffectKind Kind = EDialogueEffectKind::SetStoryFlag;
		std::string Id;
	};

	struct FDialogueChoice
	{
		std::string Text;
		/** Empty = the choice ends the conversation. */
		std::string NextNodeId;
		std::vector<FDialogueCondition> Conditions;
		std::vector<FDialogueEffect> Effects;
	};

	struct FDialogueNode
	{
		std::string Id;
		std::string Speaker;
		std::string Text;
		std::vector<FDialogueChoice> Choices;
		/** Applied when the node is entered (including entry nodes). */
		std::vector<FDialogueEffect> OnEnter;
	};

	/** The first entry whose conditions pass decides where the conversation starts. */
	struct FDialogueEntry
	{
		std::string NodeId;
		std::vector<FDialogueCondition> Conditions;
	};

	struct FDialogueDefinition
	{
		std::string Id;
		std::vector<FDialogueEntry> Entries;
		std::vector<FDialogueNode> Nodes;
	};

	/** World/player facts a dialogue can look at. */
	struct FDialogueContext
	{
		FStoryFlags StoryFlags;
		/** Missing quests count as Inactive. */
		std::unordered_map<std::string, EQuestStatus> QuestStatuses;

		EQuestStatus GetQuestStatus(std::string_view QuestId) const;
	};

	struct FDialogueStep
	{
		bool bValid = false;
		/** True when the conversation is over after this step. */
		bool bEnded = false;
		std::string NodeId;
		/** Effects to apply, in order (choice effects, then the next node's OnEnter). */
		std::vector<FDialogueEffect> Effects;
	};

	enum class EDialogueValidation : uint8
	{
		Ok,
		NoEntries,
		DuplicateNodeId,
		UnknownEntryNode,
		UnknownNextNode,
		EmptyEffectId,
	};

	DARKBLOODRULES_API const char* ToString(EDialogueValidation Result);

	DARKBLOODRULES_API EDialogueValidation ValidateDialogue(const FDialogueDefinition& Dialogue);
	DARKBLOODRULES_API const FDialogueNode* FindDialogueNode(const FDialogueDefinition& Dialogue, std::string_view NodeId);
	DARKBLOODRULES_API bool AreDialogueConditionsMet(const std::vector<FDialogueCondition>& Conditions, const FDialogueContext& Context);

	/** Opens the conversation: entry node and its OnEnter effects (invalid if no entry applies). */
	DARKBLOODRULES_API FDialogueStep BeginDialogue(const FDialogueDefinition& Dialogue, const FDialogueContext& Context);

	/** Indices of the node's choices that are currently offered. */
	DARKBLOODRULES_API std::vector<int32> GetAvailableChoices(const FDialogueNode& Node, const FDialogueContext& Context);

	/**
	 * Picks choice ChoiceIndex (index into Node.Choices) of the current node. Refuses (bValid = false)
	 * choices that do not exist or are not offered in this context.
	 */
	DARKBLOODRULES_API FDialogueStep ChooseDialogueOption(const FDialogueDefinition& Dialogue, std::string_view CurrentNodeId,
		int32 ChoiceIndex, const FDialogueContext& Context);
}
