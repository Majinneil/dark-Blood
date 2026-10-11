#include "Audio/DBAudioSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioDevice.h"
#include "Boss/DBBoss.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/DBEnemyCharacter.h"
#include "Components/AudioComponent.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/DBGameUserSettings.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "World/DBDungeon.h"
#include "World/DBEchoHall.h"
#include "World/DBParadise.h"
#include "World/DBRealmLayout.h"
#include "World/DBWorldStateComponent.h"

namespace
{
	const TCHAR* AudioRoot = TEXT("/Game/DarkBlood/Audio");

	/** Inner radius (full volume) and falloff distance (cm) of each reach. */
	constexpr float ReachInner[] = {200.f, 400.f, 600.f, 1500.f};
	constexpr float ReachFalloff[] = {1300.f, 3600.f, 6400.f, 10500.f};

	constexpr float BedVolume = 0.8f;
	constexpr float LayerVolume = 0.65f;
	constexpr float AmbienceFade = 3.f;
	constexpr float MusicFade = 2.5f;

	bool IsGreenBiome(EDBRealmBiome Biome)
	{
		switch (Biome)
		{
		case EDBRealmBiome::Capital:
		case EDBRealmBiome::CherryValley:
		case EDBRealmBiome::BambooForest:
		case EDBRealmBiome::RiceFields:
		case EDBRealmBiome::SpiritForest:
		case EDBRealmBiome::GreatCity:
		case EDBRealmBiome::Riverlands:
		case EDBRealmBiome::Coast:
			return true;
		default:
			return false;
		}
	}

	bool IsDemonBiome(EDBRealmBiome Biome)
	{
		return Biome == EDBRealmBiome::DemonWaste || Biome == EDBRealmBiome::TheEnd || Biome == EDBRealmBiome::FireMountains
			|| Biome == EDBRealmBiome::VassalFortress;
	}

	/** Where the local player hears from: the camera, else the pawn. */
	bool GetListener(const UWorld* World, FVector& OutLocation, const APawn*& OutPawn)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		OutPawn = Controller ? Controller->GetPawn() : nullptr;
		if (Controller && Controller->PlayerCameraManager)
		{
			OutLocation = Controller->PlayerCameraManager->GetCameraLocation();
			return true;
		}
		if (OutPawn)
		{
			OutLocation = OutPawn->GetActorLocation();
			return true;
		}
		return false;
	}
}

UDBAudioSubsystem* UDBAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDBAudioSubsystem>() : nullptr;
}

bool UDBAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && !IsRunningDedicatedServer();
}

void UDBAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildBank();
	bActive = true;
}

void UDBAudioSubsystem::ApplyVolumes()
{
	UWorld* World = GetWorld();
	const UDBGameUserSettings* Settings = UDBGameUserSettings::Get();
	if (!World || !Settings || !World->GetAudioDeviceRaw())
	{
		return;
	}
	if (!VolumeMix)
	{
		VolumeMix = NewObject<USoundMix>(this);
		UGameplayStatics::PushSoundMixModifier(World, VolumeMix);
	}
	// -DBMute: tests with a real audio device that must not be heard (someone is playing on this machine).
	const float Master = FParse::Param(FCommandLine::Get(), TEXT("DBMute")) ? 0.f : FMath::Clamp(Settings->MasterVolume, 0.f, 1.f);
	const TPair<const TCHAR*, float> Classes[] = {{TEXT("/Engine/EngineSounds/Music.Music"), Settings->MusicVolume},
		{TEXT("/Engine/EngineSounds/SFX.SFX"), Settings->EffectsVolume}, {TEXT("/Engine/EngineSounds/Voice.Voice"), Settings->VoiceVolume}};
	for (const TPair<const TCHAR*, float>& Class : Classes)
	{
		if (USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, Class.Key, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, SoundClass, Master * FMath::Clamp(Class.Value, 0.f, 1.f), 1.f, 0.2f, true);
		}
	}
}

void UDBAudioSubsystem::Deinitialize()
{
	bActive = false;
	for (UAudioComponent* Component : {BedComponent.Get(), LayerComponent.Get(), MusicComponent.Get()})
	{
		if (Component)
		{
			Component->Stop();
		}
	}
	Super::Deinitialize();
}

