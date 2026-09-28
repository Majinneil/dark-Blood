// Placed once in L_Realm (DBBuildRealmCommandlet). Server: assigns each player the region of the realm layout they
// stand in (DBRealmLayout; region volumes still win where placed). Every machine: builds the settlements of the realm
// locally and deterministically (like the visual slice), so co-op costs no bandwidth.
#pragma once

#include "GameFramework/Info.h"

#include "DBRealmDirector.generated.h"

USTRUCT()
struct FDBRealmSiteActors
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Actors;

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

	UPROPERTY(Transient)
	TArray<FDBRealmSiteActors> Sites;

	float RegionTimer = 0.f;
	float StreamTimer = 0.f;
};
