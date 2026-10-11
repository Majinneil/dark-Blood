#include "Quest/DBQuestSubsystem.h"

#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBQuestDefinition.h"
#include "Engine/World.h"
#include "Framework/DBGameState.h"
#include "Inventory/DBInventoryComponent.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "Quest/DBQuestComponent.h"
#include "World/DBWorldStateComponent.h"

namespace R = DarkBlood::Rules;

UDBQuestSubsystem* UDBQuestSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDBQuestSubsystem>() : nullptr;
}

bool UDBQuestSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UDBQuestComponent* UDBQuestSubsystem::GetSharedLog() const
{
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	return GameState ? GameState->GetSharedQuests() : nullptr;
}

R::FStoryFlags UDBQuestSubsystem::GetStoryFlags() const
{
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	const UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr;
	return WorldState ? WorldState->GetRulesState().StoryFlags : R::FStoryFlags();
}

bool UDBQuestSubsystem::StartQuest(FName QuestId, APlayerState* ForPlayer)
{
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		return false;
	}
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	const UDBQuestDefinition* Definition = Data ? Data->FindQuest(QuestId) : nullptr;
	if (!Definition)
	{
		UE_LOG(LogDBQuest, Warning, TEXT("StartQuest: unknown quest %s"), *QuestId.ToString());
		return false;
	}

	UDBQuestComponent* Log = nullptr;
	if (Definition->Scope == EDBQuestScope::Shared)
	{
		Log = GetSharedLog();
	}
	else if (const ADBPlayerState* Player = Cast<ADBPlayerState>(ForPlayer))
	{
		Log = Player->GetPersonalQuests();
	}
	if (!Log)
	{
		return false;
	}

	const R::EQuestResult Result = Log->StartQuest(QuestId, GetStoryFlags());
	UE_LOG(LogDBQuest, Log, TEXT("StartQuest %s: %hs"), *QuestId.ToString(), R::ToString(Result));
	if (Result == R::EQuestResult::Ok)
	{
		Notify(FText::Format(NSLOCTEXT("DarkBlood", "QuestStarted", "Neue Quest: {0}"), GetQuestTitle(QuestId)),
			Definition->Scope == EDBQuestScope::Shared ? nullptr : Cast<ADBPlayerState>(ForPlayer));
		// Bosses beaten before the quest began still count (a vassal felled on the way, a guardian before the gate quest).
		const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
		const UDBWorldStateComponent* World = GameState ? GameState->GetWorldState() : nullptr;
		for (const FDBQuestObjective& Objective : Definition->Objectives)
		{
			if (World && Objective.Kind == EDBObjectiveKind::Kill && World->GetRulesState().DefeatedBosses.count(DBBridge::ToStd(Objective.Target)) > 0)
			{
				ReportEvent(EDBObjectiveKind::Kill, Objective.Target, Objective.Required, ForPlayer);
			}
		}
	}
	return Result == R::EQuestResult::Ok;
}

void UDBQuestSubsystem::ReportEvent(EDBObjectiveKind Kind, FName Target, int32 Amount, APlayerState* Instigator)
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (GetWorld()->GetNetMode() == NM_Client || !GameState)
	{
		return;
	}

	R::FQuestEvent Event;
	Event.Kind = DBBridge::CastEnum<R::EObjectiveKind>(Kind);
	Event.Target = DBBridge::ToStd(Target);
	Event.Amount = Amount;

	if (UDBQuestComponent* Shared = GetSharedLog())
	{
		for (const FName& QuestId : Shared->HandleEvent(Event))
		{
			GrantCompletion(QuestId, nullptr);
		}
	}

	// Kills and scripted events are group achievements; pickups, talks and locations are personal. A kill counts for
	// the players who fought together - within 150 m of the one who landed the blow - not for a friend on the other side
	// of the realm (Phase 19). Without a known killer (a trap, a hazard) it counts for everyone.
	const bool bGroupEvent = Kind == EDBObjectiveKind::Kill || Kind == EDBObjectiveKind::Custom;
	const APawn* KillerPawn = Kind == EDBObjectiveKind::Kill && Instigator ? Instigator->GetPawn() : nullptr;
	constexpr float GroupRange = 15000.f;
	for (APlayerState* Candidate : GameState->PlayerArray)
	{
		ADBPlayerState* Player = Cast<ADBPlayerState>(Candidate);
		if (!Player || (!bGroupEvent && Candidate != Instigator))
		{
			continue;
		}
		const APawn* PlayerPawn = Player->GetPawn();
		if (KillerPawn && Candidate != Instigator && (!PlayerPawn || FVector::Dist(PlayerPawn->GetActorLocation(), KillerPawn->GetActorLocation()) > GroupRange))
		{
			continue;
		}
		if (UDBQuestComponent* Personal = Player->GetPersonalQuests())
		{
			for (const FName& QuestId : Personal->HandleEvent(Event))
			{
				GrantCompletion(QuestId, Player);
			}
		}
	}
}

