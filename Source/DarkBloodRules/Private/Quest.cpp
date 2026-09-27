#include "DarkBloodRules/Quest.h"

#include <algorithm>

namespace DarkBlood::Rules
{
	bool FQuestDatabase::Add(FQuestDefinition Definition)
	{
		if (Definition.Id.empty())
		{
			return false;
		}
		for (FObjectiveDefinition& Objective : Definition.Objectives)
		{
			Objective.Required = std::max(1, Objective.Required);
		}
		if (Definition.Category == EQuestCategory::Main)
		{
			Definition.bCanAbandon = false;
		}
		std::string Key = Definition.Id;
		return Definitions.emplace(std::move(Key), std::move(Definition)).second;
	}

	const FQuestDefinition* FQuestDatabase::Find(std::string_view QuestId) const
	{
		const auto It = Definitions.find(std::string(QuestId));
		return It != Definitions.end() ? &It->second : nullptr;
	}

	const char* ToString(EQuestResult Result)
	{
		switch (Result)
		{
		case EQuestResult::Ok: return "Ok";
		case EQuestResult::UnknownQuest: return "UnknownQuest";
		case EQuestResult::AlreadyActive: return "AlreadyActive";
		case EQuestResult::AlreadyCompleted: return "AlreadyCompleted";
		case EQuestResult::MissingStoryFlag: return "MissingStoryFlag";
		case EQuestResult::MissingPrerequisite: return "MissingPrerequisite";
		case EQuestResult::NotActive: return "NotActive";
		case EQuestResult::NotReady: return "NotReady";
		case EQuestResult::CannotAbandon: return "CannotAbandon";
		}
		return "Unknown";
	}

