// Vertical Visual Slice (Phase 5.5): capital courtyard, road, village with tavern, forest, stream with bridge,
// dungeon entrance and shrine, plus the lighting base (day / dusk / night / demon night).
// The server spawns one replicated director; every machine then builds the same non-replicated dressing
// locally from the replicated state (deterministic seeds), so co-op costs no bandwidth.
// Fully reversible: DBVisualSlice 0 / -DBNoVisualSlice removes it; gameplay actors are never moved.
#pragma once

#include "GameFramework/Info.h"

#include "DBVisualSliceDirector.generated.h"

class APostProcessVolume;
class AStaticMeshActor;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EDBTimeOfDay : uint8
{
	Day,
	Dusk,
	Night,
	/** Corrupted night: desaturated world, red accents (demon attacks). */
	DemonNight,
};

USTRUCT()
struct FDBVisualSliceState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = true;

	UPROPERTY()
	EDBTimeOfDay TimeOfDay = EDBTimeOfDay::Dusk;

	UPROPERTY()
	FVector_NetQuantize Origin;

	UPROPERTY()
	float Yaw = 0.f;
};

UCLASS()
class DARKBLOOD_API ADBVisualSliceDirector : public AInfo
{
	GENERATED_BODY()

public:
	ADBVisualSliceDirector();

	static ADBVisualSliceDirector* Get(const UWorld* World);

	/** Server: spawns the director (once) at the slice origin. */
	static ADBVisualSliceDirector* SpawnFor(UWorld* World, const FTransform& Origin);

	/** Server. */
	void SetSliceEnabled(bool bEnabled);
	void SetTimeOfDay(EDBTimeOfDay TimeOfDay);

	/** Local statistics of the built slice (instances, components, lights). */
	FString DescribeLocalSlice() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_State();

	void ApplyLocal();
	void BuildDressing();
	void ClearDressing();
	void ApplyLighting();
	/** Volumetric clouds and sky atmosphere when the level has none (local, transient). */
	void EnsureSky();
	/** Pale moon at night, huge blood moon in the demon night, hidden otherwise. */
	void UpdateMoon(const FRotator& LightRotation, bool bVisible, const FLinearColor& Color, float Strength, float Diameter);

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FDBVisualSliceState State;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LocalActors;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> PostProcess = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SkyActors;

	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> Moon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoonMaterial = nullptr;

	bool bBuilt = false;
	bool bSkyChecked = false;
};
