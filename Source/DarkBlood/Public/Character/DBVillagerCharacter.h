// Resident of a simulated settlement (Phase 7). Spawned by the server around players (UDBSettlementLifeComponent), wanders
// through its settlement and, when spoken to, tells what the abstract simulation says about the place: hunger, attacks,
// guards, prosperity, liberation. A presentation of the settlement state, not a source of numbers.
#pragma once

#include "Character/DBNpcCharacter.h"

#include "DBVillagerCharacter.generated.h"

UCLASS()
class DARKBLOOD_API ADBVillagerCharacter : public ADBNpcCharacter
{
	GENERATED_BODY()

public:
	ADBVillagerCharacter(const FObjectInitializer& ObjectInitializer);

	/** Server, before use: the settlement, a name and the area to wander in (cm). */
	void SetupVillager(FName InSettlementId, const FText& InName, const FVector& InHome, float InWanderRadius, int32 InSeed);

	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact(APlayerController* User) override;

	FName GetSettlementId() const { return SettlementId; }

	/** What this villager says right now (server). */
	FText MakeRemark() const;

private:
	void PickWanderTarget();

	FName SettlementId;
	FVector Home = FVector::ZeroVector;
	float WanderRadius = 1500.f;
	FVector WanderTarget = FVector::ZeroVector;
	float IdleSeconds = 0.f;
	float WalkSeconds = 0.f;
	FRandomStream Random;
	int32 RemarkIndex = 0;
};
