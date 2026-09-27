#include "Save/DBSaveSubsystem.h"

#include "Core/DBGameSettings.h"
#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBItemDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Save/DBSaveGames.h"

namespace R = DarkBlood::Rules;

void UDBSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UDBGameDataSubsystem>();
	Super::Initialize(Collection);

	FString Slot;
	if (FParse::Value(FCommandLine::Get(), TEXT("DBCharacterSlot="), Slot) && !Slot.IsEmpty())
	{
		ActiveCharacterSlot = Slot;
	}
}

FString UDBSaveSubsystem::GetDevelopmentCharacterName() const
{
	FString Name;
	if (FParse::Value(FCommandLine::Get(), TEXT("DBCharacterName="), Name) && !Name.IsEmpty())
	{
		return Name;
	}
	return TEXT("Wanderer");
}

FString UDBSaveSubsystem::MakeServerSlotKey(const FString& PlayerId, const FString& CharacterIndex)
{
	// Hash the platform id: slot names must be file-system safe and should not leak account ids.
	const uint32 Hash = GetTypeHash(PlayerId);
	return FString::Printf(TEXT("Srv_%08x_%s"), Hash, CharacterIndex.IsEmpty() ? TEXT("0") : *CharacterIndex);
}

bool UDBSaveSubsystem::BuildNewCharacter(const FString& Name, FName ClassId, const FDBAppearance& Appearance, R::FCharacterRecord& OutRecord,
	FString& OutError) const
{
	const UDBGameDataSubsystem* Data = GetGameInstance()->GetSubsystem<UDBGameDataSubsystem>();
	const UDBClassDefinition* ClassDefinition = Data ? Data->FindClass(ClassId) : nullptr;
	if (!ClassDefinition)
	{
		OutError = FString::Printf(TEXT("Unbekannte Klasse '%s'."), *ClassId.ToString());
		return false;
	}

	const std::string NormalizedName = R::NormalizeCharacterName(DBBridge::ToStd(Name));
	const R::ENameValidation NameResult = R::ValidateCharacterName(NormalizedName);
	if (NameResult != R::ENameValidation::Ok)
	{
		OutError = FString::Printf(TEXT("Ungueltiger Name (%hs)."), R::ToString(NameResult));
		return false;
	}

	R::FCharacterRecord Record = R::MakeNewCharacter(DBBridge::ToStd(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens)),
		NormalizedName, DBBridge::ToStd(ClassId), DBBridge::ToRules(Appearance));
	Record.Inventory = R::FInventory(UDBGameSettings::Get().BasePouchCapacity);
	Record.Currency = ClassDefinition->StartingCurrency;

	const R::FItemCatalog& Catalog = Data->GetItemCatalog();
	for (const FDBItemGrant& Grant : ClassDefinition->StartingItems)
	{
		const R::FItemDefinition* Item = Catalog.Find(DBBridge::ToStd(Grant.ItemId));
		if (!Item)
		{
			UE_LOG(LogDBSave, Warning, TEXT("Starting item %s of class %s does not exist"), *Grant.ItemId.ToString(), *ClassId.ToString());
			continue;
		}
		const int32 Copies = Item->MaxDurability > 0 ? Grant.Count : 1;
		for (int32 Copy = 0; Copy < Copies; ++Copy)
		{
			const R::FItemStack Stack = Item->MaxDurability > 0
				? DBBridge::MakeStack(Grant.ItemId, 1, DBBridge::NewInstanceId(), Item->MaxDurability)
				: DBBridge::MakeStack(Grant.ItemId, Grant.Count);
			Record.Inventory.Add(Catalog, Stack);
		}
	}

	R::FRecordValidationRules Rules;
	const R::FSkillTreeDefinition Tree = ClassDefinition->BuildSkillTree();
	Rules.SkillTree = &Tree;
	const std::vector<std::string> Issues = R::ValidateCharacterRecord(Record, Catalog, Rules);
	if (!Issues.empty())
	{
		OutError = DBBridge::ToFString(Issues.front());
		return false;
	}
	OutRecord = MoveTemp(Record);
	return true;
}

