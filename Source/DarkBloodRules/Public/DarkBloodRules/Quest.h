// DARK BLOOD - Rules Core: quests.
// Quest definitions are authored as UE data assets and converted into FQuestDefinition.
// Shared quests (story) live in the world record; personal quests in the character record.
// All progress changes are server-side only.
#pragma once

#include "DarkBloodRules/Items.h"

#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace DarkBlood::Rules
{
	using FStoryFlags = std::set<std::string, std::less<>>;

	enum class EObjectiveKind : uint8
	{
		Kill,     // Target = enemy/boss id or family tag
		Collect,  // Target = item id
		Reach,    // Target = location id
		Talk,     // Target = npc id
		Interact, // Target = interactable id
		Custom,   // Target = scripted event id
	};

	enum class EQuestCategory : uint8
	{
		Main,
		Side,
		Class,
		Dungeon,
		Bounty,
		Regional,
		Hidden,
		Dynamic,
		Settlement,
	};

	enum class EQuestScope : uint8
	{
		Shared,   // progress belongs to the world / group (main story)
		Personal, // progress belongs to one character
	};

	struct FObjectiveDefinition
	{
		std::string Id;
		EObjectiveKind Kind = EObjectiveKind::Custom;
		std::string Target;
		int32 Required = 1;
		bool bOptional = false;
	};

	struct FQuestReward
	{
		int64 Xp = 0;
		int64 Currency = 0;
		int32 SkillPoints = 0;
		/** Deterministic rewards; important story items are never RNG-only. */
		std::vector<FItemStack> Items;
		std::vector<std::string> StoryFlags;
	};

	struct FQuestDefinition
	{
		std::string Id;
		EQuestCategory Category = EQuestCategory::Side;
		EQuestScope Scope = EQuestScope::Personal;
		std::string RegionId;
		std::vector<std::string> RequiredStoryFlags;
		std::vector<std::string> PrerequisiteQuests;
		std::vector<FObjectiveDefinition> Objectives;
		/** When true only the first unfinished objective receives progress. */
		bool bSequential = false;
		/** When false the quest waits in ReadyToTurnIn until handed in at the quest giver. */
		bool bAutoComplete = true;
		/** Main quests cannot be abandoned. */
		bool bCanAbandon = true;
		FQuestReward Reward;
	};

	class FQuestDatabase
	{
	public:
		DARKBLOODRULES_API bool Add(FQuestDefinition Definition);
		DARKBLOODRULES_API const FQuestDefinition* Find(std::string_view QuestId) const;

	private:
		std::unordered_map<std::string, FQuestDefinition> Definitions;
	};

	enum class EQuestStatus : uint8
	{
		Inactive,
		Active,
		ReadyToTurnIn,
		Completed,
		Failed,
	};

	struct FQuestProgress
	{
		std::string QuestId;
		EQuestStatus Status = EQuestStatus::Inactive;
		std::vector<int32> ObjectiveCounts;
	};

	struct FQuestEvent
	{
		EObjectiveKind Kind = EObjectiveKind::Custom;
		std::string Target;
		int32 Amount = 1;
	};

	struct FQuestUpdate
	{
		std::string QuestId;
		int32 ObjectiveIndex = -1;
		int32 NewCount = 0;
		EQuestStatus NewStatus = EQuestStatus::Active;
	};

	enum class EQuestResult : uint8
	{
		Ok,
		UnknownQuest,
		AlreadyActive,
		AlreadyCompleted,
		MissingStoryFlag,
		MissingPrerequisite,
		NotActive,
		NotReady,
		CannotAbandon,
	};

	DARKBLOODRULES_API const char* ToString(EQuestResult Result);

	class FQuestLog
	{
	public:
		DARKBLOODRULES_API EQuestResult Start(const FQuestDatabase& Database, std::string_view QuestId, const FStoryFlags& Flags);

		/** Routes a gameplay event to all active quests. Returned updates with NewStatus == Completed must be rewarded by the caller. */
		DARKBLOODRULES_API std::vector<FQuestUpdate> ApplyEvent(const FQuestDatabase& Database, const FQuestEvent& Event);

		/** Completes a quest waiting in ReadyToTurnIn. The caller grants Definition.Reward on Ok. */
		DARKBLOODRULES_API EQuestResult TurnIn(const FQuestDatabase& Database, std::string_view QuestId);

		DARKBLOODRULES_API EQuestResult Fail(std::string_view QuestId);
		DARKBLOODRULES_API EQuestResult Abandon(const FQuestDatabase& Database, std::string_view QuestId);

		DARKBLOODRULES_API const FQuestProgress* Find(std::string_view QuestId) const;
		DARKBLOODRULES_API bool IsCompleted(std::string_view QuestId) const;
		const std::vector<FQuestProgress>& GetAll() const { return Entries; }

		/** Raw restore used by deserialization. Call Reconcile() once quest definitions are available. */
		void RestoreRaw(std::vector<FQuestProgress> InEntries) { Entries = std::move(InEntries); }

		/** Adapts stored progress to the current definitions (content patches may add/remove objectives). */
		DARKBLOODRULES_API void Reconcile(const FQuestDatabase& Database);

	private:
		FQuestProgress* FindMutable(std::string_view QuestId);
		static bool AreRequiredObjectivesDone(const FQuestDefinition& Definition, const FQuestProgress& Progress);

		std::vector<FQuestProgress> Entries;
	};
}
