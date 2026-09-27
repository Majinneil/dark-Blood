#include "Player/DBPlayerController.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Core/DBGameSettings.h"
#include "DarkBlood.h"
#include "Debug/DBCheatManager.h"
#include "Debug/DBDebugHUD.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/DBGameMode.h"
#include "Input/DBInputConfig.h"
#include "Player/DBPlayerState.h"
#include "Save/DBSaveSubsystem.h"

ADBPlayerController::ADBPlayerController()
{
	CheatClass = UDBCheatManager::StaticClass();
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
			UploadLocalCharacter();
		}
	}
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
	// Phase 3 opens the character creator here. Until then a development character is requested.
	UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
	const FString Name = Saves ? Saves->GetDevelopmentCharacterName() : TEXT("Wanderer");
	UE_LOG(LogDBSave, Warning, TEXT("Server has no character for this player - creating DEVELOPMENT character '%s'"), *Name);
	ServerCreateCharacter(Name, UDBGameSettings::Get().DevelopmentDefaultClass, FDBAppearance());
}

void ADBPlayerController::ServerRunDevCommand_Implementation(const FString& Command)
{
#if !UE_BUILD_SHIPPING
	const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
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
