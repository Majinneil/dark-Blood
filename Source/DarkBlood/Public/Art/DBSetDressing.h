// Procedural set dressing of the visual slice: torii / gates, lanterns, spline paths / fences / walls / streams,
// ground patches, bridges, a dungeon entrance and rule-based scatter volumes (PCG-style: slope, roads,
// settlements, exclusion, density falloff). All pieces are instanced primitives until authored meshes are
// assigned (scatter entries take meshes directly). Purely visual except where noted (walls/fences collide).
#pragma once

#include "Art/DBArtMaterials.h"
#include "GameFramework/Actor.h"

#include "DBSetDressing.generated.h"

class FDBArtBatcher;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class USplineComponent;
class UStaticMesh;

/** Base: rebuilds its instanced pieces on construction (editor and runtime spawn). */
UCLASS(Abstract)
class DARKBLOOD_API ADBArtActor : public AActor
{
	GENERATED_BODY()

public:
	ADBArtActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	void Rebuild();
	int32 GetInstanceCount() const { return InstanceCount; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	int32 Seed = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	bool bLightsCastShadows = false;

protected:
	virtual void Build(FDBArtBatcher& Batcher) {}
	void AddLight(const FVector& LocalPosition, float Lumens, float Radius, const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Art")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	int32 InstanceCount = 0;
};

UENUM(BlueprintType)
enum class EDBGateStyle : uint8
{
	/** Shrine gate - only at sacred places (Visual Bible). */
	Torii,
	/** Roofed wooden gate (castle / town gates). */
	RoofedGate,
};

UCLASS()
class DARKBLOOD_API ADBGate : public ADBArtActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	EDBGateStyle Style = EDBGateStyle::Torii;

	/** Clear width between the posts (the gate faces +X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float Width = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float Height = 480.f;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;
};

UENUM(BlueprintType)
enum class EDBLanternStyle : uint8
{
	/** Stone toro (shrines, gardens). */
	Stone,
	/** Wooden post with a paper lantern (streets, villages). */
	WoodPost,
};

UCLASS()
class DARKBLOOD_API ADBLantern : public ADBArtActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	EDBLanternStyle Style = EDBLanternStyle::Stone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	bool bLit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	EDBArtMaterial StoneMaterial = EDBArtMaterial::StoneMossy;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;
};

UENUM(BlueprintType)
enum class EDBSplineDressing : uint8
{
	/** Stepping stones (visual only). */
	StonePath,
	/** Packed earth road with stones along the edges (visual only). */
	Road,
	WoodFence,
	BambooFence,
	/** Plaster wall with a tiled cap (tsuiji). */
	CastleWall,
	/** Water ribbon with bank stones (visual only). */
	Stream,
};

UCLASS()
class DARKBLOOD_API ADBSplineDressing : public ADBArtActor
{
	GENERATED_BODY()

public:
	ADBSplineDressing();

	USplineComponent* GetSpline() const { return Spline; }

	/** Distance from a world location to the spline (roads / paths repel vegetation). */
	float DistanceTo(const FVector& WorldLocation) const;
	bool IsPathLike() const { return Type == EDBSplineDressing::StonePath || Type == EDBSplineDressing::Road || Type == EDBSplineDressing::Stream; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	EDBSplineDressing Type = EDBSplineDressing::StonePath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float Width = 300.f;

	/** Streams: the liquid (water, or the glowing blood river of the demon lands). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	EDBArtMaterial Liquid = EDBArtMaterial::Water;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Art")
	TObjectPtr<USplineComponent> Spline;
};

UCLASS()
class DARKBLOOD_API ADBGroundPatch : public ADBArtActor
{
	GENERATED_BODY()

public:
	/** Visual ground layer just above the blockout floor (collision only when bCollision is set). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	FVector2D Size = FVector2D(2000.f, 2000.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	EDBArtMaterial Material = EDBArtMaterial::GroundEarth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	bool bRound = false;

	/** Layering above the floor (1-4 cm) so patches can overlap without z-fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float Layer = 1.f;

	/** Base layers can carry collision at floor height (e.g. beyond the blockout floor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	bool bCollision = false;

	/** Paving: split into stone slabs with joints. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float TileSize = 0.f;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;
};

UCLASS()
class DARKBLOOD_API ADBBridge : public ADBArtActor
{
	GENERATED_BODY()

public:
	/** Spans along +X, walkable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float Length = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float Width = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float ArchHeight = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	bool bLacquered = true;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;
};

UCLASS()
class DARKBLOOD_API ADBDungeonEntrance : public ADBArtActor
{
	GENERATED_BODY()

public:
	/** Corruption around the entrance (0 = old ruin, 1 = Dark Blood nest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (ClampMin = 0, ClampMax = 1))
	float Corruption = 0.7f;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;
};

/** Authored model (e.g. CC BY Fab props), scaled to a target height and set on the ground. Multi-part models share
 *  one scene space, so all parts get the same transform. Falls back to nothing when the meshes are not imported. */
UCLASS()
class DARKBLOOD_API ADBPropActor : public ADBArtActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	TArray<TSoftObjectPtr<UStaticMesh>> Parts;

	/** Height of the combined parts after scaling (0 = keep the authored size). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (Units = "cm"))
	float TargetHeight = 0.f;

	/** Extra rotation of the model inside the actor (authoring axes differ between sources). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	FRotator ModelRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	bool bCollision = true;

	/** Place the model as authored around the actor origin (no scaling, no grounding) - e.g. the horizon terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	bool bKeepPivot = false;

	/** Replace every material of the model (Count = keep the authored materials). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	EDBArtMaterial MaterialOverride = EDBArtMaterial::Count;

	/** Slots whose name contains the key get this material instead (checked before MaterialOverride). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	TMap<FString, EDBArtMaterial> SlotMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	FLinearColor LightColor = FLinearColor(1.f, 0.6f, 0.3f);

	/** Optional light (e.g. a lantern flame) at this local offset above the ground after scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	float LightLumens = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	FVector LightOffset = FVector::ZeroVector;

	bool HasModel() const { return bHasModel; }

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;

private:
	bool bHasModel = false;
};

UENUM(BlueprintType)
enum class EDBAmbientFx : uint8
{
	/** Pink petals drifting down around cherry trees. */
	CherryPetals,
	/** Sparks rising from fires and forges. */
	Embers,
	/** Slow blinking lights over meadows and streams (dusk / night only). */
	Fireflies,
	/** Red-glowing ash drifting through corrupted land. */
	DemonAsh,
};

/**
 * Ambient particles without a particle system: a few hundred instanced pieces (tiny planes / spheres) moved on the
 * CPU each frame - falling, rising, wandering, blinking. Only animates while the local camera is near; never
 * replicated and never simulated on a dedicated server.
 */
UCLASS()
class DARKBLOOD_API ADBAmbientFx : public ADBArtActor
{
	GENERATED_BODY()

public:
	ADBAmbientFx();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	EDBAmbientFx Kind = EDBAmbientFx::CherryPetals;

