// DAS ENDE (Phase 13, docs/THE_END.md). The way into the demon king's land: the Gate of the End (three great corrupted
// torii, a blood seal in the middle one until the 14 vassals outside have fallen; a story gate, nothing blocks the way),
// the Last Bastion outside it (rest, respawn point, the humans' last camp) and the Path of Shame (corrupted torii from
// the gate to the throne). The gate starts the main quests of DAS ENDE when its seal breaks.
#pragma once

#include "GameFramework/Actor.h"
#include "World/DBDungeon.h"

#include "DBTheEnd.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace DBTheEnd
{
	/** World cm. Gate: inside DAS ENDE on the side facing the realm; bastion: outside, in front of the gate. */
	DARKBLOOD_API FVector GetGateLocation();
	DARKBLOOD_API FVector GetBastionLocation();
	/** Yaw (deg) looking from the gate into DAS ENDE. */
	DARKBLOOD_API float GetGateYaw();
	/** Respawn point tag of the bastion (APlayerStart spawned at runtime). */
	DARKBLOOD_API FName GetBastionRespawnId();

	/** Server: gate, bastion and path (idempotent). */
	DARKBLOOD_API void SpawnLandmarks(UWorld* World);
}

UCLASS()
class DARKBLOOD_API ADBEndGate : public AActor
{
	GENERATED_BODY()

public:
	ADBEndGate();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsOpen() const { return bOpen; }
	static ADBEndGate* Find(const UWorld* World);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Gate();
	void Build();
	/** Server: starts the next quest of DAS ENDE once its predecessor is done. */
	void AdvanceQuests();

	UPROPERTY(ReplicatedUsing = OnRep_Gate)
	bool bOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_Gate)
	int32 OuterVassalsDefeated = 0;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|The End")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|The End")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Seal;

	float UpdateTimer = 0.f;
	bool bBuilt = false;
	bool bKnownState = false;
};

/** The Last Bastion: rest (like a dungeon shrine) and make it your respawn point. */
UCLASS()
class DARKBLOOD_API ADBBastionShrine : public ADBDungeonShrine
{
	GENERATED_BODY()

public:
	ADBBastionShrine();

	virtual FText GetInteractionText() const override;
	virtual void Interact(APlayerController* User) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void BuildCamp();

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|The End")
	TObjectPtr<UPointLightComponent> FireLight;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|The End")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CampParts;
};
