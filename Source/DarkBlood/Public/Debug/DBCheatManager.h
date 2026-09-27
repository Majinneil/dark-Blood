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

	// ---- Combat (Phase 2) ----
	/** Spawns a training dummy Distance cm in front of the player. */
	UFUNCTION(Exec) void DBSpawnDummy(float Distance = 200.f);
	/** Every training dummy swings at the nearest player in reach. */
	UFUNCTION(Exec) void DBDummyAttack();
	/** Training dummies swing every Interval seconds (0 = off). */
	UFUNCTION(Exec) void DBDummyAutoAttack(float Interval);
	/** Local: presses an ability input (e.g. LightAttack, HeavyAttack, Dodge, Block, Sprint) and releases it after HoldSeconds. */
	UFUNCTION(Exec) void DBInput(const FString& Input, float HoldSeconds = 0.05f);
	/** Local: toggles the target lock (like the lock-on button). */
	UFUNCTION(Exec) void DBLockOn();
	/** Local: runs Command after Seconds (scripted tests: -ExecCmds="DBAfter 2 DBInput LightAttack"). */
	UFUNCTION(Exec) void DBAfter(float Seconds, const FString& Command);
	/** Vitals, poise and combat tags of players and enemies. */
	UFUNCTION(Exec) void DBDumpCombat();

private:
	/** On clients: sends the command to the server and returns true. */
	bool ForwardToServer(const FString& Command) const;
	ADBPlayerState* GetDBPlayerState() const;
};