bool UDBQuestSubsystem::TurnInQuest(FName QuestId, APlayerState* ForPlayer)
{
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	const UDBQuestDefinition* Definition = Data ? Data->FindQuest(QuestId) : nullptr;
	if (GetWorld()->GetNetMode() == NM_Client || !Definition)
	{
		return false;
	}
	ADBPlayerState* Player = Cast<ADBPlayerState>(ForPlayer);
	UDBQuestComponent* Log = Definition->Scope == EDBQuestScope::Shared ? GetSharedLog() : (Player ? Player->GetPersonalQuests() : nullptr);
	if (!Log || Log->TurnIn(QuestId) != R::EQuestResult::Ok)
	{
		return false;
	}
	GrantCompletion(QuestId, Definition->Scope == EDBQuestScope::Shared ? nullptr : Player);
	return true;
}

void UDBQuestSubsystem::GrantCompletion(FName QuestId, ADBPlayerState* PersonalOwner)
{
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	const R::FQuestDefinition* Definition = Data ? Data->GetQuestDatabase().Find(DBBridge::ToStd(QuestId)) : nullptr;
	if (!Definition)
	{
		return;
	}
	UE_LOG(LogDBQuest, Log, TEXT("Quest completed: %s"), *QuestId.ToString());
	Notify(FText::Format(NSLOCTEXT("DarkBlood", "QuestCompleted", "Quest abgeschlossen: {0}"), GetQuestTitle(QuestId)), PersonalOwner);

	if (PersonalOwner)
	{
		GrantReward(Definition->Reward, PersonalOwner);
	}
	else if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		// Shared (story) quests reward every player of the session.
		for (APlayerState* Candidate : GameState->PlayerArray)
		{
			if (ADBPlayerState* Player = Cast<ADBPlayerState>(Candidate); Player && Player->IsCharacterReady())
			{
				GrantReward(Definition->Reward, Player);
			}
		}
	}

	if (!Definition->Reward.StoryFlags.empty())
	{
		const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
		if (UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr)
		{
			for (const std::string& Flag : Definition->Reward.StoryFlags)
			{
				WorldState->SetStoryFlag(DBBridge::ToFName(Flag));
			}
		}
	}
}

void UDBQuestSubsystem::GrantReward(const R::FQuestReward& Reward, ADBPlayerState* Player) const
{
	if (!Player)
	{
		return;
	}
	if (UDBProgressionComponent* Progression = Player->GetProgression())
	{
		Progression->AwardXp(Reward.Xp);
		Progression->AwardSkillPoints(Reward.SkillPoints);
	}
	if (UDBInventoryComponent* Inventory = Player->GetInventory())
	{
		Inventory->AddCurrency(Reward.Currency);
		for (const R::FItemStack& Item : Reward.Items)
		{
			Inventory->DeliverItem(DBBridge::ToFName(Item.ItemId), Item.Count);
		}
	}
}

FText UDBQuestSubsystem::GetQuestTitle(FName QuestId) const
{
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	const UDBQuestDefinition* Definition = Data ? Data->FindQuest(QuestId) : nullptr;
	return Definition && !Definition->Title.IsEmpty() ? Definition->Title : FText::FromName(QuestId);
}

void UDBQuestSubsystem::Notify(const FText& Text, ADBPlayerState* OnlyFor) const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return;
	}
	for (APlayerState* Candidate : GameState->PlayerArray)
	{
		if (OnlyFor && Candidate != OnlyFor)
		{
			continue;
		}
		if (ADBPlayerController* Controller = Cast<ADBPlayerController>(Candidate->GetOwner()))
		{
			Controller->ClientShowNotification(Text);
		}
	}
}
