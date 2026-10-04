// Project settings (Project Settings > Game > Dark Blood). Stored in Config/DefaultGame.ini.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "Core/DBTypes.h"

#include "DBGameSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Dark Blood"))
class DARKBLOOD_API UDBGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDBGameSettings();

	static const UDBGameSettings& Get() { return *GetDefault<UDBGameSettings>(); }

	/** Resolves the persistence mode, honoring the -DBPersistence=Local|Server command line override. */
	EDBPersistenceMode GetEffectivePersistenceMode() const;

	UPROPERTY(Config, EditAnywhere, Category = "Persistence")
	EDBPersistenceMode PersistenceMode = EDBPersistenceMode::LocalCharacters;

	UPROPERTY(Config, EditAnywhere, Category = "Persistence", meta = (ClampMin = 10))
	float AutosaveIntervalSeconds = 120.f;

	UPROPERTY(Config, EditAnywhere, Category = "Persistence")
	FString DefaultWorldSlot = TEXT("World0");

	/** Upper bound for character data uploaded by clients (bytes). */
	UPROPERTY(Config, EditAnywhere, Category = "Persistence", meta = (ClampMin = 1024))
	int32 MaxCharacterUploadBytes = 64 * 1024;

	UPROPERTY(Config, EditAnywhere, Category = "Character", meta = (ClampMin = 0))
	int32 BasePouchCapacity = 20;

	UPROPERTY(Config, EditAnywhere, Category = "Character", meta = (ClampMin = 0))
	float RespawnDelaySeconds = 5.f;

	/** Class used when a development character has to be created without a character creator. */
	UPROPERTY(Config, EditAnywhere, Category = "Character")
	FName DevelopmentDefaultClass = TEXT("Warrior");

	/** Shared (story) quests started automatically when a new world is created. */
	UPROPERTY(Config, EditAnywhere, Category = "World")
	TArray<FName> InitialSharedQuests;

	/** Real minutes per in-game day. */
	UPROPERTY(Config, EditAnywhere, Category = "World", meta = (ClampMin = 1))
	float RealMinutesPerGameDay = 48.f;

	/** How often (seconds) the world clock is re-sent to clients; they extrapolate in between. */
	UPROPERTY(Config, EditAnywhere, Category = "World", meta = (ClampMin = 0.1))
	float ClockReplicationInterval = 2.f;
};
