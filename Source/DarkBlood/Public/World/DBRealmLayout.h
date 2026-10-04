// Layout of the open world (L_Realm): the 16 regions of the world map (docs/VisualPack/Reference/DarkBlood_Weltkarte.png)
// with their terrain character, rivers, coast and paint layers. Pure functions of position - used by the realm builder
// (commandlet, editor) and at runtime (region of a position, settlement sites). Deterministic, no assets.
//
// Frame: meters, origin at the realm center, +X = east, +Y = south (the map's orientation), Z = up.
#pragma once

#include "CoreMinimal.h"

enum class EDBRealmBiome : uint8
{
	Capital,
	CherryValley,
	BambooForest,
	MistMountains,
	Coast,
	RiceFields,
	SpiritForest,
	FireMountains,
	IceWaste,
	Desert,
	SkyTemple,
	DemonWaste,
	VassalFortress,
	GreatCity,
	Riverlands,
	TheEnd,
};

/** Paint layers of M_DB_Realm_Landscape, in this order. */
enum class EDBRealmLayer : uint8
{
	Meadow,
	Forest,
	Rock,
	Snow,
	Sand,
	Soil,
	Corrupt,
	Lava,
	Count
};

struct FDBRealmRegion
{
	/** Region id of the game data (Capital, Region01 ... Region14, TheEnd). */
	FName RegionId;
	const TCHAR* DisplayName;
	EDBRealmBiome Biome;
	/** Center in meters. */
	FVector2D Center;
	/** Core radius in meters (influence reaches ~1.8x). */
	float Radius;
};

/** Settlement types of the settlement sheet (docs/VisualPack/Reference/DarkBlood_Siedlungen.png). */
enum class EDBSettlementType : uint8
{
	Capital,
	GreatCity,
	Village,
	MountainVillage,
	HarborTown,
	RiceVillage,
	ForestSettlement,
	MiningTown,
	TempleSettlement,
	BorderOutpost,
	CaravanTown,
	TavernTown,
	FishingVillage,
	SnowSettlement,
	RiverSettlement,
	OasisTown,
};

struct FDBRealmSettlement
{
	const TCHAR* Name;
	EDBSettlementType Type;
	/** Center in meters; the ground is leveled within Radius (DBRealm::SampleHeight). */
	FVector2D Center;
	float Radius;
	/** Height of the leveled ground in meters. */
	double GroundHeight;
	/** Coastal settlements: direction to the sea (unit) and the distance from the center to the shore in meters. */
	FVector2D SeaDirection = FVector2D::ZeroVector;
	double ShoreDistance = 0.0;
};

namespace DBRealm
{
	/** The 16 settlements, one of each type, placed in fitting regions. */
	DARKBLOOD_API const TArray<FDBRealmSettlement>& GetSettlements();

	/** Half the side of the landscape in meters (4064 quads x 4 m / 2). */
	constexpr double HalfSize = 8128.0;
	/** The capital plateau is flat within this radius (the visual slice and the story start stand on it). */
	constexpr double CapitalFlatRadius = 420.0;

	DARKBLOOD_API const TArray<FDBRealmRegion>& GetRegions();
	DARKBLOOD_API const FDBRealmRegion& GetCapital();

	/** Terrain height in meters (sea level = 0; water covers everything below). */
	DARKBLOOD_API double SampleHeight(double X, double Y);

	/** Index into GetRegions() of the region that dominates this position. */
	DARKBLOOD_API int32 FindRegionIndex(double X, double Y);

	/** Paint layer weights (0..255, summing to 255) for a position with the given height and surface normal Z. */
	DARKBLOOD_API void SampleLayers(double X, double Y, double Height, double NormalZ, uint8 OutWeights[static_cast<int32>(EDBRealmLayer::Count)]);

	DARKBLOOD_API const TCHAR* GetLayerName(EDBRealmLayer Layer);

	/** Size of the world map image the layout follows (docs/VisualPack/Reference/DarkBlood_Weltkarte.png). */
	inline const FVector2D MapImageSize(1536.0, 1024.0);

	/** Pixel of the world map image for a position in meters (inverse of the layout's map projection). */
	DARKBLOOD_API FVector2D ToMapPixel(const FVector2D& Meters);
}
