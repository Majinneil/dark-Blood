// Sound of DARK BLOOD (Phase 18, docs/AUDIO.md). One world subsystem on every machine that plays audio:
// - the sound bank: /Game/DarkBlood/Audio/<Category>/S_<Name>_NN, variations picked at random (never the same twice
//   in a row), slight pitch and volume spread, 3D falloff per kind, a cap on simultaneous voices per sound;
// - the soundscape around the local player: an ambience bed per region and time of day (birds by day, crickets at
//   night, mountain wind, the surf, the drone of the demon lands) crossfading as the player travels;
// - music: exploration themes with long silences between them, battle music while demons hunt the player, the boss
//   theme in a boss fight - always faded, never cut.
// Presentation only: nothing in gameplay waits for a sound. A dedicated server plays nothing.
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"

#include "DBAudioSubsystem.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class USoundMix;
class USoundWave;

/** How a sound carries through the world. */
enum class EDBSoundReach : uint8
{
	/** Steps, cloth, small impacts (15 m). */
	Near,
	/** Blows, blocks, swings (40 m). */
	Combat,
	/** Roars and screams (70 m). */
	Voice,
	/** Booming impacts, a boss falling (120 m). */
	Far,
	Count
};

UENUM()
enum class EDBMusicState : uint8
{
	Silence,
	Explore,
	Battle,
	Boss,
};

UCLASS()
class DARKBLOOD_API UDBAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDBAudioSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** A random variation of Sound ("Combat/Block", "Footsteps/Grass" ...) at a place. */
	static void PlayAt(const UObject* WorldContext, FName Sound, const FVector& Location, EDBSoundReach Reach, float Volume = 1.f, float Pitch = 1.f);
	/** Interface sounds and other sounds without a place. */
	static void Play2D(const UObject* WorldContext, FName Sound, float Volume = 1.f);

	/** A looping sound attached to a component (waterfalls); stopped with the owner. */
	UAudioComponent* PlayLoopAttached(FName Sound, USceneComponent* Parent, const FVector& WorldLocation, float InnerRadius, float FalloffDistance, float Volume = 1.f);

	EDBMusicState GetMusicState() const { return MusicState; }
	FName GetAmbienceBed() const { return AmbienceBed; }
	/** Number of sound groups and waves found (tests, log). */
	int32 GetBankGroupCount() const { return Bank.Num(); }

	/** Pushes the volumes of DBGameUserSettings onto the engine sound classes (Music, SFX, Voice). */
	void ApplyVolumes();

	/** Cheat DBMusic: forces a music state (negative: back to automatic). */
	void ForceMusic(int32 State) { ForcedMusic = State; MusicTimer = 0.f; }

private:
	struct FGroup
	{
		TArray<FSoftObjectPath> Paths;
		TArray<TObjectPtr<USoundWave>> Loaded;
		int32 Last = INDEX_NONE;
	};

	void BuildBank();
	USoundWave* Pick(FName Sound);
	USoundAttenuation* GetAttenuation(EDBSoundReach Reach);
	USoundConcurrency* GetConcurrency(FName Sound);

	void UpdateSoundscape();
	void UpdateMusic(float DeltaTime);
	void SetAmbience(FName Bed, FName Layer);
	void CrossfadeMusic(FName Track);
	UAudioComponent* MakeLoop(FName Sound, float Volume);

	TMap<FName, FGroup> Bank;

	/** Keeps every loaded wave alive for the garbage collector (Bank is a plain map the GC does not see). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundWave>> LoadedWaves;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundAttenuation>> Attenuations;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundConcurrency>> Concurrency;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> VolumeMix;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BedComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LayerComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	FName AmbienceBed;
	FName AmbienceLayer;
	FName MusicTrack;
	EDBMusicState MusicState = EDBMusicState::Silence;
	int32 ForcedMusic = -1;
	float SoundscapeTimer = 0.f;
	float MusicTimer = 0.f;
	/** Exploration music comes and goes: seconds of silence left before it plays again. */
	float ExploreRest = 8.f;
	float ExplorePlayed = 0.f;
	float CombatCalm = 0.f;
	bool bActive = false;
};
