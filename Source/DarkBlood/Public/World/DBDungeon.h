// Dungeons of the open world (Phase 9). Each dungeon has an entrance gate in its region and an interior generated from
// its seed (DarkBloodRules/Dungeon.h) - built identically on every machine, far outside the landscape. Rooms wake when a
// player enters: demons in combat rooms, fire vents in trap rooms, the guardian in the last room. Treasure rooms hold
// a chest, the rest room a healing shrine. Clearing the guardian's room clears the dungeon (saved; demons return after
// three game days) and opens the way out.
#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/DBInteractable.h"

#include "DarkBloodRules/Dungeon.h"

#include "DBDungeon.generated.h"

class ADBEnemyCharacter;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

struct FDBDungeonSite
{
	FName Id;
	FString Name;
	FName RegionId;
	int32 Difficulty = 1;
	/** Entrance gate on the landscape (m). */
	FVector2D Entrance = FVector2D::ZeroVector;
	/** Corner of the interior grid (world, cm). */
	FVector InteriorOrigin = FVector::ZeroVector;
	uint32 Seed = 1;

	DarkBlood::Rules::FDungeonParams GetParams() const;
};

namespace DBDungeon
{
	/** Size of one interior cell and the wall height (cm). */
	constexpr float CellSize = 600.f;
	constexpr float WallHeight = 520.f;

	DARKBLOOD_API const TArray<FDBDungeonSite>& GetSites();
	/** Site by id or name prefix; -1 if unknown. */
	DARKBLOOD_API int32 FindSite(const FString& IdOrName);
	/** World position of the center of a cell of a site's interior (floor height). */
	DARKBLOOD_API FVector CellToWorld(const FDBDungeonSite& Site, int32 X, int32 Y);
}

/** The interior of one dungeon. Spawned by the server on the first entry, replicated (clients build it locally). */
UCLASS()
class DARKBLOOD_API ADBDungeonInstance : public AActor
{
	GENERATED_BODY()

public:
	ADBDungeonInstance();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: the instance of a site (spawns it the first time). */
	static ADBDungeonInstance* FindOrSpawn(UWorld* World, int32 SiteIndex);
	static ADBDungeonInstance* Find(const UWorld* World, int32 SiteIndex);
	/** Instance whose interior contains a world position. */
	static ADBDungeonInstance* FindAt(const UWorld* World, const FVector& Location);

	/** Server: moves a player into the entrance room / back out to the gate. */
	static bool Enter(APlayerController* User, int32 SiteIndex);
	static void Leave(APlayerController* User, int32 SiteIndex);

	int32 GetSiteIndex() const { return SiteIndex; }
	const DarkBlood::Rules::FDungeonLayout& GetLayout() const { return Layout; }
	bool ContainsLocation(const FVector& Location) const;
	FVector GetRoomCenter(int32 Room) const;
	/** Server: 0 dormant, 1 fighting, 2 cleared. */
	uint8 GetRoomState(int32 Room) const { return RoomStates.IsValidIndex(Room) ? RoomStates[Room] : 0; }
	int32 CountLivingEnemies(int32 Room) const;
	bool IsCleared() const { return bCleared; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Site();
	/** Every machine: floors, walls, ceilings and lights from the layout. */
	void Build();
	/** Server: chests, shrine, traps and the way out. */
	void SpawnFixtures();
	void ActivateRoom(int32 Room);
	void ClearDungeon();
	void NotifyPlayersInside(const FText& Text) const;
	TArray<APawn*> GetPlayersInside() const;

	UPROPERTY(ReplicatedUsing = OnRep_Site)
	int32 SiteIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Dungeon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	DarkBlood::Rules::FDungeonLayout Layout;
	bool bBuilt = false;

	// Server state
	TArray<uint8> RoomStates;
	TArray<TArray<TWeakObjectPtr<ADBEnemyCharacter>>> RoomEnemies;
	bool bCleared = false;
	float UpdateTimer = 0.f;
	float EmptySeconds = 0.f;
};

/** Gate into a dungeon (on the landscape) or out of it (in the entrance room and, once cleared, the guardian's room). */
UCLASS()
class DARKBLOOD_API ADBDungeonPortal : public AActor, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBDungeonPortal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override { return User != nullptr; }
	virtual void Interact(APlayerController* User) override;
	virtual float GetInteractionRange() const override { return 450.f; }

	void Setup(int32 InSiteIndex, bool bInExit);
	int32 GetSiteIndex() const { return SiteIndex; }
	bool IsExit() const { return bExit; }

	/** Server: one entrance gate per dungeon on the landscape (idempotent). */
	static void SpawnEntrances(UWorld* World);

private:
	UFUNCTION()
	void OnRep_Setup();

	UPROPERTY(ReplicatedUsing = OnRep_Setup)
	int32 SiteIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Setup)
	bool bExit = false;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Dungeon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Dungeon")
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Dungeon")
	TObjectPtr<UTextRenderComponent> Label;
};

/** Fire vent of a trap room: glows as a warning, then bursts and burns everyone standing on it. */
UCLASS()
class DARKBLOOD_API ADBDungeonTrap : public AActor
{
	GENERATED_BODY()

public:
	ADBDungeonTrap();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Cycle: 1.2 s quiet, 1 s warning glow, burst (share of max health), 0.8 s cooling. */
	static constexpr float CycleSeconds = 3.f;
	static constexpr float DamageShare = 0.12f;

	UPROPERTY(Replicated)
	float PhaseOffset = 0.f;

	/** Server: hits so far (tests). */
	int32 GetBurstHits() const { return BurstHits; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Dungeon")
	TObjectPtr<UStaticMeshComponent> Plate;

	int32 LastBurstCycle = -1;
	int32 BurstHits = 0;
	int32 VisualState = -1;
};

/** Rest shrine: restores health, stamina and mana. */
UCLASS()
class DARKBLOOD_API ADBDungeonShrine : public AActor, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBDungeonShrine();

	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override { return User != nullptr; }
	virtual void Interact(APlayerController* User) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Dungeon")
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;
};
