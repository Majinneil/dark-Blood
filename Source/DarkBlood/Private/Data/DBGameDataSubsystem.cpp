#include "Data/DBGameDataSubsystem.h"

#include "DarkBlood.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBDevelopmentContent.h"
#include "Data/DBItemDefinition.h"
#include "Data/DBDialogueDefinition.h"
#include "Data/DBEconomyDefinitions.h"
#include "Data/DBQuestDefinition.h"
#include "Data/DBRegionDefinition.h"
#include "Visual/DBAnimationSetDefinition.h"
#include "Visual/DBCharacterVisualDefinition.h"

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
	LoadAllOfType<UDBDialogueDefinition>(UDBDialogueDefinition::AssetType, [this](UDBDialogueDefinition* D) { RegisterDialogue(D); });
	LoadAllOfType<UDBRecipeDefinition>(UDBRecipeDefinition::AssetType, [this](UDBRecipeDefinition* D) { RegisterRecipe(D); });
	LoadAllOfType<UDBLootTableDefinition>(UDBLootTableDefinition::AssetType, [this](UDBLootTableDefinition* D) { RegisterLootTable(D); });
	LoadAllOfType<UDBAnimationSetDefinition>(UDBAnimationSetDefinition::AssetType, [this](UDBAnimationSetDefinition* D) { RegisterAnimationSet(D); });
	LoadAllOfType<UDBCharacterVisualDefinition>(UDBCharacterVisualDefinition::AssetType,
		[this](UDBCharacterVisualDefinition* D) { RegisterCharacterVisual(D); });

	// Missing content is filled with clearly marked development definitions so the game stays runnable.
	bUsingDevelopmentContent = FDBDevelopmentContent::RegisterMissing(*this);

	// Loot tables reference items; validate once everything is registered.
	for (const TPair<FName, TObjectPtr<UDBLootTableDefinition>>& Pair : LootTables)
	{
		if (!DarkBlood::Rules::ValidateLootTable(Pair.Value->ToRules(), ItemCatalog))
		{
			UE_LOG(LogDarkBlood, Error, TEXT("Loot table %s references unknown items or has invalid weights"), *Pair.Key.ToString());
		}
	}

	UE_LOG(LogDarkBlood, Log, TEXT("Game data: %d classes, %d items, %d quests, %d regions, %d dialogues%s"), Classes.Num(), Items.Num(),
		Quests.Num(), Regions.Num(), Dialogues.Num(), bUsingDevelopmentContent ? TEXT(" (includes DEVELOPMENT content)") : TEXT(""));
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

void UDBGameDataSubsystem::RegisterDialogue(UDBDialogueDefinition* Definition)
{
	if (!Definition || Definition->DialogueId.IsNone())
	{
		return;
	}
	const DarkBlood::Rules::EDialogueValidation Validation = DarkBlood::Rules::ValidateDialogue(Definition->ToRules());
	if (Validation != DarkBlood::Rules::EDialogueValidation::Ok)
	{
		UE_LOG(LogDBQuest, Error, TEXT("Invalid dialogue %s: %hs"), *Definition->DialogueId.ToString(), DarkBlood::Rules::ToString(Validation));
		return;
	}
	Dialogues.Add(Definition->DialogueId, Definition);
}

void UDBGameDataSubsystem::RegisterRecipe(UDBRecipeDefinition* Definition)
{
	if (Definition && !Definition->RecipeId.IsNone())
	{
		Recipes.Add(Definition->RecipeId, Definition);
	}
}

void UDBGameDataSubsystem::RegisterLootTable(UDBLootTableDefinition* Definition)
{
	if (Definition && !Definition->LootTableId.IsNone())
	{
		LootTables.Add(Definition->LootTableId, Definition);
	}
}

void UDBGameDataSubsystem::RegisterCharacterVisual(UDBCharacterVisualDefinition* Definition)
{
	if (Definition && !Definition->ProfileId.IsNone())
	{
		CharacterVisuals.Add(Definition->ProfileId, Definition);
	}
}

void UDBGameDataSubsystem::RegisterAnimationSet(UDBAnimationSetDefinition* Definition)
{
	if (Definition && !Definition->AnimationSetId.IsNone())
	{
		AnimationSets.Add(Definition->AnimationSetId, Definition);
	}
}

UDBCharacterVisualDefinition* UDBGameDataSubsystem::FindCharacterVisual(FName ProfileId) const
{
	const TObjectPtr<UDBCharacterVisualDefinition>* Found = CharacterVisuals.Find(ProfileId);
	return Found ? Found->Get() : nullptr;
}

UDBAnimationSetDefinition* UDBGameDataSubsystem::FindAnimationSet(FName AnimationSetId) const
{
	const TObjectPtr<UDBAnimationSetDefinition>* Found = AnimationSets.Find(AnimationSetId);
	return Found ? Found->Get() : nullptr;
}

TArray<UDBCharacterVisualDefinition*> UDBGameDataSubsystem::GetAllCharacterVisuals() const
{
	TArray<UDBCharacterVisualDefinition*> Result;
	for (const TPair<FName, TObjectPtr<UDBCharacterVisualDefinition>>& Pair : CharacterVisuals)
	{
		Result.Add(Pair.Value.Get());
	}
	return Result;
}

UDBRecipeDefinition* UDBGameDataSubsystem::FindRecipe(FName RecipeId) const
{
	const TObjectPtr<UDBRecipeDefinition>* Found = Recipes.Find(RecipeId);
	return Found ? Found->Get() : nullptr;
}

UDBLootTableDefinition* UDBGameDataSubsystem::FindLootTable(FName LootTableId) const
{
	const TObjectPtr<UDBLootTableDefinition>* Found = LootTables.Find(LootTableId);
	return Found ? Found->Get() : nullptr;
}

TArray<UDBRecipeDefinition*> UDBGameDataSubsystem::GetAllRecipes() const
{
	TArray<UDBRecipeDefinition*> Result;
	for (const TPair<FName, TObjectPtr<UDBRecipeDefinition>>& Pair : Recipes)
	{
		Result.Add(Pair.Value.Get());
	}
	Result.Sort([](const UDBRecipeDefinition& A, const UDBRecipeDefinition& B) { return A.RequiredLevel < B.RequiredLevel; });
	return Result;
}

UDBDialogueDefinition* UDBGameDataSubsystem::FindDialogue(FName DialogueId) const
{
	const TObjectPtr<UDBDialogueDefinition>* Found = Dialogues.Find(DialogueId);
	return Found ? Found->Get() : nullptr;
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
