// SaveGame containers. The payload is the versioned, checksummed rules-core record
// (DarkBlood::Rules::SerializeCharacter / SerializeWorld); header fields exist for menus only.
#pragma once

#include "GameFramework/SaveGame.h"

#include "DBSaveGames.generated.h"

UCLASS()
class DARKBLOOD_API UDBCharacterSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FGuid CharacterId;
	UPROPERTY() FString CharacterName;
	UPROPERTY() FName ClassId;
	UPROPERTY() int32 Level = 1;
	UPROPERTY() FDateTime SavedAtUtc;
	UPROPERTY() TArray<uint8> RecordData;
};

UCLASS()
class DARKBLOOD_API UDBWorldSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FString WorldId;
	UPROPERTY() FDateTime SavedAtUtc;
	UPROPERTY() TArray<uint8> RecordData;
};

/** Lists local character slots for the character selection screen. */
UCLASS()
class DARKBLOOD_API UDBCharacterIndexSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() TArray<FString> CharacterSlots;
};
