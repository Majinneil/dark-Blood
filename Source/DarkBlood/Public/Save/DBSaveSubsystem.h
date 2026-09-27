// Reads and writes character and world records.
//  - LocalCharacters mode: every machine stores its own characters; the host stores the world.
//  - ServerAuthoritative mode: the (dedicated) server stores characters keyed by player id.
#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/Records.h"

#include "DBSaveSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FDBCharacterSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Save") FString SlotKey;
	UPROPERTY(BlueprintReadOnly, Category = "Save") FString CharacterName;
	UPROPERTY(BlueprintReadOnly, Category = "Save") FName ClassId;
	UPROPERTY(BlueprintReadOnly, Category = "Save") int32 Level = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Save") FDateTime SavedAtUtc;
};

UCLASS()
class DARKBLOOD_API UDBSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ---- Characters ---------------------------------------------------------------------------

	/** Builds and validates a new character (class start kit included) without storing it. */
	bool BuildNewCharacter(const FString& Name, FName ClassId, const FDBAppearance& Appearance,
		DarkBlood::Rules::FCharacterRecord& OutRecord, FString& OutError) const;

	/** Creates, validates and stores a new character. Returns false with a reason on invalid input. */
	bool CreateCharacter(const FString& SlotKey, const FString& Name, FName ClassId, const FDBAppearance& Appearance,
		DarkBlood::Rules::FCharacterRecord& OutRecord, FString& OutError);

	bool SaveCharacter(const FString& SlotKey, const DarkBlood::Rules::FCharacterRecord& Record);
	DarkBlood::Rules::ELoadResult LoadCharacter(const FString& SlotKey, DarkBlood::Rules::FCharacterRecord& OutRecord) const;
	bool DoesCharacterExist(const FString& SlotKey) const;

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Save")
	TArray<FDBCharacterSlotInfo> ListCharacters() const;

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Save")
	void SetActiveCharacterSlot(const FString& SlotKey) { ActiveCharacterSlot = SlotKey; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Save")
	FString GetActiveCharacterSlot() const { return ActiveCharacterSlot; }

	/** Serialized record of the active slot; creates a development character when none exists yet. */
	bool LoadOrCreateActiveCharacterData(TArray<uint8>& OutData);
	bool LoadOrCreateActiveCharacter(DarkBlood::Rules::FCharacterRecord& OutRecord);

	/** Stores bytes received from the server for the active slot after verifying they deserialize. */
	bool StoreActiveCharacterData(const TArray<uint8>& Data);

	/** Name for development characters: -DBCharacterName="..." or "Wanderer". */
	FString GetDevelopmentCharacterName() const;

	/** Slot key used by a dedicated server for a player id (+ optional character index). */
	static FString MakeServerSlotKey(const FString& PlayerId, const FString& CharacterIndex);

	// ---- World --------------------------------------------------------------------------------

	bool SaveWorld(const FString& WorldSlot, const DarkBlood::Rules::FWorldRecord& Record);
	DarkBlood::Rules::ELoadResult LoadWorld(const FString& WorldSlot, DarkBlood::Rules::FWorldRecord& OutRecord) const;

private:
	static FString CharacterSaveName(const FString& SlotKey) { return TEXT("DB_Character_") + SlotKey; }
	static FString WorldSaveName(const FString& WorldSlot) { return TEXT("DB_World_") + WorldSlot; }
	static const TCHAR* IndexSaveName() { return TEXT("DB_CharacterIndex"); }

	void AddToIndex(const FString& SlotKey);

	FString ActiveCharacterSlot = TEXT("Slot0");
};
