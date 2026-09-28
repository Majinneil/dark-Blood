// Ships of the realm from the procedural art kit (like the houses), after the ship concept sheet: curved plank hulls
// built from stations along the keel, battened junk sails, lanterns and banners.
//   War      - Kriegsschiff: great battleship, crimson sails with a golden crest, dragon head, gun decks, stern castle
//   Fighting - Kampfschiff: fast and agile, black sails with a crimson crest, high ram bow, shield wall
//   Merchant - Handelsschiff: broad hull, canvas sails, cargo on deck, pavilion at the stern
//   Boat     - Kleines Boot: 1-4 people, one small sail, lantern and oar
// Local frame: forward +X, water line at Z = 0.
#pragma once

#include "Art/DBSetDressing.h"

#include "DBShipArt.generated.h"

class FDBArtBatcher;

UENUM(BlueprintType)
enum class EDBShipStyle : uint8
{
	War,
	Fighting,
	Merchant,
	Boat,
};

/** Size and handling of a ship type (cm, cm/s, deg/s). */
struct FDBShipSpec
{
	float Length = 3000.f;
	float Beam = 800.f;
	/** Main deck height above the water line. */
	float DeckZ = 400.f;
	/** Raised stern deck, where the helmsman stands. */
	float SternDeckZ = 540.f;
	float MaxSpeed = 1200.f;
	float TurnRate = 12.f;
	/** Water depth the hull needs (m). */
	float Draft = 1.5f;
	FText DisplayName;
};

namespace DBShipArt
{
	DARKBLOOD_API const FDBShipSpec& GetSpec(EDBShipStyle Style);

	/** Builds the ship's pieces into Batcher (collision, when wanted, on hull and decks only); OutLanterns receives the
	 *  main lanterns (where lights belong). */
	DARKBLOOD_API void Build(FDBArtBatcher& Batcher, EDBShipStyle Style, int32 Seed, bool bCollision = true, TArray<FVector>* OutLanterns = nullptr);
}

/** A moored ship as harbor dressing (static, walkable decks, lit lanterns at night). */
UCLASS()
class DARKBLOOD_API ADBShipModel : public ADBArtActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Ship")
	EDBShipStyle Style = EDBShipStyle::Merchant;

protected:
	virtual void Build(FDBArtBatcher& Batcher) override;
};
