// Procedural modular building (Building Assembly Layer, 11_PCGBUILDING_BRIEF / 14_BUILDING_ART_RECIPE):
// stone plinth, wooden post-and-beam frame, plaster / shoji / plank / window / door bays, sloped tile or thatch
// roofs with ridge and eave details, verandas, stairs, lanterns and per-type dressing (tavern interior, forge,
// shrine rope ...). Everything is derived from a few parameters and a seed, so one kit yields many buildings.
// Local frame: +X is the entrance side, the actor sits on the ground at the footprint center.
// Doors stay open for players/NPCs; kit meshes (UDBBuildingKitDefinition) replace primitives per category.
#pragma once

#include "Art/DBArtMaterials.h"
#include "GameFramework/Actor.h"

#include "DBModularBuilding.generated.h"

class FDBArtBatcher;
class UDBBuildingKitDefinition;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
struct FRandomStream;

UENUM(BlueprintType)
enum class EDBBuildingType : uint8
{
	SmallHouse,
	LargeHouse,
	MerchantHouse,
	Tavern,
	Smithy,
	Warehouse,
	Guardhouse,
	TempleHall,
	Shrine,
	VillageHall,
};

UENUM(BlueprintType)
enum class EDBRoofFamily : uint8
{
	Gable,
	Hip,
	/** Hip roof with an additional skirt roof (temples, palace halls). */
	Tiered,
};

UCLASS()
class DARKBLOOD_API ADBModularBuilding : public AActor
{
	GENERATED_BODY()

public:
	ADBModularBuilding();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Code setup before FinishSpawning (visual slice); applies the type preset. */
	void Configure(EDBBuildingType InType, int32 InSeed, FName InRegion, float InWealth, int32 InModulesX, int32 InModulesY, int32 InFloors = 1,
		float InDamage = 0.f);

	void Rebuild();

	/** Open world: new buildings wait for Rebuild (built a few per frame instead of all in one hitch)... */
	static void SetDeferRebuild(bool bDefer) { bDeferRebuild = bDefer; }
	/** ...and put their pieces into a settlement-wide batch (few primitives for a whole city) while one is set. */
	static void SetSharedBatcher(FDBArtBatcher* Batcher) { SharedBatcher = Batcher; }

	/** World-space bounds incl. roof overhang (vegetation / scatter exclusion). */
	FBox GetFootprintBounds() const;
	int32 GetInstanceCount() const { return InstanceCount; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building")
	EDBBuildingType BuildingType = EDBBuildingType::SmallHouse;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building")
	int32 Seed = 1;

	/** Region style: Capital, Village, Temple, DarkBlood (and later region ids). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building")
	FName Region = TEXT("Village");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building", meta = (ClampMin = 0, ClampMax = 1))
	float Wealth = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building", meta = (ClampMin = 0, ClampMax = 1))
	float Damage = 0.f;

	/** Bays along the depth (X) and the facade (Y). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building", meta = (ClampMin = 1, ClampMax = 8))
	int32 ModulesX = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building", meta = (ClampMin = 1, ClampMax = 10))
	int32 ModulesY = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building", meta = (ClampMin = 1, ClampMax = 3))
	int32 Floors = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building", meta = (ClampMin = 150, Units = "cm"))
	float ModuleSize = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building")
	EDBRoofFamily RoofFamily = EDBRoofFamily::Gable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building")
	bool bVeranda = false;

	/** 0 = none, 0.5 = entrance, 1 = every front bay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building", meta = (ClampMin = 0, ClampMax = 1))
	float LanternDensity = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building")
	bool bLanternsCastShadows = false;

	/** Optional authored kit; categories without modules keep the primitive greybox pieces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Building")
	TObjectPtr<UDBBuildingKitDefinition> Kit = nullptr;

private:
	struct FPalette;

	void Clear();
	FPalette MakePalette(FRandomStream& Random) const;
	void BuildFloor(FDBArtBatcher& Batcher, FRandomStream& Random, const FPalette& Palette, int32 Floor, float BaseZ);
	void BuildRoof(FDBArtBatcher& Batcher, FRandomStream& Random, const FPalette& Palette, float WallTopZ);
	void BuildSkirtRoof(FDBArtBatcher& Batcher, const FPalette& Palette, float Z, float Overhang, float PitchDegrees);
	void BuildDressing(FDBArtBatcher& Batcher, FRandomStream& Random, const FPalette& Palette, float FloorZ);
	void AddLight(const FVector& LocalPosition, float Lumens, float Radius, const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Building")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pieces;

	static bool bDeferRebuild;
	static FDBArtBatcher* SharedBatcher;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	int32 InstanceCount = 0;
};
