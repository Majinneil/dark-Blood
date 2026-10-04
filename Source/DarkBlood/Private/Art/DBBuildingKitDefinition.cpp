#include "Art/DBBuildingKitDefinition.h"

const FPrimaryAssetType UDBBuildingKitDefinition::AssetType(TEXT("DBBuildingKit"));

FPrimaryAssetId UDBBuildingKitDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, KitId.IsNone() ? GetFName() : KitId);
}

const FDBBuildingModule* UDBBuildingKitDefinition::PickModule(EDBBuildingModuleCategory Category, FName Region, float Wealth, int32 Seed) const
{
	TArray<const FDBBuildingModule*, TInlineAllocator<8>> Candidates;
	for (const FDBBuildingModule& Module : Modules)
	{
		const bool bRegionFits = Module.RegionTags.Num() == 0 || Module.RegionTags.Contains(Region);
		if (Module.Category == Category && bRegionFits && Wealth >= Module.WealthMin && Wealth <= Module.WealthMax && !Module.Mesh.IsNull())
		{
			Candidates.Add(&Module);
		}
	}
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}
	return Candidates[FMath::Abs(Seed) % Candidates.Num()];
}
