// Region mood of the open world: fog, sun and color grading follow the region under the local camera (red ash over
// the fire mountains, cold light on the ice waste, mist in the spirit forest ...) and blend over a few seconds when the
// camera crosses a border. Local on every machine, nothing replicated. Lives on ADBRealmDirector.
#pragma once

#include "Components/ActorComponent.h"

#include "DBRealmMoodComponent.generated.h"

class APostProcessVolume;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;

UCLASS()
class DARKBLOOD_API UDBRealmMoodComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBRealmMoodComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fog, sun and grading multipliers / tints of one region, relative to the level's own settings. */
	struct FMood
	{
		float FogDensity = 1.f;
		FLinearColor FogColor = FLinearColor::White;
		float SunIntensity = 1.f;
		FLinearColor SunColor = FLinearColor::White;
		float Saturation = 1.f;
		FLinearColor Gain = FLinearColor::White;
		float Vignette = 0.4f;
	};

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Apply(const FMood& Mood);

	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> PostProcess;

	float BaseFogDensity = 0.f;
	FLinearColor BaseFogColor = FLinearColor::White;
	float BaseSunIntensity = 0.f;
	FLinearColor BaseSunColor = FLinearColor::White;

	FMood Current;
	FMood Target;
	int32 TargetRegion = INDEX_NONE;
	float RegionTimer = 0.f;
	float SecondsSinceChange = 0.f;
};
