// Waterfalls (Phase 17, docs/WORLD_DESIGN.md): the steepest faces of the mountains, valleys and bamboo hills carry falling
// water - a spring pool on the lip, a sheet of water down the face (M_DB_Waterfall), foam and spray where it lands and
// a plunge pool. The sites come from the terrain itself (DBRealm::SampleHeight): deterministic, the same on every
// machine; each machine builds them locally (static scenery, nothing replicated).
#pragma once

#include "GameFramework/Actor.h"

#include "DBWaterfall.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

struct FDBWaterfallSite
{
	/** Lip and foot of the fall (world, cm). */
	FVector Top = FVector::ZeroVector;
	FVector Bottom = FVector::ZeroVector;
	/** Width of the sheet (cm). */
	float Width = 1200.f;
	/** How far the sheet stands off the face so the terrain never cuts through it (cm). */
	float Lift = 150.f;
	/** The foot is level ground or sea (a pool and foam fit there; on a slope they would stick out of it). */
	bool bLevelFoot = false;
	FName RegionId;
};

namespace DBWaterfalls
{
	/** The waterfalls of the realm (found once, cached). */
	DARKBLOOD_API const TArray<FDBWaterfallSite>& GetSites();
	/** Every machine: builds the waterfalls that are not built yet. */
	DARKBLOOD_API void SpawnAll(UWorld* World);
}

UCLASS()
class DARKBLOOD_API ADBWaterfall : public AActor
{
	GENERATED_BODY()

public:
	ADBWaterfall();

	void Build(const FDBWaterfallSite& Site);
	virtual void Tick(float DeltaSeconds) override;

private:
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& Transform);

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Waterfall")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	/** Spray: splashes keep bursting at the foot while a camera is near. */
	FVector Foot = FVector::ZeroVector;
	float FootRadius = 600.f;
	FRandomStream Random;
};
