// Placed once in L_Realm (DBBuildRealmCommandlet). Server: assigns each player the region of the realm layout they
// stand in (DBRealmLayout; region volumes still win where placed). Every machine: builds the settlements of the realm
// locally and deterministically (like the visual slice), so co-op costs no bandwidth.
#pragma once

#include "GameFramework/Info.h"

#include "DBRealmDirector.generated.h"

class ADBModularBuilding;
class FDBArtBatcher;
class UDBRealmMoodComponent;
class UDBSettlementLifeComponent;
class UInstancedStaticMeshComponent;

/** Shared house geometry of a block of a settlement's houses: one instanced component per mesh / material / collision,
 *  filled unregistered and registered once the block is complete. */
struct FDBRealmSiteBatch
{
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;
	TUniquePtr<FDBArtBatcher> Batcher;
	int32 Houses = 0;
	~FDBRealmSiteBatch();
};

USTRUCT()
struct FDBRealmSiteActors
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Actors;

	/** Houses still waiting to be built (a few per frame); their pieces go into Batch. */
	TArray<TWeakObjectPtr<ADBModularBuilding>> PendingHouses;
	TSharedPtr<FDBRealmSiteBatch> Batch;
	/** Owner and attach parent of the shared batches. */
	TWeakObjectPtr<USceneComponent> BatchRoot;
	int32 BatchComponents = 0;
	int32 BatchInstances = 0;

	bool bBuilt = false;
};

UCLASS()
class DARKBLOOD_API ADBRealmDirector : public AInfo
{
	GENERATED_BODY()

public:
	ADBRealmDirector();

	static ADBRealmDirector* Get(const UWorld* World);

	virtual void Tick(float DeltaSeconds) override;

	/** Realm frame (meters, see DBRealmLayout) of a world location; the realm is centered on the world origin. */
	static FVector2D ToRealm(const FVector& WorldLocation) { return FVector2D(WorldLocation.X / 100.0, WorldLocation.Y / 100.0); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdatePlayerRegions();
	/** Builds settlements near the local camera and removes far ones (every machine, deterministic). */
	void StreamSettlements();
	void BuildSettlement(int32 Index);
	void ClearSettlement(int32 Index);
	/** Builds waiting houses within a small time budget per frame, so approaching a city never freezes the game. */
	void BuildPendingHouses();
	/** Registers the current house block of a settlement (it becomes visible and collidable). */
	static void FinishBatch(FDBRealmSiteActors& Site);

	UPROPERTY(Transient)
	TArray<FDBRealmSiteActors> Sites;

	/** Region fog, sun and grading under the local camera. */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|World")
	TObjectPtr<UDBRealmMoodComponent> Mood;

	/** Server: villagers and demon attacks of settlements near players. */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|World")
	TObjectPtr<UDBSettlementLifeComponent> Life;

public:
	UDBSettlementLifeComponent* GetSettlementLife() const { return Life; }

	float RegionTimer = 0.f;
	float StreamTimer = 0.f;
};
