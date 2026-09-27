// Marks the extent of a region in the open world. Always loaded (not spatially streamed) so the
// server knows every player's region. Overlaps are evaluated on the server only.
#pragma once

#include "GameFramework/Volume.h"

#include "DBRegionVolume.generated.h"

class UDBRegionDefinition;

UCLASS()
class DARKBLOOD_API ADBRegionVolume : public AVolume
{
	GENERATED_BODY()

public:
	ADBRegionVolume();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Region")
	TObjectPtr<UDBRegionDefinition> Region;

	/** Higher priority wins when volumes overlap (e.g. the capital inside a larger province). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Region")
	int32 Priority = 0;
};
