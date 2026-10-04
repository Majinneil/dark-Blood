// DARK BLOOD - Rules Core: skill tree unlock rules.
// Node effects (abilities, mechanic changes) live in UE data assets; this file only
// guards the server-side unlock rules: points, ranks, level and prerequisites.
#pragma once

#include "DarkBloodRules/Progression.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace DarkBlood::Rules
{
	struct FSkillNodeDefinition
	{
		std::string Id;
		int32 CostPerRank = 1;
		int32 MaxRank = 1;
		int32 RequiredLevel = 1;
		/** All listed nodes must have at least rank 1. */
		std::vector<std::string> Prerequisites;
	};

	struct FSkillTreeDefinition
	{
		std::string Id;
		std::vector<FSkillNodeDefinition> Nodes;

		DARKBLOODRULES_API const FSkillNodeDefinition* Find(std::string_view NodeId) const;
	};

	struct FSkillTreeState
	{
		/** Node id -> current rank. Ordered for deterministic serialization. */
		std::map<std::string, int32, std::less<>> Ranks;

		DARKBLOODRULES_API int32 GetRank(std::string_view NodeId) const;
	};

	enum class ESkillUnlockResult : uint8
	{
		Ok,
		UnknownNode,
		MaxRankReached,
		NotEnoughPoints,
		LevelTooLow,
		MissingPrerequisite,
	};

	DARKBLOODRULES_API const char* ToString(ESkillUnlockResult Result);

	DARKBLOODRULES_API ESkillUnlockResult CanUnlockSkill(const FSkillTreeDefinition& Tree, const FSkillTreeState& State,
		const FProgressionState& Progression, std::string_view NodeId);

	/** Unlocks one rank, spending skill points. */
	DARKBLOODRULES_API ESkillUnlockResult UnlockSkill(const FSkillTreeDefinition& Tree, FSkillTreeState& State,
		FProgressionState& Progression, std::string_view NodeId);

	/** Total points invested; used for respec and validation. */
	DARKBLOODRULES_API int32 CountSpentPoints(const FSkillTreeDefinition& Tree, const FSkillTreeState& State);
}
