// Riding horse (Phase 8). Interact to mount / dismount. Like the ships, the rider stays itself (camera, abilities, saves)
// and is attached to the saddle; its movement input steers the horse on the server (relative to the camera), Sprint
// gallops while the horse has stamina. Each player owns one horse and calls it with [H]. Swims like every character.
// Placeholder body from basic shapes until a horse model is imported.
#pragma once

#include "GameFramework/Character.h"
#include "Interaction/DBInteractable.h"

#include "DBHorse.generated.h"

class APlayerState;
class UStaticMeshComponent;

UCLASS()
class DARKBLOOD_API ADBHorse : public ACharacter, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBHorse(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IDBInteractable
	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override;
	virtual void Interact(APlayerController* User) override;
	virtual float GetInteractionRange() const override { return 450.f; }

	/** Server: steering of the rider (X right, Y forward, relative to CameraYaw). */
	void SetSteering(const FVector2D& Input, float CameraYaw);
	/** Server: gallop while the horse has stamina. */
	void SetGallop(bool bInGallop);

	APawn* GetRider() const { return Rider; }
	APlayerState* GetOwnerState() const { return OwnerState; }
	float GetHorseStamina() const { return HorseStamina; }
	bool IsGalloping() const { return bGallop && HorseStamina > 0.f; }

	static ADBHorse* FindRiddenBy(const APawn* Pawn);
	static ADBHorse* FindOwnedBy(const APlayerState* PlayerState);
	/** Server: brings the player's horse (spawns it the first time) a few meters behind the player. */
	static ADBHorse* CallHorse(APawn* Player);

	/** Trot and gallop speeds (cm/s); horse stamina use / regeneration per second. */
	static constexpr float TrotSpeed = 900.f;
	static constexpr float GallopSpeed = 1500.f;
	static constexpr float MaxHorseStamina = 100.f;

private:
	void Mount(APawn* Pawn);
	void Dismount();

	UPROPERTY(Replicated)
	TObjectPtr<APawn> Rider;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> OwnerState;

	UPROPERTY(Replicated)
	float HorseStamina = MaxHorseStamina;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Horse")
	TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	FVector2D Steering = FVector2D::ZeroVector;
	float SteeringYaw = 0.f;
	bool bGallop = false;
};