bool UDBSaveSubsystem::CreateCharacter(const FString& SlotKey, const FString& Name, FName ClassId, const FDBAppearance& Appearance,
	R::FCharacterRecord& OutRecord, FString& OutError)
{
	if (!BuildNewCharacter(Name, ClassId, Appearance, OutRecord, OutError))
	{
		return false;
	}
	if (!SaveCharacter(SlotKey, OutRecord))
	{
		OutError = TEXT("Speichern fehlgeschlagen.");
		return false;
	}
	return true;
}

bool UDBSaveSubsystem::SaveCharacter(const FString& SlotKey, const R::FCharacterRecord& Record)
{
	UDBCharacterSaveGame* Save = Cast<UDBCharacterSaveGame>(UGameplayStatics::CreateSaveGameObject(UDBCharacterSaveGame::StaticClass()));
	FGuid::Parse(DBBridge::ToFString(Record.CharacterId), Save->CharacterId);
	Save->CharacterName = DBBridge::ToFString(Record.Name);
	Save->ClassId = DBBridge::ToFName(Record.ClassId);
	Save->Level = Record.Progression.Level;
	Save->SavedAtUtc = FDateTime::UtcNow();
	Save->RecordData = DBBridge::ToArray(R::SerializeCharacter(Record));

	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, CharacterSaveName(SlotKey), 0);
	if (bSaved)
	{
		AddToIndex(SlotKey);
	}
	UE_LOG(LogDBSave, Log, TEXT("Save character '%s' -> %s: %s"), *Save->CharacterName, *SlotKey, bSaved ? TEXT("ok") : TEXT("FAILED"));
	return bSaved;
}

R::ELoadResult UDBSaveSubsystem::LoadCharacter(const FString& SlotKey, R::FCharacterRecord& OutRecord) const
{
	const UDBCharacterSaveGame* Save = Cast<UDBCharacterSaveGame>(UGameplayStatics::LoadGameFromSlot(CharacterSaveName(SlotKey), 0));
	if (!Save)
	{
		return R::ELoadResult::BadMagic;
	}
	const R::ELoadResult Result = R::DeserializeCharacter(Save->RecordData.GetData(), Save->RecordData.Num(), OutRecord);
	if (Result != R::ELoadResult::Ok)
	{
		UE_LOG(LogDBSave, Error, TEXT("Character slot %s could not be loaded: %hs"), *SlotKey, R::ToString(Result));
	}
	return Result;
}

bool UDBSaveSubsystem::DoesCharacterExist(const FString& SlotKey) const
{
	return UGameplayStatics::DoesSaveGameExist(CharacterSaveName(SlotKey), 0);
}

void UDBSaveSubsystem::AddToIndex(const FString& SlotKey)
{
	UDBCharacterIndexSaveGame* Index = Cast<UDBCharacterIndexSaveGame>(UGameplayStatics::LoadGameFromSlot(IndexSaveName(), 0));
	if (!Index)
	{
		Index = Cast<UDBCharacterIndexSaveGame>(UGameplayStatics::CreateSaveGameObject(UDBCharacterIndexSaveGame::StaticClass()));
	}
	if (!Index->CharacterSlots.Contains(SlotKey))
	{
		Index->CharacterSlots.Add(SlotKey);
		UGameplayStatics::SaveGameToSlot(Index, IndexSaveName(), 0);
	}
}

