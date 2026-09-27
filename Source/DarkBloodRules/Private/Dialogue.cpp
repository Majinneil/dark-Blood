#include "DarkBloodRules/Dialogue.h"

#include <unordered_set>

namespace DarkBlood::Rules
{
	EQuestStatus FDialogueContext::GetQuestStatus(std::string_view QuestId) const
	{
		const auto Found = QuestStatuses.find(std::string(QuestId));
		return Found != QuestStatuses.end() ? Found->second : EQuestStatus::Inactive;
	}

	const char* ToString(EDialogueValidation Result)
	{
		switch (Result)
		{
		case EDialogueValidation::Ok: return "Ok";
		case EDialogueValidation::NoEntries: return "NoEntries";
		case EDialogueValidation::DuplicateNodeId: return "DuplicateNodeId";
		case EDialogueValidation::UnknownEntryNode: return "UnknownEntryNode";
		case EDialogueValidation::UnknownNextNode: return "UnknownNextNode";
		case EDialogueValidation::EmptyEffectId: return "EmptyEffectId";
		}
		return "Unknown";
	}

	const FDialogueNode* FindDialogueNode(const FDialogueDefinition& Dialogue, std::string_view NodeId)
	{
		for (const FDialogueNode& Node : Dialogue.Nodes)
		{
			if (Node.Id == NodeId)
			{
				return &Node;
			}
		}
		return nullptr;
	}

	EDialogueValidation ValidateDialogue(const FDialogueDefinition& Dialogue)
	{
		if (Dialogue.Entries.empty())
		{
			return EDialogueValidation::NoEntries;
		}
		std::unordered_set<std::string> Ids;
		for (const FDialogueNode& Node : Dialogue.Nodes)
		{
			if (!Ids.insert(Node.Id).second)
			{
				return EDialogueValidation::DuplicateNodeId;
			}
		}
		for (const FDialogueEntry& Entry : Dialogue.Entries)
		{
			if (!Ids.count(Entry.NodeId))
			{
				return EDialogueValidation::UnknownEntryNode;
			}
		}
		auto EffectsValid = [](const std::vector<FDialogueEffect>& Effects)
		{
			for (const FDialogueEffect& Effect : Effects)
			{
				if (Effect.Id.empty())
				{
					return false;
				}
			}
			return true;
		};
		for (const FDialogueNode& Node : Dialogue.Nodes)
		{
			if (!EffectsValid(Node.OnEnter))
			{
				return EDialogueValidation::EmptyEffectId;
			}
			for (const FDialogueChoice& Choice : Node.Choices)
			{
				if (!Choice.NextNodeId.empty() && !Ids.count(Choice.NextNodeId))
				{
					return EDialogueValidation::UnknownNextNode;
				}
				if (!EffectsValid(Choice.Effects))
				{
					return EDialogueValidation::EmptyEffectId;
				}
			}
		}
		return EDialogueValidation::Ok;
	}

	bool AreDialogueConditionsMet(const std::vector<FDialogueCondition>& Conditions, const FDialogueContext& Context)
	{
		for (const FDialogueCondition& Condition : Conditions)
		{
			bool bMet = false;
			switch (Condition.Kind)
			{
			case EDialogueConditionKind::HasStoryFlag: bMet = Context.StoryFlags.count(Condition.Id) > 0; break;
			case EDialogueConditionKind::LacksStoryFlag: bMet = Context.StoryFlags.count(Condition.Id) == 0; break;
			case EDialogueConditionKind::QuestStatusIs: bMet = Context.GetQuestStatus(Condition.Id) == Condition.Status; break;
			case EDialogueConditionKind::QuestStatusIsNot: bMet = Context.GetQuestStatus(Condition.Id) != Condition.Status; break;
			}
			if (!bMet)
			{
				return false;
			}
		}
		return true;
	}

	namespace
	{
		FDialogueStep EnterNode(const FDialogueNode& Node, FDialogueStep Step, const FDialogueContext& Context)
		{
			Step.bValid = true;
			Step.NodeId = Node.Id;
			Step.Effects.insert(Step.Effects.end(), Node.OnEnter.begin(), Node.OnEnter.end());
			// A node without offered choices is the last line of the conversation.
			Step.bEnded = GetAvailableChoices(Node, Context).empty();
			return Step;
		}
	}

	FDialogueStep BeginDialogue(const FDialogueDefinition& Dialogue, const FDialogueContext& Context)
	{
		for (const FDialogueEntry& Entry : Dialogue.Entries)
		{
			if (AreDialogueConditionsMet(Entry.Conditions, Context))
			{
				if (const FDialogueNode* Node = FindDialogueNode(Dialogue, Entry.NodeId))
				{
					return EnterNode(*Node, FDialogueStep(), Context);
				}
			}
		}
		return FDialogueStep();
	}

	std::vector<int32> GetAvailableChoices(const FDialogueNode& Node, const FDialogueContext& Context)
	{
		std::vector<int32> Result;
		for (size_t Index = 0; Index < Node.Choices.size(); ++Index)
		{
			if (AreDialogueConditionsMet(Node.Choices[Index].Conditions, Context))
			{
				Result.push_back(static_cast<int32>(Index));
			}
		}
		return Result;
	}

	FDialogueStep ChooseDialogueOption(const FDialogueDefinition& Dialogue, std::string_view CurrentNodeId, int32 ChoiceIndex,
		const FDialogueContext& Context)
	{
		const FDialogueNode* Node = FindDialogueNode(Dialogue, CurrentNodeId);
		if (!Node || ChoiceIndex < 0 || ChoiceIndex >= static_cast<int32>(Node->Choices.size()))
		{
			return FDialogueStep();
		}
		const FDialogueChoice& Choice = Node->Choices[static_cast<size_t>(ChoiceIndex)];
		if (!AreDialogueConditionsMet(Choice.Conditions, Context))
		{
			return FDialogueStep(); // not offered: a client cannot pick hidden options
		}

		FDialogueStep Step;
		Step.Effects = Choice.Effects;
		if (Choice.NextNodeId.empty())
		{
			Step.bValid = true;
			Step.bEnded = true;
			return Step;
		}
		const FDialogueNode* Next = FindDialogueNode(Dialogue, Choice.NextNodeId);
		if (!Next)
		{
			return FDialogueStep();
		}
		return EnterNode(*Next, std::move(Step), Context);
	}
}