TStatId UDBAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDBAudioSubsystem, STATGROUP_Tickables);
}

void UDBAudioSubsystem::BuildBank()
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
#if WITH_EDITOR
	// -game from the editor binaries scans assets in the background; the bank is needed now.
	Registry.ScanPathsSynchronous({AudioRoot}, false);
#endif
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(AudioRoot), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath != USoundWave::StaticClass()->GetClassPathName())
		{
			continue;
		}
		// /Game/DarkBlood/Audio/<Category>/S_<Name>_NN -> "<Category>/<Name>"
		FString Name = Asset.AssetName.ToString();
		Name.RemoveFromStart(TEXT("S_"));
		int32 Underscore = INDEX_NONE;
		if (Name.FindLastChar(TEXT('_'), Underscore) && Name.Mid(Underscore + 1).IsNumeric())
		{
			Name.LeftInline(Underscore);
		}
		const FString Category = FPaths::GetCleanFilename(Asset.PackagePath.ToString());
		Bank.FindOrAdd(FName(Category + TEXT("/") + Name)).Paths.Add(Asset.GetSoftObjectPath());
	}
	int32 Waves = 0;
	for (const TPair<FName, FGroup>& Group : Bank)
	{
		Waves += Group.Value.Paths.Num();
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBAUDIO bank: %d sounds, %d waves"), Bank.Num(), Waves);
}

USoundWave* UDBAudioSubsystem::Pick(FName Sound)
{
	FGroup* Group = Bank.Find(Sound);
	if (!Group || Group->Paths.IsEmpty())
	{
		return nullptr;
	}
	if (Group->Loaded.Num() != Group->Paths.Num())
	{
		Group->Loaded.Reset();
		for (const FSoftObjectPath& Path : Group->Paths)
		{
			if (USoundWave* Wave = Cast<USoundWave>(Path.TryLoad()))
			{
				Group->Loaded.Add(Wave);
				LoadedWaves.AddUnique(Wave);
			}
		}
		if (Group->Loaded.IsEmpty())
		{
			Group->Paths.Reset();
			return nullptr;
		}
	}
	// Never the same variation twice in a row.
	int32 Index = FMath::RandHelper(Group->Loaded.Num());
	if (Group->Loaded.Num() > 1 && Index == Group->Last)
	{
		Index = (Index + 1 + FMath::RandHelper(Group->Loaded.Num() - 1)) % Group->Loaded.Num();
	}
	Group->Last = Index;
	return Group->Loaded[Index];
}

USoundAttenuation* UDBAudioSubsystem::GetAttenuation(EDBSoundReach Reach)
{
	if (Attenuations.IsEmpty())
	{
		for (int32 Index = 0; Index < static_cast<int32>(EDBSoundReach::Count); ++Index)
		{
			USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(this);
			FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
			Settings.bAttenuate = true;
			Settings.bSpatialize = true;
			Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
			Settings.dBAttenuationAtMax = -50.f;
			Settings.AttenuationShape = EAttenuationShape::Sphere;
			Settings.AttenuationShapeExtents = FVector(ReachInner[Index], 0.f, 0.f);
			Settings.FalloffDistance = ReachFalloff[Index];
			// Far sounds lose their highs, like in air.
			Settings.bAttenuateWithLPF = true;
			Settings.LPFRadiusMin = ReachInner[Index] * 2.f;
			Settings.LPFRadiusMax = ReachInner[Index] + ReachFalloff[Index];
			Settings.LPFFrequencyAtMax = 2500.f;
			Attenuations.Add(Attenuation);
		}
	}
	return Attenuations[static_cast<int32>(Reach)];
}

USoundConcurrency* UDBAudioSubsystem::GetConcurrency(FName Sound)
{
	if (TObjectPtr<USoundConcurrency>* Found = Concurrency.Find(Sound))
	{
		return *Found;
	}
	// A big fight should stay readable: a handful of voices per sound, the farthest give way first.
	const FString Name = Sound.ToString();
	USoundConcurrency* Limit = NewObject<USoundConcurrency>(this);
	Limit->Concurrency.MaxCount = Name.StartsWith(TEXT("Footsteps")) ? 10 : Name.StartsWith(TEXT("Voice")) ? 3 : 6;
	Limit->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
	Limit->Concurrency.RetriggerTime = Name.StartsWith(TEXT("Footsteps")) ? 0.f : 0.03f;
	Concurrency.Add(Sound, Limit);
	return Limit;
}

