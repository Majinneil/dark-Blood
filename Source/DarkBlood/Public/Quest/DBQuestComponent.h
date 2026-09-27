// Holds one quest log. Used on the GameState (shared story quests) and on each PlayerState (personal quests).
#pragma once

#include "Components/ActorComponent.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/Quest.h"

#include "DBQuestComponent.generated.h"

class UDBQuestComponent;

USTRUCT(BlueprintType)
struct FDBQuestProgressView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	EDBQuestStatus Status = EDBQuestStatus::Inactive;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TArray<int32> ObjectiveCounts;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDBOnQuestLogChanged, UDBQuestComponent*, QuestLog);

UCLASS(ClassGroup = (DarkBlood), meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBQuestComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: starts a quest in this log. Story flags come from the world state. */
	DarkBlood::Rules::EQuestResult StartQuest(FName QuestId, const DarkBlood::Rules::FStoryFlags& Flags);

	/** Server: routes an event; returns ids of quests that were completed by it. */
	TArray<FName> HandleEvent(const DarkBlood::Rules::FQuestEvent& Event);

	DarkBlood::Rules::EQuestResult TurnIn(FName QuestId);
	DarkBlood::Rules::EQuestResult Abandon(FName QuestId);

	void RestoreFromRecord(const DarkBlood::Rules::FQuestLog& InLog);
	const DarkBlood::Rules::FQuestLog& GetLog() const { return Log; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Quest")
	TArray<FDBQuestProgressView> GetQuests() const { return Quests; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Quest")
	EDBQuestStatus GetQuestStatus(FName QuestId) const;

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|Quest")
	FDBOnQuestLogChanged OnQuestLogChanged;

private:
	UFUNCTION()
	void OnRep_Quests();

	void SyncReplicatedView();

	DarkBlood::Rules::FQuestLog Log;

	UPROPERTY(ReplicatedUsing = OnRep_Quests)
	TArray<FDBQuestProgressView> Quests;
};
