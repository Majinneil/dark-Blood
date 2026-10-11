// Footsteps (Phase 18): every character near the listener steps audibly - grass, stone, snow, earth or wood, read
// from what it stands on (the realm's paint layers on the landscape, wood on ships, stone elsewhere). Cadence follows
// the speed; demons tread heavier and deeper. Local presentation only, ticks only on machines with audio.
#pragma once

#include "Components/ActorComponent.h"

#include "DBFootstepComponent.generated.h"

UCLASS(ClassGroup = (DarkBlood), meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Footstep sound for a spot of the realm's landscape ("Footsteps/Grass" ...) and its pitch. */
	static FName GetLandscapeStep(const FVector& Location, float& OutPitch);

	int32 GetStepCount() const { return Steps; }

protected:
	virtual void BeginPlay() override;

private:
	FVector LastLocation = FVector::ZeroVector;
	float Travelled = 0.f;
	int32 Steps = 0;
	bool bLastOnGround = true;
};
