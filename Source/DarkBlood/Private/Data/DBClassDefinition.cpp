#include "Data/DBClassDefinition.h"

#include "Core/DBRulesBridge.h"

const FPrimaryAssetType UDBClassDefinition::AssetType(TEXT("DBClass"));

DarkBlood::Rules::FPrimaryStats FDBStatBlock::ToRules() const
{
	DarkBlood::Rules::FPrimaryStats Out;
	Out.Strength = Strength;
	Out.Dexterity = Dexterity;
	Out.Intelligence = Intelligence;
	Out.Spirit = Spirit;
	Out.Vitality = Vitality;
	Out.Endurance = Endurance;
	return Out;
}

FPrimaryAssetId UDBClassDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, ClassId.IsNone() ? GetFName() : ClassId);
}

DarkBlood::Rules::FClassGrowth UDBClassDefinition::GetGrowth() const
{
	DarkBlood::Rules::FClassGrowth Growth;
	Growth.BaseAtLevel1 = BaseStats.ToRules();
	Growth.PerLevel = StatsPerLevel.ToRules();
	return Growth;
}

DarkBlood::Rules::FSkillTreeDefinition UDBClassDefinition::BuildSkillTree() const
{
	DarkBlood::Rules::FSkillTreeDefinition Tree;
	Tree.Id = DBBridge::ToStd(ClassId);
	for (const FDBSkillNode& Node : SkillTree)
	{
		DarkBlood::Rules::FSkillNodeDefinition& Out = Tree.Nodes.emplace_back();
		Out.Id = DBBridge::ToStd(Node.NodeId);
		Out.CostPerRank = Node.CostPerRank;
		Out.MaxRank = Node.MaxRank;
		Out.RequiredLevel = Node.RequiredLevel;
		for (const FName& Prerequisite : Node.Prerequisites)
		{
			Out.Prerequisites.push_back(DBBridge::ToStd(Prerequisite));
		}
	}
	return Tree;
}

const FDBSkillNode* UDBClassDefinition::FindSkillNode(FName NodeId) const
{
	return SkillTree.FindByPredicate([NodeId](const FDBSkillNode& Node) { return Node.NodeId == NodeId; });
}
