// Player controller: input setup, character hand-over between client and server, dev commands.
#pragma once

#include "GameFramework/PlayerController.h"
#include "Core/DBTypes.h"

#include "DBPlayerController.generated.h"

class UDBInputConfig;

UCLASS()
class DARKBLOOD_API ADBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADBPlayerController();

	const UDBInputConfig* GetInputConfig() const;

	virtual void BeginPlay() override;
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	// ---- Character hand-over ------------------------------------------------------------------

	/** Client -> server: local character data (LocalCharacters persistence). Validated by the game mode. */
	UFUNCTION(Server, Reliable)
	void ServerUploadCharacter(const TArray<uint8>& CharacterData);

	/** Client -> server: request a new character (ServerAuthoritative persistence). */
	UFUNCTION(Server, Reliable)
	void ServerCreateCharacter(const FString& CharacterName, FName ClassId, const FDBAppearance& Appearance);

	/** Server -> client: latest authoritative snapshot to store locally (LocalCharacters persistence). */
	UFUNCTION(Client, Reliable)
	void ClientStoreCharacterSnapshot(const TArray<uint8>& CharacterData);

	UFUNCTION(Client, Reliable)
	void ClientCharacterRejected(const FString& Reason);

	/** Server -> client: no stored character exists on the server; the client must create one. */
	UFUNCTION(Client, Reliable)
	void ClientRequestCharacterCreation();

	bool HasUploadedCharacter() const { return bCharacterUploaded; }

	// ---- Development --------------------------------------------------------------------------

	/** Forwards a DB* developer command to the server's cheat manager (dev builds only). */
	UFUNCTION(Server, Reliable)
	void ServerRunDevCommand(const FString& Command);

	UFUNCTION(Exec)
	void DBToggleDebugHUD();

protected:
	/** Assign an input config asset in a Blueprint subclass; a code-generated fallback is used otherwise. */
	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Input")
	TObjectPtr<UDBInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Input")
	int32 MappingContextPriority = 0;

private:
	void UploadLocalCharacter();

	UPROPERTY(Transient)
	TObjectPtr<UDBInputConfig> RuntimeInputConfig;

	bool bCharacterUploaded = false;
};
