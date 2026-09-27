#include "Data/DBDialogueDefinition.h"

#include "Core/DBRulesBridge.h"

namespace R = DarkBlood::Rules;

static_assert(static_cast<uint8>(EDBDialogueCondition::QuestStatusIsNot) == static_cast<uint8>(R::EDialogueConditionKind::QuestStatusIsNot));
static_assert(static_cast<uint8>(EDBDialogueEffect::ReportTalk) == static_cast<uint8>(R::EDialogueEffectKind::ReportTalk));

const FPrimaryAssetType UDBDialogueDefinition::AssetType(TEXT("DBDialogue"));

FPrimaryAssetId UDBDialogueDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, DialogueId.IsNone() ? GetFName() : DialogueId);
}

namespace
{
	std::string IdOrEmpty(FName Name)
	{
		return Name.IsNone() ? std::string() : DBBridge::ToStd(Name);
	}

	std::vector<R::FDialogueCondition> ConvertConditions(const TArray<FDBDialogueCondition>& Conditions)
	{
		std::vector<R::FDialogueCondition> Out;
		for (const FDBDialogueCondition& Condition : Conditions)
		{
			R::FDialogueCondition& Converted = Out.emplace_back();
			Converted.Kind = DBBridge::CastEnum<R::EDialogueConditionKind>(Condition.Kind);
			Converted.Id = IdOrEmpty(Condition.Id);
			Converted.Status = DBBridge::CastEnum<R::EQuestStatus>(Condition.Status);
		}
		return Out;
	}

	std::vector<R::FDialogueEffect> ConvertEffects(const TArray<FDBDialogueEffect>& Effects)
	{
		std::vector<R::FDialogueEffect> Out;
		for (const FDBDialogueEffect& Effect : Effects)
		{
			R::FDialogueEffect& Converted = Out.emplace_back();
			Converted.Kind = DBBridge::CastEnum<R::EDialogueEffectKind>(Effect.Kind);
			Converted.Id = IdOrEmpty(Effect.Id);
		}
		return Out;
	}
}

R::FDialogueDefinition UDBDialogueDefinition::ToRules() const
{
	R::FDialogueDefinition Out;
	Out.Id = IdOrEmpty(DialogueId);
	for (const FDBDialogueEntry& Entry : Entries)
	{
		Out.Entries.push_back({IdOrEmpty(Entry.NodeId), ConvertConditions(Entry.Conditions)});
	}
	for (const FDBDialogueNode& Node : Nodes)
	{
		R::FDialogueNode& Converted = Out.Nodes.emplace_back();
		Converted.Id = IdOrEmpty(Node.NodeId);
		Converted.OnEnter = ConvertEffects(Node.OnEnter);
		// Text stays on the UE side; the rules only need the graph.
		for (const FDBDialogueChoice& Choice : Node.Choices)
		{
			R::FDialogueChoice& ConvertedChoice = Converted.Choices.emplace_back();
			ConvertedChoice.NextNodeId = IdOrEmpty(Choice.NextNodeId);
			ConvertedChoice.Conditions = ConvertConditions(Choice.Conditions);
			ConvertedChoice.Effects = ConvertEffects(Choice.Effects);
		}
	}
	return Out;
}

const FDBDialogueNode* UDBDialogueDefinition::FindNode(FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const FDBDialogueNode& Node) { return Node.NodeId == NodeId; });
}
