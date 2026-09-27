#include "Data/DBGameDataSubsystem.h"

#include "DarkBlood.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBDevelopmentContent.h"
#include "Data/DBItemDefinition.h"
#include "Data/DBQuestDefinition.h"
#include "Data/DBRegionDefinition.h"

#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UDBGameDataSubsystem* UDBGameDataSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UDBGameDataSubsystem>() : nullptr;
}

template <typename TDefinition>
void UDBGameDataSubsystem::LoadAllOfType(const FPrimaryAssetType& Type, TFunctionRef<void(TDefinition*)> Register)
{
	if (!UAssetManager::IsInitialized())
	{
		return;
	}
	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> Ids;
	AssetManager.GetPrimaryAssetIdList(Type, Ids);
	for (const FPrimaryAssetId& Id : Ids)
	{
		// Gameplay data is small; synchronous loading keeps Phase 1 simple. Visuals stay soft references.
		if (TDefinition* Definition = Cast<TDefinition>(AssetManager.GetPrimaryAssetPath(Id).TryLoad()))
		{
			Register(Definition);
		}
		else
		{
			UE_LOG(LogDarkBlood, Warning, TEXT("Could not load %s"), *Id.ToString());
		}
	}
}

void UDBGameDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadAllOfType<UDBClassDefinition>(UDBClassDefinition::AssetType, [this](UDBClassDefinition* D) { RegisterClass(D); });
	LoadAllOfType<UDBItemDefinition>(UDBItemDefinition::AssetType, [this](UDBItemDefinition* D) { RegisterItem(D); });
	LoadAllOfType<UDBQuestDefinition>(UDBQuestDefinition::AssetType, [this](UDBQuestDefinition* D) { RegisterQuest(D); });
	LoadAllOfType<UDBRegionDefinition>(UDBRegionDefinition::AssetType, [this](UDBRegionDefinition* D) { RegisterRegion(D); });

	// Missing content is filled with clearly marked development definitions so the game stays runnable.
	bUsingDevelopmentContent = FDBDevelopmentContent::RegisterMissing(*this);

	UE_LOG(LogDarkBlood, Log, TEXT("Game data: %d classes, %d items, %d quests, %d regions%s"), Classes.Num(), Items.Num(),
		Quests.Num(), Regions.Num(), bUsingDevelopmentContent ? TEXT(" (includes DEVELOPMENT content)") : TEXT(""));
}

void UDBGameDataSubsystem::RegisterClass(UDBClassDefinition* Definition)
{
	if (Definition && !Definition->ClassId.IsNone())
	{
		Classes.Add(Definition->ClassId, Definition);
	}
}

void UDBGameDataSubsystem::RegisterItem(UDBItemDefinition* Definition)
{
	if (Definition && !Definition->ItemId.IsNone())
	{
		Items.Add(Definition->ItemId, Definition);
		RebuildItemCatalog();
	}
}

void UDBGameDataSubsystem::RegisterQuest(UDBQuestDefinition* Definition)
{
	if (Definition && !Definition->QuestId.IsNone())
	{
		Quests.Add(Definition->QuestId, Definition);
		RebuildQuestDatabase();
	}
}

void UDBGameDataSubsystem::RegisterRegion(UDBRegionDefinition* Definition)
{
	if (Definition && !Definition->RegionId.IsNone())
	{
		Regions.Add(Definition->RegionId, Definition);
	}
}

void UDBGameDataSubsystem::RebuildItemCatalog()
{
	ItemCatalog = DarkBlood::Rules::FItemCatalog();
	for (const TPair<FName, TObjectPtr<UDBItemDefinition>>& Pair : Items)
	{
		if (!ItemCatalog.Add(Pair.Value->ToRules()))
		{
			UE_LOG(LogDarkBlood, Error, TEXT("Invalid item definition %s"), *Pair.Key.ToString());
		}
	}
}

void UDBGameDataSubsystem::RebuildQuestDatabase()
{
	QuestDatabase = DarkBlood::Rules::FQuestDatabase();
	for (const TPair<FName, TObjectPtr<UDBQuestDefinition>>& Pair : Quests)
	{
		if (!QuestDatabase.Add(Pair.Value->ToRules()))
		{
			UE_LOG(LogDBQuest, Error, TEXT("Invalid quest definition %s"), *Pair.Key.ToString());
		}
	}
}

UDBClassDefinition* UDBGameDataSubsystem::FindClass(FName ClassId) const
{
	const TObjectPtr<UDBClassDefinition>* Found = Classes.Find(ClassId);
	return Found ? Found->Get() : nullptr;
}

UDBItemDefinition* UDBGameDataSubsystem::FindItem(FName ItemId) const
{
	const TObjectPtr<UDBItemDefinition>* Found = Items.Find(ItemId);
	return Found ? Found->Get() : nullptr;
}

UDBQuestDefinition* UDBGameDataSubsystem::FindQuest(FName QuestId) const
{
	const TObjectPtr<UDBQuestDefinition>* Found = Quests.Find(QuestId);
	return Found ? Found->Get() : nullptr;
}

UDBRegionDefinition* UDBGameDataSubsystem::FindRegion(FName RegionId) const
{
	const TObjectPtr<UDBRegionDefinition>* Found = Regions.Find(RegionId);
	return Found ? Found->Get() : nullptr;
}

TArray<UDBClassDefinition*> UDBGameDataSubsystem::GetAllClasses() const
{
	TArray<UDBClassDefinition*> Result;
	for (const TPair<FName, TObjectPtr<UDBClassDefinition>>& Pair : Classes)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

TArray<UDBRegionDefinition*> UDBGameDataSubsystem::GetAllRegions() const
{
	TArray<UDBRegionDefinition*> Result;
	for (const TPair<FName, TObjectPtr<UDBRegionDefinition>>& Pair : Regions)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}
