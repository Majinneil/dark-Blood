// Data asset for one of the 16 main areas (capital, 14 vassal regions, DAS ENDE) plus the epilogue.
#pragma once

#include "Engine/DataAsset.h"
#include "Core/DBTypes.h"

#include "DBRegionDefinition.generated.h"

class USoundBase;

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBRegionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
	FName RegionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
	FText DisplayName;

	/** Theme, e.g. "Blut", "Frost", "Schatten" ... */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
	FText Theme;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
	EDBRegionKind Kind = EDBRegionKind::VassalRegion;

	/** Recommended power ("Empfohlene Staerke"). Informational only - regions are never locked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Danger", meta = (ClampMin = 1))
	int32 RecommendedPowerMin = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Danger", meta = (ClampMin = 1))
	int32 RecommendedPowerMax = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bosses")
	FName VassalBossId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bosses")
	FName MidBossId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<USoundBase> AmbientMusic;
};
