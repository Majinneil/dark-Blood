// Carriage station of a settlement (Phase 8): paid fast travel to every other settlement of the open world. Interact
// opens the destination list (fare and travel time); the server checks range and Mon, takes the fare and moves the
// player. Alone, the travel time passes; in co-op it does not (the others would jump with it). Spawned by the realm
// director next to every settlement. Placeholder cart until a model is imported.
#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/DBInteractable.h"

#include "DBCarriageStation.generated.h"

class APlayerController;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class DARKBLOOD_API ADBCarriageStation : public AActor, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBCarriageStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IDBInteractable
	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override { return User != nullptr; }
	virtual void Interact(APlayerController* User) override;
	virtual float GetInteractionRange() const override { return 400.f; }

	/** Server, before use: the settlement (index into DBRealm::GetSettlements()). */
	void SetSite(int32 InSiteIndex);
	int32 GetSiteIndex() const { return SiteIndex; }

	/** Server: travel of this player to a destination settlement. False (with a reason) when refused. */
	bool Travel(APlayerController* User, int32 Destination, FText& OutReason);

	static int64 GetFare(int32 From, int32 To);
	static float GetTravelHours(int32 From, int32 To);
	/** Where the station of a settlement stands (world, cm). */
	static FVector GetStationLocation(int32 SiteIndex);
	static ADBCarriageStation* FindStation(const UWorld* World, int32 SiteIndex);

	/** Server: one station per settlement (idempotent). */
	static void SpawnStations(UWorld* World);

private:
	UFUNCTION()
	void OnRep_Site();

	UPROPERTY(ReplicatedUsing = OnRep_Site)
	int32 SiteIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Carriage")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Carriage")
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Carriage")
	TObjectPtr<UTextRenderComponent> Sign;
};
