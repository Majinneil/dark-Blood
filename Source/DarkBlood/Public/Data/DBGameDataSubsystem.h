// Central registry of gameplay data assets and the rules-core views built from them.
#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "DarkBloodRules/Items.h"
#include "DarkBloodRules/Quest.h"

#include "DBGameDataSubsystem.generated.h"

class UDBClassDefinition;
class UDBDialogueDefinition;
class UDBItemDefinition;
class UDBQuestDefinition;
class UDBRegionDefinition;

UCLASS()
class DARKBLOOD_API UDBGameDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDBGameDataSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Registers a definition at runtime (development content, tests, mods). Replaces entries with the same id. */
	void RegisterClass(UDBClassDefinition* Definition);
	void RegisterItem(UDBItemDefinition* Definition);
	void RegisterQuest(UDBQuestDefinition* Definition);
	void RegisterRegion(UDBRegionDefinition* Definition);
	void RegisterDialogue(UDBDialogueDefinition* Definition);

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Data")
	UDBClassDefinition* FindClass(FName ClassId) const;

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Data")
	UDBItemDefinition* FindItem(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Data")
	UDBQuestDefinition* FindQuest(FName QuestId) const;

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Data")
	UDBRegionDefinition* FindRegion(FName RegionId) const;

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Data")
	UDBDialogueDefinition* FindDialogue(FName DialogueId) const;

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Data")
	TArray<UDBClassDefinition*> GetAllClasses() const;

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Data")
	TArray<UDBRegionDefinition*> GetAllRegions() const;

	const DarkBlood::Rules::FItemCatalog& GetItemCatalog() const { return ItemCatalog; }
	const DarkBlood::Rules::FQuestDatabase& GetQuestDatabase() const { return QuestDatabase; }

	/** True when code-generated development content was registered because assets were missing. */
	bool IsUsingDevelopmentContent() const { return bUsingDevelopmentContent; }

private:
	template <typename TDefinition>
	void LoadAllOfType(const FPrimaryAssetType& Type, TFunctionRef<void(TDefinition*)> Register);

	void RebuildItemCatalog();
	void RebuildQuestDatabase();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDBClassDefinition>> Classes;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDBItemDefinition>> Items;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDBQuestDefinition>> Quests;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDBRegionDefinition>> Regions;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UDBDialogueDefinition>> Dialogues;

	DarkBlood::Rules::FItemCatalog ItemCatalog;
	DarkBlood::Rules::FQuestDatabase QuestDatabase;
	bool bUsingDevelopmentContent = false;
};
