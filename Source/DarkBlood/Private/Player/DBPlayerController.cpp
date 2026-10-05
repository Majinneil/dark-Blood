#include "Player/DBPlayerController.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Core/DBGameSettings.h"
#include "DarkBlood.h"
#include "Debug/DBCheatManager.h"
#include "Debug/DBDebugHUD.h"
#include "Dialogue/DBDialogueComponent.h"
#include "Interaction/DBInteractionComponent.h"
#include "UI/DBGameHUD.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/DBGameMode.h"
#include "Input/DBInputConfig.h"
#include "Player/DBPlayerState.h"
#include "Save/DBSaveSubsystem.h"
#include "World/DBCarriageStation.h"

ADBPlayerController::ADBPlayerController()
{
	CheatClass = UDBCheatManager::StaticClass();
	Dialogue = CreateDefaultSubobject<UDBDialogueComponent>(TEXT("Dialogue"));
}

const UDBInputConfig* ADBPlayerController::GetInputConfig() const
{
	if (InputConfig)
	{
		return InputConfig;
	}
	if (!RuntimeInputConfig)
	{
		ADBPlayerController* MutableThis = const_cast<ADBPlayerController*>(this);
		MutableThis->RuntimeInputConfig = UDBInputConfig::CreateDevelopmentDefaults(MutableThis);
		UE_LOG(LogDarkBlood, Warning, TEXT("No input config asset assigned - using code-generated DEVELOPMENT input bindings"));
	}
	return RuntimeInputConfig;
}

void ADBPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		const UDBInputConfig* Config = GetInputConfig();
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (Config && Config->DefaultMappingContext)
			{
				Subsystem->AddMappingContext(Config->DefaultMappingContext, MappingContextPriority);
			}
		}

		// Remote clients bring their own character into the host's world.
		if (!HasAuthority() && UDBGameSettings::Get().GetEffectivePersistenceMode() == EDBPersistenceMode::LocalCharacters)
		{
			const UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
			if (Saves && Saves->ShouldUseCharacterCreator())
			{
				RequestLocalCharacterCreation();
			}
			else
			{
				UploadLocalCharacter();
			}
		}
		RunAutoExecScript();
	}
}

void ADBPlayerController::RunAutoExecScript()
{
#if !UE_BUILD_SHIPPING
	// -DBAutoExec="Cmd1|Cmd2|..." runs once the local controller exists (unlike -ExecCmds, this also works on
	// clients, after connecting). Used for scripted multiplayer tests together with DBAfter.
	static bool bRan = false;
	FString Script;
	if (bRan || !FParse::Value(FCommandLine::Get(), TEXT("DBAutoExec="), Script, false))
	{
		return;
	}
	bRan = true;
	EnableCheats();
	TArray<FString> Commands;
	Script.TrimQuotes().ParseIntoArray(Commands, TEXT("|"));
	for (const FString& Command : Commands)
	{
		UE_LOG(LogDarkBlood, Display, TEXT("DBAutoExec: %s"), *Command);
		ConsoleCommand(Command.TrimStartAndEnd(), true);
	}
#endif
}

void ADBPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (const ADBPlayerState* DBPlayerState = GetPlayerState<ADBPlayerState>())
	{
		if (UDBAbilitySystemComponent* ASC = DBPlayerState->GetDBAbilitySystemComponent())
		{
			ASC->ProcessAbilityInput(DeltaTime, bGamePaused);
		}
	}
	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void ADBPlayerController::UploadLocalCharacter()
{
	UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
	TArray<uint8> Data;
	if (!Saves || !Saves->LoadOrCreateActiveCharacterData(Data))
	{
		UE_LOG(LogDBSave, Error, TEXT("No local character available for upload"));
		return;
	}
	UE_LOG(LogDBSave, Log, TEXT("Uploading local character (%d bytes)"), Data.Num());
	ServerUploadCharacter(Data);
}

void ADBPlayerController::ServerUploadCharacter_Implementation(const TArray<uint8>& CharacterData)
{
	if (bCharacterUploaded)
	{
		ClientCharacterRejected(TEXT("Character already loaded for this session."));
		return;
	}
	if (CharacterData.Num() > UDBGameSettings::Get().MaxCharacterUploadBytes)
	{
		ClientCharacterRejected(TEXT("Character data too large."));
		return;
	}
	if (ADBGameMode* GameMode = GetWorld()->GetAuthGameMode<ADBGameMode>())
	{
		FString Error;
		if (GameMode->AcceptCharacterData(this, CharacterData, Error))
		{
			bCharacterUploaded = true;
		}
		else
		{
			ClientCharacterRejected(Error);
		}
	}
}

void ADBPlayerController::ServerCreateCharacter_Implementation(const FString& CharacterName, FName ClassId, const FDBAppearance& Appearance)
{
	if (bCharacterUploaded)
	{
		return;
	}
	if (ADBGameMode* GameMode = GetWorld()->GetAuthGameMode<ADBGameMode>())
	{
		FString Error;
		if (GameMode->CreateServerCharacter(this, CharacterName, ClassId, Appearance, Error))
		{
			bCharacterUploaded = true;
		}
		else
		{
			ClientCharacterRejected(Error);
		}
	}
}

void ADBPlayerController::ClientStoreCharacterSnapshot_Implementation(const TArray<uint8>& CharacterData)
{
	// A client RPC on a controller without a connection (guest during server shutdown) runs on the server;
	// storing it there would write the guest into the host's active slot.
	if (!GetLocalPlayer())
	{
		return;
	}
	if (UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>())
	{
		Saves->StoreActiveCharacterData(CharacterData);
	}
}