void UDBAudioSubsystem::PlayAt(const UObject* WorldContext, FName Sound, const FVector& Location, EDBSoundReach Reach, float Volume, float Pitch)
{
	UDBAudioSubsystem* Audio = Get(WorldContext);
	UWorld* World = Audio ? Audio->GetWorld() : nullptr;
	if (!World || !World->GetAudioDeviceRaw())
	{
		return;
	}
	if (USoundWave* Wave = Audio->Pick(Sound))
	{
		UE_LOG(LogDarkBlood, Verbose, TEXT("DBAUDIO play %s (%s)"), *Sound.ToString(), *Wave->GetName());
		UGameplayStatics::PlaySoundAtLocation(World, Wave, Location, FRotator::ZeroRotator, Volume * FMath::FRandRange(0.88f, 1.f),
			Pitch * FMath::FRandRange(0.94f, 1.06f), 0.f, Audio->GetAttenuation(Reach), Audio->GetConcurrency(Sound));
	}
}

void UDBAudioSubsystem::Play2D(const UObject* WorldContext, FName Sound, float Volume)
{
	UDBAudioSubsystem* Audio = Get(WorldContext);
	UWorld* World = Audio ? Audio->GetWorld() : nullptr;
	if (!World || !World->GetAudioDeviceRaw())
	{
		return;
	}
	if (USoundWave* Wave = Audio->Pick(Sound))
	{
		UGameplayStatics::PlaySound2D(World, Wave, Volume, 1.f, 0.f, Audio->GetConcurrency(Sound));
	}
}

UAudioComponent* UDBAudioSubsystem::PlayLoopAttached(FName Sound, USceneComponent* Parent, const FVector& WorldLocation, float InnerRadius, float FalloffDistance,
	float Volume)
{
	UWorld* World = GetWorld();
	USoundWave* Wave = World && World->GetAudioDeviceRaw() && Parent ? Pick(Sound) : nullptr;
	if (!Wave)
	{
		return nullptr;
	}
	USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(Parent->GetOwner());
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Attenuation->Attenuation.dBAttenuationAtMax = -60.f;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(InnerRadius, 0.f, 0.f);
	Attenuation->Attenuation.FalloffDistance = FalloffDistance;
	Attenuation->Attenuation.bAttenuateWithLPF = true;
	Attenuation->Attenuation.LPFRadiusMin = InnerRadius;
	Attenuation->Attenuation.LPFRadiusMax = InnerRadius + FalloffDistance;
	Attenuation->Attenuation.LPFFrequencyAtMax = 1200.f;
	return UGameplayStatics::SpawnSoundAttached(Wave, Parent, NAME_None, WorldLocation, EAttachLocation::KeepWorldPosition, true, Volume, 1.f,
		FMath::FRandRange(0.f, 20.f), Attenuation);
}

UAudioComponent* UDBAudioSubsystem::MakeLoop(FName Sound, float Volume)
{
	UWorld* World = GetWorld();
	USoundWave* Wave = World && World->GetAudioDeviceRaw() ? Pick(Sound) : nullptr;
	if (!Wave)
	{
		return nullptr;
	}
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(World, Wave, 1.f, 1.f, 0.f, nullptr, true, true);
	if (Component)
	{
		// Start somewhere inside the loop, so the same bed never starts the same way.
		Component->FadeIn(AmbienceFade, Volume, FMath::FRandRange(0.f, Wave->Duration * 0.8f));
	}
	return Component;
}

void UDBAudioSubsystem::Tick(float DeltaTime)
{
	if (!bActive)
	{
		return;
	}
	SoundscapeTimer -= DeltaTime;
	if (SoundscapeTimer <= 0.f)
	{
		if (!VolumeMix)
		{
			ApplyVolumes();
		}
		SoundscapeTimer = 1.f;
		UpdateSoundscape();
	}
	UpdateMusic(DeltaTime);
}

