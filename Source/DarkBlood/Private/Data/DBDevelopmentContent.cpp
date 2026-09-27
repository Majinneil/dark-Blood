#include "Data/DBDevelopmentContent.h"

#include "Abilities/DBAbilitySet.h"
#include "Abilities/DBCombatAbilities.h"
#include "Abilities/DBMeleeAttackAbility.h"
#include "Core/DBGameplayTags.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBItemDefinition.h"
#include "Data/DBQuestDefinition.h"
#include "Data/DBRegionDefinition.h"

namespace
{
	FDBStatBlock Stats(float Str, float Dex, float Int, float Spi, float Vit, float End)
	{
		FDBStatBlock Block;
		Block.Strength = Str;
		Block.Dexterity = Dex;
		Block.Intelligence = Int;
		Block.Spirit = Spi;
		Block.Vitality = Vit;
		Block.Endurance = End;
		return Block;
	}

	FDBItemGrant Grant(const TCHAR* ItemId, int32 Count)
	{
		FDBItemGrant Result;
		Result.ItemId = ItemId;
		Result.Count = Count;
		return Result;
	}

	int32 CategoryMask(std::initializer_list<EDBItemCategory> Categories)
	{
		int32 Mask = 0;
		for (const EDBItemCategory Category : Categories)
		{
			Mask |= 1 << static_cast<int32>(Category);
		}
		return Mask;
	}

	/** DEVELOPMENT moveset shared by all classes until class kits exist (Phase 4). */
	UDBAbilitySet* CreateDevelopmentMoveset(UObject* Outer)
	{
		UDBAbilitySet* Set = NewObject<UDBAbilitySet>(Outer, NAME_None, RF_Transient);
		auto Add = [Set](TSubclassOf<UDBGameplayAbility> Ability, const FGameplayTag& InputTag)
		{
			FDBAbilitySetAbility Entry;
			Entry.Ability = Ability;
			Entry.InputTag = InputTag;
			Set->GrantedAbilities.Add(Entry);
		};
		Add(UDBAbility_LightCombo::StaticClass(), DBTags::Input_LightAttack);
		Add(UDBAbility_HeavyAttack::StaticClass(), DBTags::Input_HeavyAttack);
		Add(UDBAbility_Dodge::StaticClass(), DBTags::Input_Dodge);
		Add(UDBAbility_Block::StaticClass(), DBTags::Input_Block);
		Add(UDBAbility_Sprint::StaticClass(), DBTags::Input_Sprint);
		Add(UDBAbility_HitReact::StaticClass(), FGameplayTag()); // triggered by gameplay events
		return Set;
	}
}

