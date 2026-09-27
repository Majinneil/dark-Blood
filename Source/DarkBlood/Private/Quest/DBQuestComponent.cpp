#include "Quest/DBQuestComponent.h"

#include "Core/DBRulesBridge.h"
#include "Data/DBGameDataSubsystem.h"
#include "Net/UnrealNetwork.h"

namespace R = DarkBlood::Rules;

namespace
{
	const R::FQuestDatabase* GetQuestDatabase(const UObject* Context)
	{
		const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(Context);
		return Data ? &Data->GetQuestDatabase() : nullptr;
	}
}

UDBQuestComponent::UDBQuestComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UDBQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Shared quests (GameState) go to everyone; personal quests (PlayerState) are visible to party members too.
	DOREPLIFETIME_CONDITION(UDBQuestComponent, Quests, COND_None);
}

R::EQuestResult UDBQuestComponent::StartQuest(FName QuestId, const R::FStoryFlags& Flags)
{
	const R::FQuestDatabase* Database = GetQuestDatabase(this);
	if (!Database || !GetOwner()->HasAuthority())
	{
		return R::EQuestResult::UnknownQuest;
	}
	const R::EQuestResult Result = Log.Start(*Database, DBBridge::ToStd(QuestId), Flags);
	if (Result == R::EQuestResult::Ok)
	{
		SyncReplicatedView();
	}
	return Result;
}

TArray<FName> UDBQuestComponent::HandleEvent(const R::FQuestEvent& Event)
{
	TArray<FName> Completed;
	const R::FQuestDatabase* Database = GetQuestDatabase(this);
	if (!Database || !GetOwner()->HasAuthority())
	{
		return Completed;
	}
	const std::vector<R::FQuestUpdate> Updates = Log.ApplyEvent(*Database, Event);
	for (const R::FQuestUpdate& Update : Updates)
	{
		if (Update.NewStatus == R::EQuestStatus::Completed)
		{
			Completed.Add(DBBridge::ToFName(Update.QuestId));
		}
	}
	if (!Updates.empty())
	{
		SyncReplicatedView();
	}
	return Completed;
}

R::EQuestResult UDBQuestComponent::TurnIn(FName QuestId)
{
	const R::FQuestDatabase* Database = GetQuestDatabase(this);
	if (!Database || !GetOwner()->HasAuthority())
	{
		return R::EQuestResult::UnknownQuest;
	}
	const R::EQuestResult Result = Log.TurnIn(*Database, DBBridge::ToStd(QuestId));
	if (Result == R::EQuestResult::Ok)
	{
		SyncReplicatedView();
	}
	return Result;
}

R::EQuestResult UDBQuestComponent::Abandon(FName QuestId)
{
	const R::FQuestDatabase* Database = GetQuestDatabase(this);
	if (!Database || !GetOwner()->HasAuthority())
	{
		return R::EQuestResult::UnknownQuest;
	}
	const R::EQuestResult Result = Log.Abandon(*Database, DBBridge::ToStd(QuestId));
	if (Result == R::EQuestResult::Ok)
	{
		SyncReplicatedView();
	}
	return Result;
}

void UDBQuestComponent::RestoreFromRecord(const R::FQuestLog& InLog)
{
	Log = InLog;
	if (const R::FQuestDatabase* Database = GetQuestDatabase(this))
	{
		Log.Reconcile(*Database);
	}
	SyncReplicatedView();
}

EDBQuestStatus UDBQuestComponent::GetQuestStatus(FName QuestId) const
{
	const FDBQuestProgressView* Found = Quests.FindByPredicate([QuestId](const FDBQuestProgressView& Quest) { return Quest.QuestId == QuestId; });
	return Found ? Found->Status : EDBQuestStatus::Inactive;
}

void UDBQuestComponent::SyncReplicatedView()
{
	Quests.Reset();
	for (const R::FQuestProgress& Progress : Log.GetAll())
	{
		FDBQuestProgressView& View = Quests.AddDefaulted_GetRef();
		View.QuestId = DBBridge::ToFName(Progress.QuestId);
		View.Status = DBBridge::CastEnum<EDBQuestStatus>(Progress.Status);
		View.ObjectiveCounts.Append(Progress.ObjectiveCounts.data(), static_cast<int32>(Progress.ObjectiveCounts.size()));
	}
	OnQuestLogChanged.Broadcast(this);
}

void UDBQuestComponent::OnRep_Quests()
{
	OnQuestLogChanged.Broadcast(this);
}