void ADBPlayerController::ClientCharacterRejected_Implementation(const FString& Reason)
{
	UE_LOG(LogDBSave, Error, TEXT("Server rejected character: %s"), *Reason);
	ClientMessage(FString::Printf(TEXT("Charakter abgelehnt: %s"), *Reason));
}

void ADBPlayerController::ClientRequestCharacterCreation_Implementation()
{
	UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
	if (Saves && Saves->CanShowCharacterCreator())
	{
		RequestLocalCharacterCreation();
		return;
	}
	// Headless / scripted runs: create a development character without UI.
	const FString Name = Saves ? Saves->GetDevelopmentCharacterName() : TEXT("Wanderer");
	UE_LOG(LogDBSave, Warning, TEXT("Server has no character for this player - creating DEVELOPMENT character '%s'"), *Name);
	ServerCreateCharacter(Name, UDBGameSettings::Get().DevelopmentDefaultClass, FDBAppearance());
}

void ADBPlayerController::ServerRunDevCommand_Implementation(const FString& Command)
{
#if !UE_BUILD_SHIPPING
	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	if (!GameMode || !GameMode->AllowCheats(this) || !Command.StartsWith(TEXT("DB")))
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("Refused dev command from %s: %s"), *GetName(), *Command);
		return;
	}
	if (!CheatManager)
	{
		CheatManager = NewObject<UCheatManager>(this, CheatClass);
		CheatManager->InitCheatManager();
	}
	CheatManager->ProcessConsoleExec(*Command, *GLog, this);
#endif
}

void ADBPlayerController::DBToggleDebugHUD()
{
	if (ADBDebugHUD* DebugHUD = GetHUD<ADBDebugHUD>())
	{
		DebugHUD->ToggleDebugOverlay();
	}
}

void ADBPlayerController::RequestLocalCharacterCreation()
{
	bCreationPending = true;
	UE_LOG(LogDBSave, Log, TEXT("Opening character creator"));
	if (ADBGameHUD* GameHUD = GetHUD<ADBGameHUD>())
	{
		GameHUD->ShowCharacterCreator(); // otherwise the HUD opens it in its BeginPlay
	}
}

bool ADBPlayerController::SubmitCharacterCreation(const FString& CharacterName, FName ClassId, const FDBAppearance& Appearance, FString& OutError)
{
	if (UDBGameSettings::Get().GetEffectivePersistenceMode() == EDBPersistenceMode::ServerAuthoritative)
	{
		// The server validates and stores; a rejection comes back through ClientCharacterRejected.
		ServerCreateCharacter(CharacterName, ClassId, Appearance);
	}
	else
	{
		UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
		DarkBlood::Rules::FCharacterRecord Record;
		if (!Saves || !Saves->CreateCharacter(Saves->GetActiveCharacterSlot(), CharacterName, ClassId, Appearance, Record, OutError))
		{
			return false;
		}
		if (HasAuthority())
		{
			ADBGameMode* GameMode = GetWorld()->GetAuthGameMode<ADBGameMode>();
			if (!GameMode || !GameMode->AcceptCharacterRecord(this, Record, OutError))
			{
				return false;
			}
		}
		else
		{
			UploadLocalCharacter();
		}
	}
	UE_LOG(LogDBSave, Log, TEXT("Character created: '%s' (%s)"), *CharacterName, *ClassId.ToString());
	bCreationPending = false;
	if (ADBGameHUD* GameHUD = GetHUD<ADBGameHUD>())
	{
		GameHUD->HideCharacterCreator();
	}
	return true;
}

void ADBPlayerController::ServerInteract_Implementation(AActor* Target)
{
	UDBInteractionComponent::ServerValidateAndInteract(this, Target);
}

void ADBPlayerController::ClientShowNotification_Implementation(const FText& Text)
{
	UE_LOG(LogDarkBlood, Display, TEXT("Notification: %s"), *Text.ToString());
	if (ADBGameHUD* GameHUD = GetHUD<ADBGameHUD>())
	{
		GameHUD->ShowNotification(Text);
	}
}

void ADBPlayerController::ClientShowFinale_Implementation()
{
	UE_LOG(LogDarkBlood, Display, TEXT("Finale shown"));
	if (ADBGameHUD* GameHUD = GetHUD<ADBGameHUD>())
	{
		GameHUD->ShowFinale();
	}
}

void ADBPlayerController::ClientOpenCrafting_Implementation(AActor* Station)
{
	UE_LOG(LogDarkBlood, Display, TEXT("Crafting opened: %s"), *GetNameSafe(Station));
	if (ADBGameHUD* GameHUD = GetHUD<ADBGameHUD>())
	{
		GameHUD->ShowCrafting(Station);
	}
}

void ADBPlayerController::ClientOpenCarriage_Implementation(AActor* Station)
{
	UE_LOG(LogDarkBlood, Display, TEXT("Carriage opened: %s"), *GetNameSafe(Station));
	if (ADBGameHUD* GameHUD = GetHUD<ADBGameHUD>())
	{
		GameHUD->ShowCarriage(Station);
	}
}

void ADBPlayerController::ServerTravelByCarriage_Implementation(AActor* Station, int32 Destination)
{
	FText Reason;
	ADBCarriageStation* Carriage = Cast<ADBCarriageStation>(Station);
	if (!Carriage || !Carriage->Travel(this, Destination, Reason))
	{
		ClientShowNotification(Reason.IsEmpty() ? NSLOCTEXT("DarkBlood", "CarriageRefused", "Die Kutsche faehrt nicht.") : Reason);
	}
}
