#include "DarkBloodRules/SkillTree.h"

namespace DarkBlood::Rules
{
	const FSkillNodeDefinition* FSkillTreeDefinition::Find(std::string_view NodeId) const
	{
		for (const FSkillNodeDefinition& Node : Nodes)
		{
			if (Node.Id == NodeId)
			{
				return &Node;
			}
		}
		return nullptr;
	}

	int32 FSkillTreeState::GetRank(std::string_view NodeId) const
	{
		const auto It = Ranks.find(NodeId);
		return It != Ranks.end() ? It->second : 0;
	}

	const char* ToString(ESkillUnlockResult Result)
	{
		switch (Result)
		{
		case ESkillUnlockResult::Ok: return "Ok";
		case ESkillUnlockResult::UnknownNode: return "UnknownNode";
		case ESkillUnlockResult::MaxRankReached: return "MaxRankReached";
		case ESkillUnlockResult::NotEnoughPoints: return "NotEnoughPoints";
		case ESkillUnlockResult::LevelTooLow: return "LevelTooLow";
		case ESkillUnlockResult::MissingPrerequisite: return "MissingPrerequisite";
		}
		return "Unknown";
	}

	ESkillUnlockResult CanUnlockSkill(const FSkillTreeDefinition& Tree, const FSkillTreeState& State,
		const FProgressionState& Progression, std::string_view NodeId)
	{
		const FSkillNodeDefinition* Node = Tree.Find(NodeId);
		if (!Node)
		{
			return ESkillUnlockResult::UnknownNode;
		}
		if (State.GetRank(NodeId) >= Node->MaxRank)
		{
			return ESkillUnlockResult::MaxRankReached;
		}
		if (Progression.Level < Node->RequiredLevel)
		{
			return ESkillUnlockResult::LevelTooLow;
		}
		for (const std::string& Prerequisite : Node->Prerequisites)
		{
			if (State.GetRank(Prerequisite) < 1)
			{
				return ESkillUnlockResult::MissingPrerequisite;
			}
		}
		if (Progression.UnspentSkillPoints < Node->CostPerRank)
		{
			return ESkillUnlockResult::NotEnoughPoints;
		}
		return ESkillUnlockResult::Ok;
	}

	ESkillUnlockResult UnlockSkill(const FSkillTreeDefinition& Tree, FSkillTreeState& State,
		FProgressionState& Progression, std::string_view NodeId)
	{
		const ESkillUnlockResult Result = CanUnlockSkill(Tree, State, Progression, NodeId);
		if (Result != ESkillUnlockResult::Ok)
		{
			return Result;
		}
		const FSkillNodeDefinition* Node = Tree.Find(NodeId);
		SpendSkillPoints(Progression, Node->CostPerRank);
		++State.Ranks[Node->Id];
		return ESkillUnlockResult::Ok;
	}

	int32 CountSpentPoints(const FSkillTreeDefinition& Tree, const FSkillTreeState& State)
	{
		int32 Spent = 0;
		for (const auto& [NodeId, Rank] : State.Ranks)
		{
			if (const FSkillNodeDefinition* Node = Tree.Find(NodeId))
			{
				Spent += Node->CostPerRank * Rank;
			}
		}
		return Spent;
	}
}
