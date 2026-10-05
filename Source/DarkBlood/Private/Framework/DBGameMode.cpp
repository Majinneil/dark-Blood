#include "Framework/DBGameMode.h"

#include "Framework/DBDevelopmentSlice.h"
#include "UI/DBGameHUD.h"

#include "Character/DBPlayerCharacter.h"
#include "Core/DBGameSettings.h"
#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBGameDataSubsystem.h"
#include "Debug/DBDebugHUD.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "GameFramework/PlayerStart.h"
#include "Inventory/DBInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "Quest/DBQuestComponent.h"
#include "Save/DBSaveSubsystem.h"
#include "TimerManager.h"
#include "World/DBWorldStateComponent.h"

namespace R = DarkBlood::Rules;

ADBGameMode::ADBGameMode()
{
	GameStateClass = ADBGameState::StaticClass();
	PlayerStateClass = ADBPlayerState::StaticClass();
	PlayerControllerClass = ADBPlayerController::StaticClass();
	DefaultPawnClass = ADBPlayerCharacter::StaticClass();
	HUDClass = ADBGameHUD::StaticClass();
}

void ADBGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	PersistenceMode = UDBGameSettings::Get().GetEffectivePersistenceMode();
	WorldSlot = UGameplayStatics::ParseOption(Options, TEXT("World"));
	if (WorldSlot.IsEmpty())
	{
		WorldSlot = UDBGameSettings::Get().DefaultWorldSlot;
	}
	UE_LOG(LogDarkBlood, Log, TEXT("DARK BLOOD session: world slot '%s', persistence %s"), *WorldSlot,
		PersistenceMode == EDBPersistenceMode::LocalCharacters ? TEXT("LocalCharacters") : TEXT("ServerAuthoritative"));
}

void ADBGameMode::InitGameState()
{
	Super::InitGameState();

	ADBGameState* DBGameState = GetGameState<ADBGameState>();
	UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
	if (!DBGameState || !Saves)
	{
		return;
	}

	R::FWorldRecord Record;
	if (Saves->LoadWorld(WorldSlot, Record) == R::ELoadResult::Ok)
	{
		WorldId = DBBridge::ToFString(Record.WorldId);
		DBGameState->GetWorldState()->RestoreFromRecord(Record.World);
		DBGameState->GetSharedQuests()->RestoreFromRecord(Record.World.SharedQuests);
		UE_LOG(LogDBSave, Log, TEXT("World '%s' loaded (%d/%d vassals defeated)"), *WorldSlot, Record.World.CountDefeatedVassals(),
			R::NumVassals);
		return;
	}

	// New world.
	WorldId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	DBGameState->GetWorldState()->EnsureRegionsRegistered();
	for (const FName& QuestId : UDBGameSettings::Get().InitialSharedQuests)
	{
		DBGameState->GetSharedQuests()->StartQuest(QuestId, DBGameState->GetWorldState()->GetRulesState().StoryFlags);
	}
	UE_LOG(LogDBSave, Log, TEXT("New world created in slot '%s'"), *WorldSlot);
}

void ADBGameMode::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(AutosaveTimer, this, &ADBGameMode::SaveAll, UDBGameSettings::Get().AutosaveIntervalSeconds, true);

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("DBDevSlice")))
	{
		FTransform Origin = FTransform::Identity;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			Origin = FTransform(FRotator(0.f, It->GetActorRotation().Yaw, 0.f), It->GetActorLocation());
			break;
		}
		DBDevelopmentSlice::Spawn(GetWorld(), Origin);
	}
#endif
}

void ADBGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveAll();
	Super::EndPlay(EndPlayReason);
}

FString ADBGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options,
	const FString& Portal)
{
	RequestedCharacterIndex.Add(NewPlayerController, UGameplayStatics::ParseOption(Options, TEXT("Character")));
	return Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
}

FString ADBGameMode::GetServerSlotKey(const APlayerController* Controller) const
{
	const APlayerState* PlayerState = Controller ? Controller->PlayerState : nullptr;
	const FString PlayerId = PlayerState && PlayerState->GetUniqueId().IsValid() ? PlayerState->GetUniqueId().ToString()
		: (PlayerState ? PlayerState->GetPlayerName() : TEXT("Unknown"));
	const FString* Index = RequestedCharacterIndex.Find(const_cast<APlayerController*>(Controller));
	return UDBSaveSubsystem::MakeServerSlotKey(PlayerId, Index ? *Index : FString());
}

void ADBGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
	if (Saves && NewPlayer)
	{
		if (PersistenceMode == EDBPersistenceMode::LocalCharacters)
		{
			// The host (or a standalone player) loads directly; remote clients upload from their controller.
			if (NewPlayer->IsLocalController() && Saves->ShouldUseCharacterCreator())
			{
				if (ADBPlayerController* DBController = Cast<ADBPlayerController>(NewPlayer))
				{
					DBController->ClientRequestCharacterCreation(); // local: opens the creator
				}
			}
			else if (NewPlayer->IsLocalController())
			{
				R::FCharacterRecord Record;
				FString Error;
				if (!Saves->LoadOrCreateActiveCharacter(Record) || !AcceptCharacterRecord(NewPlayer, Record, Error))
				{
					UE_LOG(LogDBSave, Error, TEXT("Local character could not be loaded: %s"), *Error);
				}
			}
		}
		else
		{
			const FString SlotKey = GetServerSlotKey(NewPlayer);
			R::FCharacterRecord Record;
			FString Error;
			if (Saves->DoesCharacterExist(SlotKey) && Saves->LoadCharacter(SlotKey, Record) == R::ELoadResult::Ok)
			{
				if (!AcceptCharacterRecord(NewPlayer, Record, Error))
				{
					UE_LOG(LogDBSave, Error, TEXT("Stored character %s rejected: %s"), *SlotKey, *Error);
				}
			}
			else if (ADBPlayerController* DBController = Cast<ADBPlayerController>(NewPlayer))
			{
				DBController->ClientRequestCharacterCreation();
			}
		}
	}
	// The base implementation only spawns the pawn; skip it if accepting the character already did.
	if (NewPlayer && !NewPlayer->GetPawn())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
}

bool ADBGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// No body before the character data is loaded and validated.
	const ADBPlayerState* PlayerState = Player ? Player->GetPlayerState<ADBPlayerState>() : nullptr;
	return PlayerState && PlayerState->IsCharacterReady() && Super::PlayerCanRestart_Implementation(Player);
}

bool ADBGameMode::AcceptCharacterData(ADBPlayerController* Controller, const TArray<uint8>& Data, FString& OutError)
{
	R::FCharacterRecord Record;
	const R::ELoadResult Result = R::DeserializeCharacter(Data.GetData(), Data.Num(), Record);
	if (Result != R::ELoadResult::Ok)
	{
		OutError = FString::Printf(TEXT("Charakterdaten beschaedigt (%hs)."), R::ToString(Result));
		return false;
	}
	return AcceptCharacterRecord(Controller, Record, OutError);
}

bool ADBGameMode::AcceptCharacterRecord(APlayerController* Controller, const R::FCharacterRecord& Record, FString& OutError)
{
	ADBPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ADBPlayerState>() : nullptr;
	const UDBGameDataSubsystem* Data = GetGameInstance()->GetSubsystem<UDBGameDataSubsystem>();
	if (!PlayerState || !Data)
	{
		OutError = TEXT("Server nicht bereit.");
		return false;
	}
	if (PlayerState->IsCharacterReady())
	{
		OutError = TEXT("Fuer diesen Spieler ist bereits ein Charakter geladen.");
		return false;
	}

	const UDBClassDefinition* ClassDefinition = Data->FindClass(DBBridge::ToFName(Record.ClassId));
	if (!ClassDefinition)
	{
		OutError = TEXT("Unbekannte Klasse.");
		return false;
	}

	R::FRecordValidationRules Rules;
	const R::FSkillTreeDefinition Tree = ClassDefinition->BuildSkillTree();
	Rules.SkillTree = &Tree;
	const std::vector<std::string> Issues = R::ValidateCharacterRecord(Record, Data->GetItemCatalog(), Rules);
	if (!Issues.empty())
	{
		for (const std::string& Issue : Issues)
		{
			UE_LOG(LogDBSave, Warning, TEXT("Character validation: %s"), *DBBridge::ToFString(Issue));
		}
		OutError = DBBridge::ToFString(Issues.front());
		return false;
	}

	// The same character must never be in the session twice (item duplication).
	for (const APlayerState* Other : GameState->PlayerArray)
	{
		const ADBPlayerState* OtherState = Cast<ADBPlayerState>(Other);
		if (OtherState && OtherState != PlayerState && OtherState->IsCharacterReady() &&
			DBBridge::ToStd(OtherState->GetProfile().CharacterId.ToString(EGuidFormats::DigitsWithHyphens)) == Record.CharacterId)
		{
			OutError = TEXT("Dieser Charakter ist bereits in der Sitzung.");
			return false;
		}
	}

	PlayerState->ApplyCharacterRecord(Record);
	if (!Controller->GetPawn() && PlayerCanRestart(Controller))
	{
		RestartPlayer(Controller);
	}
	return true;
}

bool ADBGameMode::CreateServerCharacter(ADBPlayerController* Controller, const FString& Name, FName ClassId, const FDBAppearance& Appearance,
	FString& OutError)
{
	if (PersistenceMode != EDBPersistenceMode::ServerAuthoritative)
	{
		OutError = TEXT("Charaktere werden in diesem Modus lokal erstellt.");
		return false;
	}
	UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
	const FString SlotKey = GetServerSlotKey(Controller);
	if (!Saves || Saves->DoesCharacterExist(SlotKey))
	{
		OutError = TEXT("Charakter existiert bereits.");
		return false;
	}
	R::FCharacterRecord Record;
	if (!Saves->CreateCharacter(SlotKey, Name, ClassId, Appearance, Record, OutError))
	{
		return false;
	}
	return AcceptCharacterRecord(Controller, Record, OutError);
}

