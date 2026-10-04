// Character movement with swimming in the open world's sea. Everything below sea level is water (DBRealm::SampleHeight),
// so instead of water volumes in the level the component moves its capsule into one shared, brushless water volume while
// the water reaches its chest, and computes the immersion against the sea surface. Every machine applies the same rule,
// so client prediction and the server agree. Outside the open world (test maps) nothing changes.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "DBCharacterMovementComponent.generated.h"

class APhysicsVolume;

UCLASS()
class DARKBLOOD_API UDBCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UDBCharacterMovementComponent();

	virtual float ImmersionDepth() const override;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	bool IsInRealm();
	APhysicsVolume* GetSeaVolume();
	bool IsInSeaVolume() const;

	TWeakObjectPtr<APhysicsVolume> SeaVolume;
	double NextRealmCheck = 0.0;
	bool bRealm = false;
};
