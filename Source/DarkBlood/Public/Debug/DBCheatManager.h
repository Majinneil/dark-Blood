// Developer commands (console). Every command executes on the server; clients forward automatically.
// Not available in shipping builds. Full list: docs/ARCHITECTURE.md#entwicklerkommandos
#pragma once

#include "GameFramework/CheatManager.h"

#include "DBCheatManager.generated.h"

class ADBPlayerState;

UCLASS()
class DARKBLOOD_API UDBCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec) void DBGiveXp(int32 Amount);
	UFUNCTION(Exec) void DBSetLevel(int32 Level);
	UFUNCTION(Exec) void DBGiveSkillPoints(int32 Amount);
	UFUNCTION(Exec) void DBGiveItem(FName ItemId, int32 Count);
	UFUNCTION(Exec) void DBGiveCurrency(int32 Amount);
	UFUNCTION(Exec) void DBEquipBag(int32 Section, int32 Index);
	UFUNCTION(Exec) void DBEquipItem(int32 Section, int32 Index, const FString& Slot);
	UFUNCTION(Exec) void DBStartQuest(FName QuestId);
	UFUNCTION(Exec) void DBQuestEvent(const FString& Kind, FName Target, int32 Amount);
	UFUNCTION(Exec) void DBSetStoryFlag(FName Flag);
	UFUNCTION(Exec) void DBDefeatBoss(FName BossId, const FString& Rank, FName RegionId);
	UFUNCTION(Exec) void DBSetTime(float Hour);
	UFUNCTION(Exec) void DBDamageSelf(float Amount);
	UFUNCTION(Exec) void DBHeal();
	UFUNCTION(Exec) void DBSaveAll();
	UFUNCTION(Exec) void DBDumpCharacter();
	UFUNCTION(Exec) void DBDumpWorld();

private:
	/** On clients: sends the command to the server and returns true. */
	bool ForwardToServer(const FString& Command) const;
	ADBPlayerState* GetDBPlayerState() const;
};