void ADBGameMode::SaveCharacter(ADBPlayerState* PlayerState)
{
	UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
	if (!Saves || !IsValid(PlayerState) || !PlayerState->IsCharacterReady())
	{
		return;
	}
	const R::FCharacterRecord Record = PlayerState->BuildCharacterRecord();
	APlayerController* Controller = PlayerState->GetPlayerController();

	if (PersistenceMode == EDBPersistenceMode::ServerAuthoritative)
	{
		Saves->SaveCharacter(GetServerSlotKey(Controller), Record);
	}
	else if (Controller && Controller->IsLocalController())
	{
		Saves->SaveCharacter(Saves->GetActiveCharacterSlot(), Record);
	}
	else if (ADBPlayerController* Remote = Cast<ADBPlayerController>(Controller))
	{
		Remote->ClientStoreCharacterSnapshot(DBBridge::ToArray(R::SerializeCharacter(Record)));
	}
}

void ADBGameMode::SaveWorld()
{
	UDBSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UDBSaveSubsystem>();
	const ADBGameState* DBGameState = GetGameState<ADBGameState>();
	if (!Saves || !DBGameState)
	{
		return;
	}
	R::FWorldRecord Record;
	Record.WorldId = DBBridge::ToStd(WorldId);
	Record.World = DBGameState->GetWorldState()->GetRulesState();
	// The shared quest component owns the authoritative story quest log.
	Record.World.SharedQuests = DBGameState->GetSharedQuests()->GetLog();
	Saves->SaveWorld(WorldSlot, Record);
}

void ADBGameMode::SaveAll()
{
	if (!GameState)
	{
		return;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		SaveCharacter(Cast<ADBPlayerState>(PlayerState));
	}
	SaveWorld();
}

void ADBGameMode::Logout(AController* Exiting)
{
	if (const APlayerController* Controller = Cast<APlayerController>(Exiting))
	{
		SaveCharacter(Controller->GetPlayerState<ADBPlayerState>());
		RequestedCharacterIndex.Remove(const_cast<APlayerController*>(Controller));
	}
	Super::Logout(Exiting);
}

bool ADBGameMode::AllowCheats(APlayerController* P)
{
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("DBCheats")))
	{
		return true;
	}
#endif
	return Super::AllowCheats(P);
}

void ADBGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	if (ADBCharacterBase* Character = Cast<ADBCharacterBase>(PlayerPawn))
	{
		Character->OnDied.AddUniqueDynamic(this, &ADBGameMode::HandleCharacterDied);
	}
}

void ADBGameMode::HandleCharacterDied(ADBCharacterBase* Character)
{
	AController* Controller = Character ? Character->GetController() : nullptr;
	const ADBPlayerState* PlayerState = Character ? Character->GetPlayerState<ADBPlayerState>() : nullptr;
	if (!Controller || !PlayerState)
	{
		return;
	}

	// Items are kept (they live on the PlayerState); only the moderate penalty applies.
	const int64 CurrencyLost = PlayerState->GetInventory()->ApplyDeathPenalty();
	UE_LOG(LogDarkBlood, Log, TEXT("%s died (lost %lld Mon)"), *PlayerState->GetPlayerName(), CurrencyLost);

	FTimerHandle RespawnTimer;
	const TWeakObjectPtr<AController> WeakController = Controller;
	GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
	{
		if (WeakController.IsValid())
		{
			RespawnPlayer(WeakController.Get());
		}
	}), FMath::Max(0.1f, UDBGameSettings::Get().RespawnDelaySeconds), false);
}

void ADBGameMode::RespawnPlayer(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}
	// The cached first start spot would win over the rest point (ChoosePlayerStart): forget it.
	Controller->StartSpot = nullptr;
	RestartPlayer(Controller);
	if (const ADBPlayerState* PlayerState = Controller->GetPlayerState<ADBPlayerState>())
	{
		PlayerState->GetProgression()->RecalculateAttributes(true);
		UE_LOG(LogDarkBlood, Log, TEXT("%s respawned"), *PlayerState->GetPlayerName());
	}
}

AActor* ADBGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Respawn at the last rest point (tavern, camp ...) if a matching PlayerStart exists.
	const ADBPlayerState* PlayerState = Player ? Player->GetPlayerState<ADBPlayerState>() : nullptr;
	if (PlayerState && !PlayerState->GetRespawnPointId().IsNone())
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == PlayerState->GetRespawnPointId())
			{
				return *It;
			}
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
