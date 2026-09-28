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
	/** Spawns a lesser demon (melee AI) Distance cm in front of the player. */
	UFUNCTION(Exec) void DBSpawnEnemy(float Distance = 800.f);
	/** Every training dummy swings at the nearest player in reach. */
	UFUNCTION(Exec) void DBDummyAttack();
	/** Training dummies swing every Interval seconds (0 = off). */
	UFUNCTION(Exec) void DBDummyAutoAttack(float Interval);
	/** Local: presses an ability input (e.g. LightAttack, HeavyAttack, Dodge, Block, Sprint) and releases it after HoldSeconds. */
	UFUNCTION(Exec) void DBInput(const FString& Input, float HoldSeconds = 0.05f);
	/** Local: presses jump (like the jump button); logs the jump count. */
	UFUNCTION(Exec) void DBJump();
	/** Local: toggles the target lock (like the lock-on button). */
	UFUNCTION(Exec) void DBLockOn();
	/** Local: runs Command after Seconds (scripted tests: -ExecCmds="DBAfter 2 DBInput LightAttack"). */
	UFUNCTION(Exec) void DBAfter(float Seconds, const FString& Command);
	/** Vitals, poise and combat tags of players and enemies. */
	UFUNCTION(Exec) void DBDumpCombat();

	// ---- Story (Phase 3) ----
	/** Spawns the development story slice (king, dummies, captain, demon encounter) around the player. */
	UFUNCTION(Exec) void DBSetupSlice();
	/** Teleports the player in front of the nearest NPC / living enemy whose id contains Target (e.g. NPC_King, LesserDemon). */
	UFUNCTION(Exec) void DBGoto(const FString& Target);
	/** Local: picks dialogue option Index (0-based, as displayed). */
	UFUNCTION(Exec) void DBDialogueChoose(int32 Index);
	/** Local: completes the character creator (e.g. "DBCreateCharacter Warrior Jin Akagi"). */
	UFUNCTION(Exec) void DBCreateCharacter(FName ClassId, const FString& Name);

	// ---- Classes (Phase 4) ----
	/** Unlocks one rank of a skill node (normal rules: points, level, prerequisites). */
	UFUNCTION(Exec) void DBUnlockSkill(FName NodeId);

	// ---- Economy (Phase 5) ----
	/** Uses the consumable in Section/Index. */
	UFUNCTION(Exec) void DBUseItem(int32 Section, int32 Index);
	/** Crafts RecipeId at the nearest crafting station (within reach). */
	UFUNCTION(Exec) void DBCraft(FName RecipeId);
	/** Repairs equipped gear at the nearest station. */
	UFUNCTION(Exec) void DBRepair();
	/** Rolls a loot table for the player. */
	UFUNCTION(Exec) void DBGrantLoot(FName LootTableId);
	/** Equips the first carried item with this id in its default slot. */
	UFUNCTION(Exec) void DBEquipById(FName ItemId);
	/** Uses the first carried consumable with this id. */
	UFUNCTION(Exec) void DBUse(FName ItemId);

	// ---- Visual foundation (Phase 5.5) ----
	/** Server: builds (1) or removes (0) the visual slice on every machine. */
	UFUNCTION(Exec) void DBVisualSlice(int32 bEnabled);
	/** Server: Day | Dusk | Night | DemonNight. */
	UFUNCTION(Exec) void DBTimeOfDay(const FString& Preset);
	/** Local: character visual profiles on (1) or greybox bodies (0). */
	UFUNCTION(Exec) void DBVisuals(int32 bEnabled);
	/** Local: frame time, GPU time, draw calls, primitives and slice statistics. */
	UFUNCTION(Exec) void DBPerfSnapshot();
	/** Graphics settings for tests: quality 0-4, upscaling 0 (native) - 4, hardware ray tracing 0/1 (not saved). */
	UFUNCTION(Exec) void DBGraphics(int32 Quality, int32 Upscaling = 0, int32 bRayTracing = 1);
	/** Local: missing / placeholder visual references (materials, profiles, animation sets). */
	UFUNCTION(Exec) void DBVisualAudit();
	/** Server: puts your character at X Y (relative to the slice origin) facing Yaw, camera pitch Pitch. */
	UFUNCTION(Exec) void DBView(float X, float Y, float Yaw, float Pitch = -10.f);
	/** Local camera orbit around your character for visual checks: Yaw relative to its facing (180 = front), arm length in cm. */
	UFUNCTION(Exec) void DBOrbit(float Yaw, float Pitch = -10.f, float Distance = 250.f);

private:
	/** On clients: sends the command to the server and returns true. */
	bool ForwardToServer(const FString& Command) const;
	ADBPlayerState* GetDBPlayerState() const;
};
