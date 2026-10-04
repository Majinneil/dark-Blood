// A sailing ship of the open world. Interact on board to take the helm: the player character stays itself (camera,
// abilities, saves), is attached at the helm, and its movement input steers the ship (W/S sails, A/D rudder). Others
// ride along on the deck. The server moves the ship on the sea (it never sails onto land); clients receive the
// replicated movement. The model comes from the art kit (DBShipArt): war ship, fighting ship, merchant ship or boat.
#pragma once

#include "Art/DBShipArt.h"
#include "GameFramework/Actor.h"
#include "Interaction/DBInteractable.h"

#include "DBShip.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;

UCLASS()
class DARKBLOOD_API ADBShip : public AActor, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBShip();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IDBInteractable
	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override;
	virtual void Interact(APlayerController* User) override;
	virtual float GetInteractionRange() const override;

	/** Server: steering input of the helmsman (X = rudder, Y = sails), each -1..1. */
	void SetSteering(const FVector2D& Input);

	APawn* GetHelmsman() const { return Helmsman; }
	static ADBShip* FindSteeredBy(const APawn* Pawn);

	/** Server: spawns a ship floating at a sea position (world XY, cm). */
	static ADBShip* SpawnAt(UWorld* World, const FVector2D& Location, float Yaw, const FText& Name, EDBShipStyle Style = EDBShipStyle::Merchant);

	/** Ship type (DBShipArt): model, deck size and handling. Set before spawning finishes; replicated once. */
	UPROPERTY(ReplicatedUsing = OnRep_Style, EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Ship")
	EDBShipStyle Style = EDBShipStyle::Merchant;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Ship")
	FText ShipName;

	/** Top speed under full sail (cm/s) and turn rate at speed (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Ship")
	float MaxSpeed = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Ship")
	float TurnRate = 14.f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Style();
	/** Deck size, handling and the model of the current style. */
	void ApplyStyle();
	void TakeHelm(APawn* Pawn);
	void ReleaseHelm();
	/** Sea depth check ahead of the bow (open world layout; always free elsewhere). */
	bool IsWaterAhead(const FVector& Location, const FVector& Forward) const;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Ship")
	TObjectPtr<UBoxComponent> Hull;

	/** Visual model; rolls and pitches with the waves (the deck collision stays level). */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Ship")
	TObjectPtr<USceneComponent> Model;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ModelPieces;

	UPROPERTY(Replicated)
	TObjectPtr<APawn> Helmsman = nullptr;

	FVector2D Steering = FVector2D::ZeroVector;
	float Speed = 0.f;
	/** Walkable deck height above the water line: the authored model's deck, else the kit ship's deck (cm). */
	float DeckAboveWater = 0.f;
	float WaveTime = 0.f;
};
