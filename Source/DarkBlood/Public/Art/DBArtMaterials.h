// Named material slots of the DARK BLOOD art kit (docs/VISUAL_FOUNDATION.md).
// Each slot maps to a material instance created by Tools/UE58/db_create_material_foundation.py
// (/Game/DarkBlood/Art/Materials/...). Missing instances fall back to a flat-colored engine material, so the
// procedural kit always renders - swapping textures on the instances restyles the whole world at once.
#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "DBArtMaterials.generated.h"

class UMaterialInterface;

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

private:
	UMaterialInterface* Resolve(EDBArtMaterial Slot);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> Cache;

	TArray<bool> Authored;
};