bool FDBDevelopmentContent::RegisterMissing(UDBGameDataSubsystem& Data)
{
	bool bAddedAny = false;

	// ---- Classes --------------------------------------------------------------------------------
	UDBAbilitySet* DevMoveset = nullptr;
	auto AddClass = [&](FName Id, const FGameplayTag& Tag, const TCHAR* Name, const FDBStatBlock& Base, const FDBStatBlock& PerLevel,
		TArray<FDBItemGrant> StartItems)
	{
		if (Data.FindClass(Id))
		{
			return;
		}
		UDBClassDefinition* Def = NewObject<UDBClassDefinition>(&Data, NAME_None, RF_Transient);
		Def->ClassId = Id;
		Def->ClassTag = Tag;
		Def->DisplayName = FText::FromString(FString(Name) + TEXT(" [DEV]"));
		Def->BaseStats = Base;
		Def->StatsPerLevel = PerLevel;
		Def->StartingItems = MoveTemp(StartItems);
		if (!DevMoveset)
		{
			DevMoveset = CreateDevelopmentMoveset(&Data);
		}
		Def->BaseAbilitySet = DevMoveset;
		Data.RegisterClass(Def);
		bAddedAny = true;
	};

	AddClass(TEXT("Warrior"), DBTags::Class_Warrior, TEXT("Krieger"), Stats(14, 8, 3, 5, 14, 12), Stats(2.0f, 0.8f, 0.2f, 0.5f, 2.0f, 1.5f),
		{Grant(TEXT("Katana_Dev"), 1), Grant(TEXT("RiceBall"), 5)});
	AddClass(TEXT("Shadowrunner"), DBTags::Class_Shadowrunner, TEXT("Schattenlaeufer"), Stats(8, 15, 5, 6, 9, 14),
		Stats(0.9f, 2.2f, 0.4f, 0.6f, 1.2f, 1.8f), {Grant(TEXT("Kunai_Dev"), 1), Grant(TEXT("RiceBall"), 5)});
	AddClass(TEXT("Mage"), DBTags::Class_Mage, TEXT("Magier"), Stats(3, 6, 16, 12, 8, 8), Stats(0.2f, 0.6f, 2.4f, 1.6f, 1.0f, 1.0f),
		{Grant(TEXT("Staff_Dev"), 1), Grant(TEXT("RiceBall"), 5)});
	AddClass(TEXT("Monk"), DBTags::Class_Monk, TEXT("Moench"), Stats(11, 12, 4, 12, 11, 13), Stats(1.4f, 1.4f, 0.3f, 1.4f, 1.4f, 1.6f),
		{Grant(TEXT("Handwraps_Dev"), 1), Grant(TEXT("RiceBall"), 5)});

	// ---- Items ----------------------------------------------------------------------------------
	auto AddItem = [&](FName Id, const TCHAR* Name, EDBItemCategory Category, int32 MaxStack, TFunctionRef<void(UDBItemDefinition&)> Setup)
	{
		if (Data.FindItem(Id))
		{
			return;
		}
		UDBItemDefinition* Def = NewObject<UDBItemDefinition>(&Data, NAME_None, RF_Transient);
		Def->ItemId = Id;
		Def->DisplayName = FText::FromString(FString(Name) + TEXT(" [DEV]"));
		Def->Category = Category;
		Def->MaxStack = MaxStack;
		Def->bUsesPlaceholderVisuals = true;
		Setup(*Def);
		Data.RegisterItem(Def);
		bAddedAny = true;
	};

	auto Weapon = [&](FName Id, const TCHAR* Name, FName ClassId)
	{
		AddItem(Id, Name, EDBItemCategory::Weapon, 1, [ClassId](UDBItemDefinition& D)
		{
			D.EquipSlot = EDBEquipSlot::MainHand;
			D.ItemLevel = 1;
			D.MaxDurability = 100;
			D.AllowedClasses = {ClassId};
		});
	};
	Weapon(TEXT("Katana_Dev"), TEXT("Uebungskatana"), TEXT("Warrior"));
	Weapon(TEXT("Kunai_Dev"), TEXT("Uebungskunai"), TEXT("Shadowrunner"));
	Weapon(TEXT("Staff_Dev"), TEXT("Uebungsstab"), TEXT("Mage"));
	Weapon(TEXT("Handwraps_Dev"), TEXT("Faustbandagen"), TEXT("Monk"));

	AddItem(TEXT("Tamahagane"), TEXT("Tamahagane"), EDBItemCategory::Material, 99, [](UDBItemDefinition&) {});
	AddItem(TEXT("DemonOre"), TEXT("Daemonenerz"), EDBItemCategory::Material, 99, [](UDBItemDefinition& D) { D.Rarity = EDBItemRarity::Rare; });
	AddItem(TEXT("RiceBall"), TEXT("Onigiri"), EDBItemCategory::Food, 20, [](UDBItemDefinition&) {});
	AddItem(TEXT("KingsSeal"), TEXT("Siegel des Koenigs"), EDBItemCategory::Quest, 1, [](UDBItemDefinition&) {});

	auto Bag = [&](FName Id, const TCHAR* Name, EDBBagKind Kind, int32 Capacity, int32 Mask)
	{
		AddItem(Id, Name, EDBItemCategory::Bag, 1, [Kind, Capacity, Mask](UDBItemDefinition& D)
		{
			D.bIsBag = true;
			D.BagKind = Kind;
			D.BagCapacity = Capacity;
			D.BagAcceptedCategories = Mask;
		});
	};
	Bag(TEXT("Bag_Small"), TEXT("Kleine Reisetasche"), EDBBagKind::General, 9, 0);
	Bag(TEXT("Bag_Adventurer"), TEXT("Abenteurertasche"), EDBBagKind::General, 18, 0);
	Bag(TEXT("Bag_LargeBackpack"), TEXT("Grosser Reiserucksack"), EDBBagKind::General, 27, 0);
	Bag(TEXT("Bag_Materials"), TEXT("Materialtasche"), EDBBagKind::Materials, 12, CategoryMask({EDBItemCategory::Material}));
	Bag(TEXT("Bag_Provisions"), TEXT("Provianttasche"), EDBBagKind::Provisions, 12, CategoryMask({EDBItemCategory::Food, EDBItemCategory::Potion}));
	Bag(TEXT("Bag_Scrolls"), TEXT("Schriftrollentasche"), EDBBagKind::Scrolls, 12, CategoryMask({EDBItemCategory::Scroll, EDBItemCategory::Magic}));
	Bag(TEXT("Bag_Loot"), TEXT("Beutetasche"), EDBBagKind::Loot, 12,
		CategoryMask({EDBItemCategory::Weapon, EDBItemCategory::Armor, EDBItemCategory::Misc}));

	// ---- Regions (working titles from the master plan) ------------------------------------------
	auto AddRegion = [&](FName Id, const TCHAR* Name, const TCHAR* Theme, EDBRegionKind Kind, int32 Min, int32 Max)
	{
		if (Data.FindRegion(Id))
		{
			return;
		}
		UDBRegionDefinition* Def = NewObject<UDBRegionDefinition>(&Data, NAME_None, RF_Transient);
		Def->RegionId = Id;
		Def->DisplayName = FText::FromString(FString(Name) + TEXT(" [DEV]"));
		Def->Theme = FText::FromString(Theme);
		Def->Kind = Kind;
		Def->RecommendedPowerMin = Min;
		Def->RecommendedPowerMax = Max;
		if (Kind == EDBRegionKind::VassalRegion)
		{
			Def->VassalBossId = FName(*FString::Printf(TEXT("Vassal_%s"), *Id.ToString()));
			Def->MidBossId = FName(*FString::Printf(TEXT("MidBoss_%s"), *Id.ToString()));
		}
		Data.RegisterRegion(Def);
		bAddedAny = true;
	};

	AddRegion(TEXT("Capital"), TEXT("Hauptstadt"), TEXT("Schutzzauber"), EDBRegionKind::Capital, 1, 5);
	const TCHAR* Themes[] = {TEXT("Blut"), TEXT("Frost"), TEXT("Schatten"), TEXT("Donner"), TEXT("Feuer / Asche"), TEXT("Knochen / Tod"),
		TEXT("Gift / Seuche"), TEXT("Wahnsinn / Illusion"), TEXT("Bestien"), TEXT("Eisen"), TEXT("Meer"), TEXT("Wind / Himmel"),
		TEXT("Finsternis"), TEXT("Schwarze Festung")};
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Themes)); ++Index)
	{
		const int32 Number = Index + 1;
		const int32 Min = 4 * Number - 1;
		AddRegion(FName(*FString::Printf(TEXT("Region%02d"), Number)), *FString::Printf(TEXT("Vasallengebiet %d"), Number), Themes[Index],
			EDBRegionKind::VassalRegion, Min, Min + 6);
	}
	AddRegion(TEXT("TheEnd"), TEXT("Das Ende"), TEXT("Dunkles Blut"), EDBRegionKind::FinalRegion, 62, 70);
	AddRegion(TEXT("Paradise"), TEXT("Das Paradies"), TEXT("Epilog"), EDBRegionKind::Epilogue, 1, 100);

	// ---- Quests ---------------------------------------------------------------------------------
	if (!Data.FindQuest(TEXT("MQ01_KingsSummons")))
	{
		UDBQuestDefinition* Quest = NewObject<UDBQuestDefinition>(&Data, NAME_None, RF_Transient);
		Quest->QuestId = TEXT("MQ01_KingsSummons");
		Quest->Title = FText::FromString(TEXT("Der Ruf des Koenigs [DEV]"));
		Quest->Category = EDBQuestCategory::Main;
		Quest->Scope = EDBQuestScope::Shared;
		Quest->RegionId = TEXT("Capital");
		Quest->bSequential = true;
		FDBQuestObjective Talk;
		Talk.ObjectiveId = TEXT("TalkToKing");
		Talk.Kind = EDBObjectiveKind::Talk;
		Talk.Target = TEXT("NPC_King");
		Quest->Objectives.Add(Talk);
		FDBQuestObjective Train;
		Train.ObjectiveId = TEXT("DefeatTrainingDummies");
		Train.Kind = EDBObjectiveKind::Kill;
		Train.Target = TEXT("TrainingDummy");
		Train.Required = 3;
		Quest->Objectives.Add(Train);
		Quest->Reward.Xp = 250;
		Quest->Reward.Currency = 100;
		Quest->Reward.SkillPoints = 1;
		Quest->Reward.StoryFlags = {TEXT("Story.KingsSummonsDone")};
		Quest->Reward.Items = {Grant(TEXT("Bag_Small"), 1)};
		Data.RegisterQuest(Quest);
		bAddedAny = true;
	}

	return bAddedAny;
}
