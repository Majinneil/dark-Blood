// Player controller: input setup, character hand-over between client and server, dev commands.
#pragma once

#include "GameFramework/PlayerController.h"
#include "Core/DBTypes.h"

#include "DBPlayerController.generated.h"

class UDBDialogueComponent;
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

	/** Development: runs the -DBAutoExec="Cmd|Cmd" command line script once (non-shipping). */
	void RunAutoExecScript();

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

	/** Local: result of the character creator. Returns false (with a reason) if the character was not created. */
	bool SubmitCharacterCreation(const FString& CharacterName, FName ClassId, const FDBAppearance& Appearance, FString& OutError);

	/** Local: the character creator should be shown (the HUD may not exist yet when this is requested). */
	bool IsCharacterCreationPending() const { return bCreationPending; }

	// ---- Interaction, dialogue, notifications -------------------------------------------------

	UDBDialogueComponent* GetDialogue() const { return Dialogue; }

	/** Client -> server: use an interactable (validated: range, conditions). */
	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	/** Server -> client: open the crafting window of a station. */
	UFUNCTION(Client, Reliable)
	void ClientOpenCrafting(AActor* Station);

	/** Server -> client: short on-screen message (quest started/completed ...). */
	UFUNCTION(Client, Reliable)
	void ClientShowNotification(const FText& Text);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dark Blood|Dialogue")
	TObjectPtr<UDBDialogueComponent> Dialogue;

private:
	void UploadLocalCharacter();
	void RequestLocalCharacterCreation();

	UPROPERTY(Transient)
	TObjectPtr<UDBInputConfig> RuntimeInputConfig;

	bool bCharacterUploaded = false;
	bool bCreationPending = false;
};
