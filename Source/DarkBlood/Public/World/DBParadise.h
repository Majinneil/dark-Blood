// DAS PARADIES and the finale (Phase 15, docs/PARADISE.md). When the demon king falls the world is purified and a gate
// of light opens at his throne. It leads to the Paradise, a floating island high above DAS ENDE (the floating islands of
// the world map): cherry trees, a pond, a pagoda and the Shrine of Peace. Resting at the shrine ends the story (finale,
// credits); a gate brings the players back to the capital, the world goes on.
#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/DBInteractable.h"
#include "World/DBDungeon.h"

#include "DBParadise.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace DBParadise
{
	/** Center of the island's top (world cm), high above DAS ENDE. */
	DARKBLOOD_API FVector GetIslandCenter();
	DARKBLOOD_API bool IsInParadise(const FVector& WorldLocation);
	/** Server: island, both gates and the shrine (idempotent). */
	DARKBLOOD_API void SpawnParadise(UWorld* World);
}

UENUM()
enum class EDBParadiseGate : uint8
{
	/** At the throne: opens when the demon king has fallen, leads up to the Paradise. */
	ToParadise,
	/** On the island: back down to the capital. */
	Home,
};

UCLASS()
class DARKBLOOD_API ADBParadiseGate : public AActor, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBParadiseGate();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override { return User != nullptr && bActive; }
	/** A wide gate: usable anywhere in front of it. */
	virtual float GetInteractionRange() const override { return 520.f; }
	virtual void Interact(APlayerController* User) override;

	void Setup(EDBParadiseGate InKind);
	bool IsActive() const { return bActive; }
	EDBParadiseGate GetKind() const { return Kind; }
	static ADBParadiseGate* Find(const UWorld* World, EDBParadiseGate Kind);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Gate();
	void Build();

	UPROPERTY(ReplicatedUsing = OnRep_Gate)
	EDBParadiseGate Kind = EDBParadiseGate::ToParadise;

	UPROPERTY(ReplicatedUsing = OnRep_Gate)
	bool bActive = false;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Paradise")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Paradise")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Paradise")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Light;

	float UpdateTimer = 0.f;
	bool bBuilt = false;
};

/** The island itself (built on every machine, static). */
UCLASS()
class DARKBLOOD_API ADBParadiseIsland : public AActor
{
	GENERATED_BODY()

public:
	ADBParadiseIsland();

protected:
	virtual void BeginPlay() override;

private:
	void Build();

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Paradise")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Paradise")
	TObjectPtr<UPointLightComponent> ShrineLight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;
};

/** The Shrine of Peace: rest and see the end of the story. */
UCLASS()
class DARKBLOOD_API ADBPeaceShrine : public ADBDungeonShrine
{
	GENERATED_BODY()

public:
	virtual FText GetInteractionText() const override;
	virtual void Interact(APlayerController* User) override;
};