	FQuestProgress* FQuestLog::FindMutable(std::string_view QuestId)
	{
		for (FQuestProgress& Entry : Entries)
		{
			if (Entry.QuestId == QuestId)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	const FQuestProgress* FQuestLog::Find(std::string_view QuestId) const
	{
		for (const FQuestProgress& Entry : Entries)
		{
			if (Entry.QuestId == QuestId)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	bool FQuestLog::IsCompleted(std::string_view QuestId) const
	{
		const FQuestProgress* Entry = Find(QuestId);
		return Entry && Entry->Status == EQuestStatus::Completed;
	}

	bool FQuestLog::AreRequiredObjectivesDone(const FQuestDefinition& Definition, const FQuestProgress& Progress)
	{
		for (size_t Index = 0; Index < Definition.Objectives.size(); ++Index)
		{
			const FObjectiveDefinition& Objective = Definition.Objectives[Index];
			if (!Objective.bOptional && Progress.ObjectiveCounts[Index] < Objective.Required)
			{
				return false;
			}
		}
		return true;
	}

	EQuestResult FQuestLog::Start(const FQuestDatabase& Database, std::string_view QuestId, const FStoryFlags& Flags)
	{
		const FQuestDefinition* Definition = Database.Find(QuestId);
		if (!Definition)
		{
			return EQuestResult::UnknownQuest;
		}

		FQuestProgress* Existing = FindMutable(QuestId);
		if (Existing)
		{
			if (Existing->Status == EQuestStatus::Completed)
			{
				return EQuestResult::AlreadyCompleted;
			}
			if (Existing->Status == EQuestStatus::Active || Existing->Status == EQuestStatus::ReadyToTurnIn)
			{
				return EQuestResult::AlreadyActive;
			}
		}

		for (const std::string& Flag : Definition->RequiredStoryFlags)
		{
			if (Flags.find(Flag) == Flags.end())
			{
				return EQuestResult::MissingStoryFlag;
			}
		}
		for (const std::string& Prerequisite : Definition->PrerequisiteQuests)
		{
			if (!IsCompleted(Prerequisite))
			{
				return EQuestResult::MissingPrerequisite;
			}
		}

		if (!Existing)
		{
			Entries.emplace_back();
			Existing = &Entries.back();
			Existing->QuestId = Definition->Id;
		}
		Existing->Status = EQuestStatus::Active;
		Existing->ObjectiveCounts.assign(Definition->Objectives.size(), 0);
		return EQuestResult::Ok;
	}

	std::vector<FQuestUpdate> FQuestLog::ApplyEvent(const FQuestDatabase& Database, const FQuestEvent& Event)
	{
		std::vector<FQuestUpdate> Updates;
		if (Event.Amount <= 0)
		{
			return Updates;
		}

		for (FQuestProgress& Entry : Entries)
		{
			if (Entry.Status != EQuestStatus::Active)
			{
				continue;
			}
			const FQuestDefinition* Definition = Database.Find(Entry.QuestId);
			if (!Definition)
			{
				continue;
			}

			for (size_t Index = 0; Index < Definition->Objectives.size(); ++Index)
			{
				const FObjectiveDefinition& Objective = Definition->Objectives[Index];
				int32& Count = Entry.ObjectiveCounts[Index];
				const bool bDone = Count >= Objective.Required;

				if (!bDone && Objective.Kind == Event.Kind && Objective.Target == Event.Target)
				{
					Count = std::min(Objective.Required, Count + Event.Amount);
					Updates.push_back({Entry.QuestId, static_cast<int32>(Index), Count, EQuestStatus::Active});
				}
				// Sequential quests: stop at the first unfinished required objective.
				if (Definition->bSequential && !Objective.bOptional && Count < Objective.Required)
				{
					break;
				}
			}

			if (AreRequiredObjectivesDone(*Definition, Entry))
			{
				Entry.Status = Definition->bAutoComplete ? EQuestStatus::Completed : EQuestStatus::ReadyToTurnIn;
				Updates.push_back({Entry.QuestId, -1, 0, Entry.Status});
			}
		}
		return Updates;
	}

	EQuestResult FQuestLog::TurnIn(const FQuestDatabase& Database, std::string_view QuestId)
	{
		if (!Database.Find(QuestId))
		{
			return EQuestResult::UnknownQuest;
		}
		FQuestProgress* Entry = FindMutable(QuestId);
		if (!Entry || Entry->Status != EQuestStatus::ReadyToTurnIn)
		{
			return Entry && Entry->Status == EQuestStatus::Active ? EQuestResult::NotReady : EQuestResult::NotActive;
		}
		Entry->Status = EQuestStatus::Completed;
		return EQuestResult::Ok;
	}

	EQuestResult FQuestLog::Fail(std::string_view QuestId)
	{
		FQuestProgress* Entry = FindMutable(QuestId);
		if (!Entry || (Entry->Status != EQuestStatus::Active && Entry->Status != EQuestStatus::ReadyToTurnIn))
		{
			return EQuestResult::NotActive;
		}
		Entry->Status = EQuestStatus::Failed;
		return EQuestResult::Ok;
	}

	EQuestResult FQuestLog::Abandon(const FQuestDatabase& Database, std::string_view QuestId)
	{
		const FQuestDefinition* Definition = Database.Find(QuestId);
		if (!Definition)
		{
			return EQuestResult::UnknownQuest;
		}
		if (!Definition->bCanAbandon)
		{
			return EQuestResult::CannotAbandon;
		}
		FQuestProgress* Entry = FindMutable(QuestId);
		if (!Entry || (Entry->Status != EQuestStatus::Active && Entry->Status != EQuestStatus::ReadyToTurnIn))
		{
			return EQuestResult::NotActive;
		}
		Entry->Status = EQuestStatus::Inactive;
		Entry->ObjectiveCounts.clear();
		return EQuestResult::Ok;
	}

	void FQuestLog::Reconcile(const FQuestDatabase& Database)
	{
		for (FQuestProgress& Entry : Entries)
		{
			const FQuestDefinition* Definition = Database.Find(Entry.QuestId);
			if (!Definition)
			{
				continue; // keep unknown quests untouched; content may be re-added later
			}
			Entry.ObjectiveCounts.resize(Definition->Objectives.size(), 0);
			for (size_t Index = 0; Index < Definition->Objectives.size(); ++Index)
			{
				Entry.ObjectiveCounts[Index] = std::clamp(Entry.ObjectiveCounts[Index], 0, Definition->Objectives[Index].Required);
			}
		}
	}
}
