// Server-only game rules: character acceptance, persistence, autosave, death and respawn.
#pragma once

#include "GameFramework/GameModeBase.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/Records.h"

#include "DBGameMode.generated.h"

class ADBCharacterBase;
class ADBPlayerController;
class ADBPlayerState;

UCLASS()
class DARKBLOOD_API ADBGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADBGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options,
		const FString& Portal = TEXT("")) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void Logout(AController* Exiting) override;

	/** Standalone/PIE as usual; -DBCheats also allows dev commands on listen/dedicated servers (non-shipping). */
	virtual bool AllowCheats(APlayerController* P) override;

	/** Deserializes, validates and applies character data uploaded by a client. */
	bool AcceptCharacterData(ADBPlayerController* Controller, const TArray<uint8>& Data, FString& OutError);

	/** Validates and applies a character record (from any source) to the controller's player state. */
	bool AcceptCharacterRecord(APlayerController* Controller, const DarkBlood::Rules::FCharacterRecord& Record, FString& OutError);

	/** ServerAuthoritative mode: creates and stores a new character on the server. */
	bool CreateServerCharacter(ADBPlayerController* Controller, const FString& Name, FName ClassId, const FDBAppearance& Appearance,
		FString& OutError);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Save")
	void SaveAll();

	void SaveCharacter(ADBPlayerState* PlayerState);
	void SaveWorld();

	void RespawnPlayer(AController* Controller);

	/** New Game+ (docs/ENDGAME.md): the purified world begins its next cycle - regions occupied again, vassals back on
	 *  their thrones, the shared story restarts; characters, items and settlements stay. Saves at once. */
	bool BeginNewCycle();

private:
	UFUNCTION()
	void HandleCharacterDied(ADBCharacterBase* Character);

	FString GetServerSlotKey(const APlayerController* Controller) const;

	FString WorldSlot;
	FString WorldId;
	EDBPersistenceMode PersistenceMode = EDBPersistenceMode::LocalCharacters;
	FTimerHandle AutosaveTimer;
	TMap<TWeakObjectPtr<APlayerController>, FString> RequestedCharacterIndex;
	/** Lasting player keys sent with the login (UDBLocalPlayer), for platforms without a stable id. */
	TMap<TWeakObjectPtr<APlayerController>, FString> PlayerKeys;
};
