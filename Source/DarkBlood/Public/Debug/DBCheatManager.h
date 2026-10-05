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
	/** Open world: teleport to a region of the realm (id, name or index 0-15), optionally offset in meters. */
	UFUNCTION(Exec) void DBTravel(const FString& Region, float OffsetX = 0.f, float OffsetY = 0.f);
	/** Local: moves the own character through normal movement input towards a world yaw (0 = east, 90 = south). */
	UFUNCTION(Exec) void DBWalk(float Yaw, float Seconds);
	/** Settlement simulation: state of every settlement / let game hours pass / demon attack fight in a settlement. */
	UFUNCTION(Exec) void DBDumpSettlements();
	UFUNCTION(Exec) void DBDumpVillagers();
	UFUNCTION(Exec) void DBSkipHours(float Hours);
	UFUNCTION(Exec) void DBSettlementAttack(const FString& Settlement);
	/** Survival: set satiety and warmth (0..100). */
	UFUNCTION(Exec) void DBSurvival(float Satiety, float Warmth);
	/** Calls the own horse and mounts it, or dismounts. */
	UFUNCTION(Exec) void DBRide();
	/** Travels by carriage from the nearest station (normal checks: range, fare). */
	UFUNCTION(Exec) void DBCarriage(const FString& Destination);
	/** Dungeons: enter by id / name, jump to a room (index or kind) inside, state of all dungeons; kill enemies nearby. */
	UFUNCTION(Exec) void DBDungeonEnter(const FString& Dungeon);
	UFUNCTION(Exec) void DBDungeonRoom(const FString& Room);
	UFUNCTION(Exec) void DBDungeonDump();
	UFUNCTION(Exec) void DBKillNearby(float RadiusMeters);

	/** Bosses (Phase 10): list with arena states; teleport into an arena (starts the fight if open); spawn one in front;
	 * state of the bosses nearby; set the nearest boss to a health fraction (phases); defeat bosses ("Outer" = the 14 outside). */
	UFUNCTION(Exec) void DBBossList();
	UFUNCTION(Exec) void DBBossArena(const FString& Boss);
	UFUNCTION(Exec) void DBBossSpawn(const FString& Boss, float Distance = 1500.f);
	UFUNCTION(Exec) void DBBossDump();
	UFUNCTION(Exec) void DBBossHurt(float HealthFraction);
	UFUNCTION(Exec) void DBBossDefeat(const FString& Boss);
	/** The nearest boss uses its signature attack now (Phase 12). */
	UFUNCTION(Exec) void DBBossSignature();

	/** Regions (Phase 11): packs, camps and region states; teleport to a region's demon camp; spawn a pack now. */
	UFUNCTION(Exec) void DBRegionDump();
	UFUNCTION(Exec) void DBCamp(const FString& Region);
	UFUNCTION(Exec) void DBRegionPack();
	/** Spawns a sailing ship at the water line in front of the player (Style 0 war, 1 fighting, 2 merchant, 3 boat). */
	UFUNCTION(Exec) void DBSpawnShip(float Distance = 2500.f, int32 Style = 2);
	/** Takes (or leaves) the helm of the nearest ship and steers it for Seconds (Rudder/Sails -1..1); logs the course. */
	/** Puts the player on the deck of the nearest ship of a style (0 war, 1 fighting, 2 merchant, 3 boat). */
	UFUNCTION(Exec) void DBBoardShip(int32 Style);
	/** Sets up every free model of the library in a row ahead of the player (size and facing check); again clears it. */
	UFUNCTION(Exec) void DBModelShowroom(float Spacing = 5000.f);
	/** Shows one library model Distance ahead of the player, turned by Yaw (0: its front faces the player). */
	UFUNCTION(Exec) void DBModelShow(const FString& Key, float Distance = 3000.f, float Yaw = 0.f);
	UFUNCTION(Exec) void DBSail(float Rudder = 0.f, float Sails = 1.f, float Seconds = 10.f);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ShowroomActors;

	/** On clients: sends the command to the server and returns true. */
	bool ForwardToServer(const FString& Command) const;
	ADBPlayerState* GetDBPlayerState() const;
};
