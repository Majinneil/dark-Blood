// Japanese ships from the procedural art kit (like the houses): the bezaisen merchant sailer with its great square
// sail of cloth strips, the atakebune war ship with its armored castle, and the kobaya rowing boat. Local frame:
// forward +X, water line at Z = 0, deck of the bezaisen at Z = 380 (matches ADBShip's walkable deck).
#pragma once

#include "Art/DBSetDressing.h"

#include "DBShipArt.generated.h"

class FDBArtBatcher;

UENUM(BlueprintType)
enum class EDBShipStyle : uint8
{
	Bezaisen,
	Atakebune,
	Kobaya,
};

namespace DBShipArt
{
	/** Builds the ship's pieces into Batcher (collision on for hull and deck only). */
	DARKBLOOD_API void Build(FDBArtBatcher& Batcher, EDBShipStyle Style, int32 Seed, bool bCollision = true);
}

/** A moored ship as harbor dressing (static, walkable deck). */
UCLASS()
class DARKBLOOD_API ADBShipModel : public ADBArtActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Ship")
	EDBShipStyle Style = EDBShipStyle::Bezaisen;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;
};
