#include "Data/DBRegionDefinition.h"

const FPrimaryAssetType UDBRegionDefinition::AssetType(TEXT("DBRegion"));

FPrimaryAssetId UDBRegionDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, RegionId.IsNone() ? GetFName() : RegionId);
}
