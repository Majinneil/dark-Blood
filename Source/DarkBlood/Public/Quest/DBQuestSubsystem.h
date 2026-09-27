// Server-side quest router: gameplay reports events here, the subsystem updates shared and personal
// quest logs and hands out rewards. Rewards are never granted by clients.
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/Quest.h"

#include "DBQuestSubsystem.generated.h"

class ADBPlayerState;
class APlayerState;
class UDBQuestComponent;

UCLASS()
class DARKBLOOD_API UDBQuestSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDBQuestSubsystem* Get(const UObject* WorldContext);

	/**
	 * Reports a gameplay event (kill, pickup, reached location, talk, interaction, scripted event).
	 * Kill and custom events count for every player (co-op); the others only for the instigator.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Quest")
	void ReportEvent(EDBObjectiveKind Kind, FName Target, int32 Amount, APlayerState* Instigator);

	/** Starts a quest in the correct log (shared or personal of ForPlayer). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Quest")
	bool StartQuest(FName QuestId, APlayerState* ForPlayer);

	/** Hands in a quest waiting in ReadyToTurnIn. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Quest")
	bool TurnInQuest(FName QuestId, APlayerState* ForPlayer);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UDBQuestComponent* GetSharedLog() const;
	DarkBlood::Rules::FStoryFlags GetStoryFlags() const;
	void GrantCompletion(FName QuestId, ADBPlayerState* PersonalOwner);
	void GrantReward(const DarkBlood::Rules::FQuestReward& Reward, ADBPlayerState* Player) const;

	/** On-screen message for one player, or for everyone when OnlyFor is null. */
	void Notify(const FText& Text, ADBPlayerState* OnlyFor) const;
	FText GetQuestTitle(FName QuestId) const;
};
