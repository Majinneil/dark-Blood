// Modular building kit (DBBuildingKit) - follows Schemas/building_module.schema.json of the visual pack.
// A kit lists authored modules per category; ADBModularBuilding picks modules by region, wealth and seed.
// Categories without a module are built from primitives (greybox kit) at the same grid positions, so an
// imported Fab/Blender kit replaces the greybox piece by piece without moving any gameplay volume.
#pragma once

#include "Art/DBArtMaterials.h"
#include "Engine/DataAsset.h"

#include "DBBuildingKitDefinition.generated.h"

class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class EDBBuildingModuleCategory : uint8
{
	Foundation,
	Wall,
	Pillar,
	Door,
	Window,
	Roof,
	Veranda,
	Stair,
	Fence,
	Gate,
	Bridge,
	PropSocket,
};

USTRUCT(BlueprintType)
struct FDBBuildingModule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	FName ModuleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	EDBBuildingModuleCategory Category = EDBBuildingModuleCategory::Wall;

	/** Grid size the mesh is authored for (pivot: bottom center of the module, facing +X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module", meta = (ClampMin = 0.25, Units = "m"))
	float ModuleSizeMeters = 2.5f;

	/** Region/biome tags (Capital, Village, Temple, DarkBlood ...); empty = everywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TArray<FName> RegionTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module", meta = (ClampMin = 0, ClampMax = 1))
	float WealthMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module", meta = (ClampMin = 0, ClampMax = 1))
	float WealthMax = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TArray<FName> DamageVariants;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TArray<FName> MaterialVariants;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Optional material; empty keeps the mesh's own materials. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Module")
	TSoftObjectPtr<UMaterialInterface> Material;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBBuildingKitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Deterministic pick among the modules that fit; null when the category has none (primitive fallback). */
	const FDBBuildingModule* PickModule(EDBBuildingModuleCategory Category, FName Region, float Wealth, int32 Seed) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kit")
	FName KitId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kit", meta = (TitleProperty = "ModuleId"))
	TArray<FDBBuildingModule> Modules;
};