	/** Half size of the box the particles live in (Z = half height, the box sits on the actor origin). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	FVector Extent = FVector(800.f, 800.f, 400.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art", meta = (ClampMin = 1, ClampMax = 2000))
	int32 Count = 250;

	/** Only visible at dusk and night (fireflies). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Art")
	bool bNightOnly = false;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;

private:
	struct FParticle
	{
		FVector Base;
		float Phase;
		float Speed;
		float Sway;
		float SwayRate;
		float Size;
	};

	TArray<FParticle> Particles;
	TArray<FTransform> Transforms;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Swarm = nullptr;

	float Time = 0.f;
};

UENUM(BlueprintType)
enum class EDBBiome : uint8
{
	TemperateForest,
	CherryGrove,
	MountainForest,
	WetForest,
	Bamboo,
	Roadside,
	ShrineGarden,
	Corrupted,
	/** Open grassland: grass and a few shrubs and rocks, no trees. */
	Meadow,
};

USTRUCT(BlueprintType)
struct FDBScatterEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter", meta = (ClampMin = 0))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter")
	FVector2D ScaleRange = FVector2D(0.8f, 1.2f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter")
	bool bAlignToSlope = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scatter")
	bool bCollision = false;
};

UCLASS()
class DARKBLOOD_API ADBScatterVolume : public ADBArtActor
{
	GENERATED_BODY()

public:
	ADBScatterVolume();

	UBoxComponent* GetArea() const { return Area; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter")
	EDBBiome Biome = EDBBiome::TemperateForest;

	/** Large plants (trees, bamboo clumps) per 100 m2 at the volume center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter", meta = (ClampMin = 0))
	float Density = 4.f;

	/** Undergrowth / rocks per 100 m2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter", meta = (ClampMin = 0))
	float UndergrowthDensity = 10.f;

	/** Grass clumps per m2 (imported CC0 grass; none without it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter", meta = (ClampMin = 0))
	float GrassDensity = 0.f;

	/** Density fades to zero over this share of the half extent (soft forest edges). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter", meta = (ClampMin = 0, ClampMax = 1))
	float EdgeFalloff = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter", meta = (Units = "deg"))
	float MaxSlopeDegrees = 30.f;

	/** Keep-out distances: roads/paths, buildings and landmarks (gates, lanterns, bridges, dungeon). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter", meta = (Units = "cm"))
	float RoadClearance = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter", meta = (Units = "cm"))
	float BuildingClearance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter", meta = (Units = "cm"))
	float GameplayClearance = 500.f;

	/** Authored meshes (CC0 trees, Fab vegetation). Empty = DEV primitive plants for the biome. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter")
	TArray<FDBScatterEntry> Plants;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Scatter")
	TArray<FDBScatterEntry> Undergrowth;

	int32 GetPlacedCount() const { return PlacedCount; }
	int32 GetRejectedCount() const { return RejectedCount; }

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Scatter")
	TObjectPtr<UBoxComponent> Area;

private:
	bool IsExcluded(const FVector& World, float Clearance) const;
	void PlaceDevPlant(FDBArtBatcher& Batcher, const FVector& Local, float Scale, FRandomStream& Random) const;
	void PlaceDevUndergrowth(FDBArtBatcher& Batcher, const FVector& Local, float Scale, FRandomStream& Random) const;

	int32 PlacedCount = 0;
	int32 RejectedCount = 0;
};