void UDBAudioSubsystem::UpdateSoundscape()
{
	UWorld* World = GetWorld();
	FVector Listener;
	const APawn* Pawn = nullptr;
	if (!GetListener(World, Listener, Pawn))
	{
		return;
	}
	FName Bed;
	FName Layer;
	const ADBGameState* GameState = World->GetGameState<ADBGameState>();
	const UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr;
	const bool bNight = WorldState && WorldState->IsNight();
	if (const ADBDungeonInstance* Dungeon = ADBDungeonInstance::FindAt(World, Listener))
	{
		Bed = Dungeon->IsAbyss() ? TEXT("Ambience/DemonLands") : TEXT("Ambience/Wind");
		Layer = Dungeon->IsAbyss() ? NAME_None : FName(TEXT("Ambience/DemonLands"));
	}
	else if (FVector::Dist2D(Listener, DBEchoHall::GetCenter()) < DBEchoHall::Radius * 2.f && FMath::Abs(Listener.Z - DBEchoHall::GetCenter().Z) < 5000.f)
	{
		Bed = TEXT("Ambience/Wind");
		Layer = TEXT("Ambience/Shore");
	}
	else if (DBParadise::IsInParadise(Listener))
	{
		Bed = bNight ? TEXT("Ambience/Night") : TEXT("Ambience/ForestDay");
		Layer = TEXT("Ambience/Wind");
	}
	else
	{
		const double X = Listener.X / 100.0;
		const double Y = Listener.Y / 100.0;
		const FDBRealmRegion& Region = DBRealm::GetRegions()[DBRealm::FindRegionIndex(X, Y)];
		const double Ground = DBRealm::SampleHeight(X, Y);
		if (IsDemonBiome(Region.Biome))
		{
			Bed = TEXT("Ambience/DemonLands");
		}
		else if (IsGreenBiome(Region.Biome))
		{
			Bed = bNight ? TEXT("Ambience/Night") : TEXT("Ambience/ForestDay");
		}
		else
		{
			Bed = TEXT("Ambience/Wind");
		}
		// The surf where land meets the sea; elsewhere the demons' hold on a region is heard as a low drone.
		if (Ground < 6.0 || !DBRealm::IsInside(X, Y))
		{
			Layer = TEXT("Ambience/Shore");
		}
		else if (Bed != FName(TEXT("Ambience/DemonLands")) && WorldState && WorldState->GetDemonInfluence(Region.RegionId) > 0.6f)
		{
			Layer = TEXT("Ambience/DemonLands");
		}
	}
	SetAmbience(Bed, Layer);
}

void UDBAudioSubsystem::SetAmbience(FName Bed, FName Layer)
{
	if (Bed != AmbienceBed)
	{
		if (BedComponent)
		{
			BedComponent->FadeOut(AmbienceFade, 0.f);
		}
		AmbienceBed = Bed;
		BedComponent = Bed.IsNone() ? nullptr : MakeLoop(Bed, BedVolume);
		UE_LOG(LogDarkBlood, Display, TEXT("DBAUDIO ambience bed %s"), *Bed.ToString());
	}
	if (Layer != AmbienceLayer)
	{
		if (LayerComponent)
		{
			LayerComponent->FadeOut(AmbienceFade, 0.f);
		}
		AmbienceLayer = Layer;
		LayerComponent = Layer.IsNone() ? nullptr : MakeLoop(Layer, LayerVolume);
		UE_LOG(LogDarkBlood, Display, TEXT("DBAUDIO ambience layer %s"), Layer.IsNone() ? TEXT("-") : *Layer.ToString());
	}
}

