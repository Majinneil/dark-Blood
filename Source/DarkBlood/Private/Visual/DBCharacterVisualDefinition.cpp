#include "Visual/DBCharacterVisualDefinition.h"

const FPrimaryAssetType UDBCharacterVisualDefinition::AssetType(TEXT("DBCharacterVisual"));

FPrimaryAssetId UDBCharacterVisualDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, ProfileId.IsNone() ? GetFName() : ProfileId);
}
