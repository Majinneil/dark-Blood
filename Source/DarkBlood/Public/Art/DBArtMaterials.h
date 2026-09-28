// Named material slots of the DARK BLOOD art kit (docs/VISUAL_FOUNDATION.md).
// Each slot maps to a material instance created by Tools/UE58/db_create_material_foundation.py
// (/Game/DarkBlood/Art/Materials/...). Missing instances fall back to a flat-colored engine material, so the
// procedural kit always renders - swapping textures on the instances restyles the whole world at once.
#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "DBArtMaterials.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** Groups of authored meshes (CC0 Poly Haven imports, later Fab/Megascans) used by scatter and dressing. */
UENUM(BlueprintType)
enum class EDBArtMeshSet : uint8
{
	Tree,
	DeadTree,
	Shrub,
	Fern,
	Moss,
	Rock,
	Boulder,
	Stump,
	/** Blossoming cherry: island tree geometry with the sakura leaf material. */
	SakuraTree,
	Conifer,
	/** Grass clumps (Nanite) for meadows and forest floors. */
	Grass,
	/** Long cliff lines and cliff blocks (plateaus, coast, castle rock). */
	Cliff,
	RockFace,
	Count UMETA(Hidden)
};

USTRUCT()
struct FDBArtMeshList
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> Meshes;
};

UENUM(BlueprintType)
enum class EDBArtMaterial : uint8
{
	WoodDark,
	WoodLight,
	WoodWet,
	WoodBurnt,
	WoodLacquerRed,
	WoodLacquerBlack,
	StoneDry,
	StoneWet,
	StoneMossy,
	StoneTemple,
	StoneMountain,
	StoneRuin,
	StoneCorrupted,
	PlasterLime,
	PlasterClay,
	PaperShoji,
	PaperShojiLit,
	RoofTile,
	RoofThatch,
	RoofCopper,
	MetalIron,
	MetalBronze,
	FabricLinen,
	FabricCrimson,
	FabricIndigo,
	LanternPaper,
	LanternFire,
	Water,
	GroundEarth,
	GroundForest,
	GroundCourtyard,
	FoliageLeaves,
	FoliageSakura,
	FoliageDead,
	DarkBloodVeins,
	DarkBloodSoil,
	DarkBloodStone,
	BarkCedar,
	BarkSakura,
	Bamboo,
	FoliageSakuraLeaves,
	TerrainMountain,
	TerrainHills,
	TerrainCliff,
	GroundMeadow,
	GroundShore,
	TerrainSnow,
	/** Glowing blood-red water of the demon lands. */
	BloodRiver,
	/** Dark red, faintly glowing leaf cards (the demon tree). */
	FoliageDemonLeaves,
	Void,
	Count UMETA(Hidden)
};

UCLASS()
class DARKBLOOD_API UDBArtMaterialSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** Material of a slot (cached; never null while the engine runs). */
	static UMaterialInterface* Get(EDBArtMaterial Slot);

	/** Asset path of the instance behind a slot. */
	static FString GetAssetPath(EDBArtMaterial Slot);

	/** True when the slot resolved to its authored instance instead of the flat fallback. */
	static bool IsAuthored(EDBArtMaterial Slot);

	/** Imported meshes of a set (empty when not imported: callers fall back to primitives). */
	static const TArray<TObjectPtr<UStaticMesh>>& GetMeshes(EDBArtMeshSet Set);

	/** Deterministic pick from a set, or null. */
	static UStaticMesh* PickMesh(EDBArtMeshSet Set, FRandomStream& Random);

	/** Asset paths behind a set (audit). */
	static TArray<FString> GetMeshPaths(EDBArtMeshSet Set);

private:
	UMaterialInterface* Resolve(EDBArtMaterial Slot);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> Cache;

	TArray<bool> Authored;

	UPROPERTY(Transient)
	TArray<FDBArtMeshList> MeshSets;
};
