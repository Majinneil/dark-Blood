// Sky of the open world: the sun follows the game clock (DBWorldStateComponent), the blood moon is a second atmosphere
// light, and at dusk the realm takes its signature look (docs/SKY_AND_ATMOSPHERE.md) - a burning red-violet horizon,
// valleys drowned in a dense low fog layer while the peaks stand clear against the sky, warm lanterns against cool
// shadows. Drives the Lumen-lit, fully dynamic light (nothing baked), the two-layer height fog, volumetric fog and
// clouds, sky light and a cinematic post-process (exposure, tonemapper, split toning, local exposure, bloom).
// Region moods (DBRealmMoodComponent) multiply on top of these values. Local on every machine, nothing replicated.
#pragma once

#include "Components/ActorComponent.h"

#include "DBRealmSkyComponent.generated.h"

class APostProcessVolume;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UDBRealmMoodComponent;

UCLASS()
class DARKBLOOD_API UDBRealmSkyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBRealmSkyComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** One key of the day: sun height and light, sky light, fog and how much of the dusk look applies. */
	struct FSkyKey
	{
		float Hour = 12.f;
		float SunElevation = 45.f;   // degrees above the horizon
		float SunLux = 10.f;
		float SunKelvin = 5800.f;
		float SkyLight = 1.f;
		float Fog = 1.f;             // multiplier on the base fog density
		float Dusk = 0.f;            // 0 day / night grading, 1 full dusk grading
		float Moon = 0.f;            // blood moon visibility
	};

	/** The state for an hour (keys interpolated), exposed for tests and the debug log. */
	static FSkyKey Evaluate(float Hour);

	/** Forces an hour regardless of the game clock (cheat DBSky); negative returns to the clock. */
	void SetHourOverride(float Hour) { HourOverride = Hour; NextUpdate = 0.f; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	float GetHour() const;
	void Configure();
	void Apply(float Hour);

	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<USkyLightComponent> SkyLight;
	TWeakObjectPtr<UExponentialHeightFogComponent> Fog;
	TWeakObjectPtr<UVolumetricCloudComponent> Clouds;
	TWeakObjectPtr<USkyAtmosphereComponent> Atmosphere;
	TWeakObjectPtr<UDBRealmMoodComponent> Mood;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Moon;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> PostProcess;

	float HourOverride = -1.f;
	float NextUpdate = 0.f;
	float AppliedHour = -100.f;
};
