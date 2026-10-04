#include "Data/DBQuestDefinition.h"

#include "Core/DBRulesBridge.h"

const FPrimaryAssetType UDBQuestDefinition::AssetType(TEXT("DBQuest"));

FPrimaryAssetId UDBQuestDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, QuestId.IsNone() ? GetFName() : QuestId);
}

DarkBlood::Rules::FQuestDefinition UDBQuestDefinition::ToRules() const
{
	namespace R = DarkBlood::Rules;

	R::FQuestDefinition Out;
	Out.Id = DBBridge::ToStd(QuestId);
	Out.Category = DBBridge::CastEnum<R::EQuestCategory>(Category);
	Out.Scope = DBBridge::CastEnum<R::EQuestScope>(Scope);
	Out.RegionId = DBBridge::ToStd(RegionId);
	for (const FName& Flag : RequiredStoryFlags)
	{
		Out.RequiredStoryFlags.push_back(DBBridge::ToStd(Flag));
	}
	for (const FName& Prerequisite : PrerequisiteQuests)
	{
		Out.PrerequisiteQuests.push_back(DBBridge::ToStd(Prerequisite));
	}
	for (const FDBQuestObjective& Objective : Objectives)
	{
		R::FObjectiveDefinition& O = Out.Objectives.emplace_back();
		O.Id = DBBridge::ToStd(Objective.ObjectiveId);
		O.Kind = DBBridge::CastEnum<R::EObjectiveKind>(Objective.Kind);
		O.Target = DBBridge::ToStd(Objective.Target);
		O.Required = Objective.Required;
		O.bOptional = Objective.bOptional;
	}
	Out.bSequential = bSequential;
	Out.bAutoComplete = bAutoComplete;
	Out.bCanAbandon = bCanAbandon;

	Out.Reward.Xp = Reward.Xp;
	Out.Reward.Currency = Reward.Currency;
	Out.Reward.SkillPoints = Reward.SkillPoints;
	for (const FDBItemGrant& Grant : Reward.Items)
	{
		Out.Reward.Items.push_back(DBBridge::MakeStack(Grant.ItemId, Grant.Count));
	}
	for (const FName& Flag : Reward.StoryFlags)
	{
		Out.Reward.StoryFlags.push_back(DBBridge::ToStd(Flag));
	}
	return Out;
}