TArray<FDBCharacterSlotInfo> UDBSaveSubsystem::ListCharacters() const
{
	TArray<FDBCharacterSlotInfo> Result;
	const UDBCharacterIndexSaveGame* Index = Cast<UDBCharacterIndexSaveGame>(UGameplayStatics::LoadGameFromSlot(IndexSaveName(), 0));
	if (!Index)
	{
		return Result;
	}
	for (const FString& SlotKey : Index->CharacterSlots)
	{
		if (const UDBCharacterSaveGame* Save = Cast<UDBCharacterSaveGame>(UGameplayStatics::LoadGameFromSlot(CharacterSaveName(SlotKey), 0)))
		{
			FDBCharacterSlotInfo& Info = Result.AddDefaulted_GetRef();
			Info.SlotKey = SlotKey;
			Info.CharacterName = Save->CharacterName;
			Info.ClassId = Save->ClassId;
			Info.Level = Save->Level;
			Info.SavedAtUtc = Save->SavedAtUtc;
		}
	}
	return Result;
}

bool UDBSaveSubsystem::LoadOrCreateActiveCharacter(R::FCharacterRecord& OutRecord)
{
	if (DoesCharacterExist(ActiveCharacterSlot))
	{
		return LoadCharacter(ActiveCharacterSlot, OutRecord) == R::ELoadResult::Ok;
	}
	// Phase 3 replaces this with the character creator.
	FString Error;
	const FString Name = GetDevelopmentCharacterName();
	UE_LOG(LogDBSave, Warning, TEXT("No character in slot %s - creating DEVELOPMENT character '%s'"), *ActiveCharacterSlot, *Name);
	if (!CreateCharacter(ActiveCharacterSlot, Name, UDBGameSettings::Get().DevelopmentDefaultClass, FDBAppearance(), OutRecord, Error))
	{
		UE_LOG(LogDBSave, Error, TEXT("Development character creation failed: %s"), *Error);
		return false;
	}
	return true;
}

bool UDBSaveSubsystem::LoadOrCreateActiveCharacterData(TArray<uint8>& OutData)
{
	R::FCharacterRecord Record;
	if (!LoadOrCreateActiveCharacter(Record))
	{
		return false;
	}
	OutData = DBBridge::ToArray(R::SerializeCharacter(Record));
	return true;
}

bool UDBSaveSubsystem::StoreActiveCharacterData(const TArray<uint8>& Data)
{
	R::FCharacterRecord Record;
	const R::ELoadResult Result = R::DeserializeCharacter(Data.GetData(), Data.Num(), Record);
	if (Result != R::ELoadResult::Ok)
	{
		UE_LOG(LogDBSave, Error, TEXT("Received invalid character snapshot: %hs"), R::ToString(Result));
		return false;
	}
	return SaveCharacter(ActiveCharacterSlot, Record);
}

bool UDBSaveSubsystem::SaveWorld(const FString& WorldSlot, const R::FWorldRecord& Record)
{
	UDBWorldSaveGame* Save = Cast<UDBWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UDBWorldSaveGame::StaticClass()));
	Save->WorldId = DBBridge::ToFString(Record.WorldId);
	Save->SavedAtUtc = FDateTime::UtcNow();
	Save->RecordData = DBBridge::ToArray(R::SerializeWorld(Record));
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, WorldSaveName(WorldSlot), 0);
	UE_LOG(LogDBSave, Log, TEXT("Save world -> %s: %s"), *WorldSlot, bSaved ? TEXT("ok") : TEXT("FAILED"));
	return bSaved;
}

R::ELoadResult UDBSaveSubsystem::LoadWorld(const FString& WorldSlot, R::FWorldRecord& OutRecord) const
{
	const UDBWorldSaveGame* Save = Cast<UDBWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(WorldSaveName(WorldSlot), 0));
	if (!Save)
	{
		return R::ELoadResult::BadMagic;
	}
	const R::ELoadResult Result = R::DeserializeWorld(Save->RecordData.GetData(), Save->RecordData.Num(), OutRecord);
	if (Result != R::ELoadResult::Ok)
	{
		UE_LOG(LogDBSave, Error, TEXT("World slot %s could not be loaded: %hs"), *WorldSlot, R::ToString(Result));
	}
	return Result;
}