void UDBAudioSubsystem::UpdateMusic(float DeltaTime)
{
	MusicTimer -= DeltaTime;
	ExplorePlayed += MusicState == EDBMusicState::Explore ? DeltaTime : 0.f;
	ExploreRest -= MusicState == EDBMusicState::Silence ? DeltaTime : 0.f;
	if (MusicTimer > 0.f)
	{
		return;
	}
	MusicTimer = 0.5f;
	UWorld* World = GetWorld();
	FVector Listener;
	const APawn* Pawn = nullptr;
	if (!GetListener(World, Listener, Pawn))
	{
		return;
	}
	EDBMusicState Wanted = EDBMusicState::Silence;
	if (ForcedMusic >= 0)
	{
		Wanted = static_cast<EDBMusicState>(FMath::Clamp(ForcedMusic, 0, 3));
	}
	else
	{
		// A boss close by (its fight), demons on the player (a fight), otherwise themes that come and go.
		const FVector Center = Pawn ? Pawn->GetActorLocation() : Listener;
		bool bBoss = false;
		int32 Hunters = 0;
		for (TActorIterator<ADBEnemyCharacter> It(World); It; ++It)
		{
			if (It->IsDead() || It->GetTeam() != EDBTeam::Demons)
			{
				continue;
			}
			const float Distance = FVector::Dist(It->GetActorLocation(), Center);
			if (It->IsA<ADBBossCharacter>() && Distance < 6000.f)
			{
				bBoss = true;
			}
			else if (Distance < 1500.f)
			{
				++Hunters;
			}
		}
		CombatCalm = bBoss || Hunters > 0 ? 0.f : CombatCalm + 0.5f;
		if (bBoss)
		{
			Wanted = EDBMusicState::Boss;
		}
		else if (Hunters > 0 || (CombatCalm < 6.f && (MusicState == EDBMusicState::Battle || MusicState == EDBMusicState::Boss)))
		{
			// Battle music lingers a few seconds after the last demon, so it does not flicker.
			Wanted = EDBMusicState::Battle;
		}
		else if (MusicState == EDBMusicState::Explore)
		{
			const float Length = MusicComponent && MusicComponent->Sound ? MusicComponent->Sound->GetDuration() : 100.f;
			Wanted = ExplorePlayed < Length - MusicFade ? EDBMusicState::Explore : EDBMusicState::Silence;
		}
		else
		{
			Wanted = ExploreRest <= 0.f ? EDBMusicState::Explore : EDBMusicState::Silence;
		}
	}
	if (Wanted == MusicState)
	{
		return;
	}
	if (MusicState == EDBMusicState::Explore && Wanted == EDBMusicState::Silence)
	{
		// The theme has played out: a long quiet stretch before the next one (90 to 180 s).
		ExploreRest = FMath::FRandRange(90.f, 180.f);
	}
	if (Wanted == EDBMusicState::Explore)
	{
		ExplorePlayed = 0.f;
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBAUDIO music %s -> %s"), *StaticEnum<EDBMusicState>()->GetNameStringByValue(static_cast<int64>(MusicState)),
		*StaticEnum<EDBMusicState>()->GetNameStringByValue(static_cast<int64>(Wanted)));
	MusicState = Wanted;
	switch (Wanted)
	{
	case EDBMusicState::Explore: CrossfadeMusic(TEXT("Music/Explore")); break;
	case EDBMusicState::Battle: CrossfadeMusic(TEXT("Music/Battle")); break;
	case EDBMusicState::Boss: CrossfadeMusic(TEXT("Music/Boss")); break;
	default: CrossfadeMusic(NAME_None); break;
	}
}

void UDBAudioSubsystem::CrossfadeMusic(FName Track)
{
	if (Track == MusicTrack)
	{
		return;
	}
	if (MusicComponent)
	{
		MusicComponent->FadeOut(MusicFade, 0.f);
		MusicComponent = nullptr;
	}
	MusicTrack = Track;
	UWorld* World = GetWorld();
	USoundWave* Wave = !Track.IsNone() && World && World->GetAudioDeviceRaw() ? Pick(Track) : nullptr;
	if (!Wave)
	{
		return;
	}
	const float Volume = Track == FName(TEXT("Music/Explore")) ? 0.42f : Track == FName(TEXT("Music/Boss")) ? 0.62f : 0.55f;
	MusicComponent = UGameplayStatics::CreateSound2D(World, Wave, 1.f, 1.f, 0.f, nullptr, true, true);
	if (MusicComponent)
	{
		MusicComponent->FadeIn(MusicFade, Volume);
	}
}
