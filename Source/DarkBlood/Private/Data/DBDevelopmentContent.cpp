#include "Data/DBDevelopmentContent.h"

#include "Abilities/DBAbilitySet.h"
#include "Abilities/DBCombatAbilities.h"
#include "Abilities/DBMeleeAttackAbility.h"
#include "Core/DBGameplayTags.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBDialogueDefinition.h"
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
		Add(UDBAbility_DoubleJump::StaticClass(), FGameplayTag()); // DEV: later unlocked through the skill tree
		return Set;
	}
}

bool FDBDevelopmentContent::RegisterMissing(UDBGameDataSubsystem& Data)
{
	bool bAddedAny = false;

	// ---- Classes --------------------------------------------------------------------------------
	UDBAbilitySet* DevMoveset = nullptr;
	auto AddClass = [&](FName Id, const FGameplayTag& Tag, const TCHAR* Name, const FDBStatBlock& Base, const FDBStatBlock& PerLevel,
		TArray<FDBItemGrant> StartItems, const TCHAR* Description)
	{
		if (Data.FindClass(Id))
		{
			return;
		}
		UDBClassDefinition* Def = NewObject<UDBClassDefinition>(&Data, NAME_None, RF_Transient);
		Def->ClassId = Id;
		Def->ClassTag = Tag;
		Def->DisplayName = FText::FromString(FString(Name) + TEXT(" [DEV]"));
		Def->Description = FText::FromString(Description);
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
		{Grant(TEXT("Katana_Dev"), 1), Grant(TEXT("RiceBall"), 5)},
		TEXT("Schwertkaempfer der Frontlinie. Viel Leben und Ausdauer, starke Haltungen, haelt Treffer aus, die andere umwerfen."));
	AddClass(TEXT("Shadowrunner"), DBTags::Class_Shadowrunner, TEXT("Schattenlaeufer"), Stats(8, 15, 5, 6, 9, 14),
		Stats(0.9f, 2.2f, 0.4f, 0.6f, 1.2f, 1.8f), {Grant(TEXT("Kunai_Dev"), 1), Grant(TEXT("RiceBall"), 5)},
		TEXT("Schneller Kaempfer aus den Schatten. Kunai, Schattenmal und Teleport, hohe Kritchance, wenig Ruestung."));
	AddClass(TEXT("Mage"), DBTags::Class_Mage, TEXT("Magier"), Stats(3, 6, 16, 12, 8, 8), Stats(0.2f, 0.6f, 2.4f, 1.6f, 1.0f, 1.0f),
		{Grant(TEXT("Staff_Dev"), 1), Grant(TEXT("RiceBall"), 5)},
		TEXT("Gelehrter der alten Zauber. Elementarmagie, Schutzkreis und Flug - maechtig auf Distanz, verletzlich im Nahkampf."));
	AddClass(TEXT("Monk"), DBTags::Class_Monk, TEXT("Moench"), Stats(11, 12, 4, 12, 11, 13), Stats(1.4f, 1.4f, 0.3f, 1.4f, 1.4f, 1.6f),
		{Grant(TEXT("Handwraps_Dev"), 1), Grant(TEXT("RiceBall"), 5)},
		TEXT("Kriegermoench mit blossen Faeusten. Konter, Luftkampf und geistige Kraft gegen Daemonen."));

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

	// ---- Quests (story slice: courtyard -> east gate) ------------------------------------------
	auto Objective = [](const TCHAR* Id, EDBObjectiveKind Kind, const TCHAR* Target, int32 Required, const TCHAR* Description)
	{
		FDBQuestObjective Result;
		Result.ObjectiveId = Id;
		Result.Kind = Kind;
		Result.Target = Target;
		Result.Required = Required;
		Result.Description = FText::FromString(Description);
		return Result;
	};

	if (!Data.FindQuest(TEXT("MQ01_KingsSummons")))
	{
		UDBQuestDefinition* Quest = NewObject<UDBQuestDefinition>(&Data, NAME_None, RF_Transient);
		Quest->QuestId = TEXT("MQ01_KingsSummons");
		Quest->Title = FText::FromString(TEXT("Der Ruf des Koenigs [DEV]"));
		Quest->Category = EDBQuestCategory::Main;
		Quest->Scope = EDBQuestScope::Shared;
		Quest->RegionId = TEXT("Capital");
		Quest->bSequential = true;
		Quest->bAutoComplete = false; // report back to the king
		Quest->bCanAbandon = false;
		Quest->Objectives.Add(Objective(TEXT("TalkToKing"), EDBObjectiveKind::Talk, TEXT("NPC_King"), 1, TEXT("Sprich mit Koenig Aoki")));
		Quest->Objectives.Add(Objective(TEXT("DefeatTrainingDummies"), EDBObjectiveKind::Kill, TEXT("TrainingDummy"), 3,
			TEXT("Zerschlage die Uebungspuppen im Hof")));
		Quest->Reward.Xp = 250;
		Quest->Reward.Currency = 100;
		Quest->Reward.SkillPoints = 1;
		Quest->Reward.StoryFlags = {TEXT("Story.KingsSummonsDone")};
		Quest->Reward.Items = {Grant(TEXT("Bag_Small"), 1)};
		Data.RegisterQuest(Quest);
		bAddedAny = true;
	}

	if (!Data.FindQuest(TEXT("MQ02_EastGate")))
	{
		UDBQuestDefinition* Quest = NewObject<UDBQuestDefinition>(&Data, NAME_None, RF_Transient);
		Quest->QuestId = TEXT("MQ02_EastGate");
		Quest->Title = FText::FromString(TEXT("Schatten vor dem Osttor [DEV]"));
		Quest->Category = EDBQuestCategory::Main;
		Quest->Scope = EDBQuestScope::Shared;
		Quest->RegionId = TEXT("Capital");
		Quest->RequiredStoryFlags = {TEXT("Story.KingsSummonsDone")};
		Quest->PrerequisiteQuests = {TEXT("MQ01_KingsSummons")};
		Quest->bSequential = true;
		Quest->bAutoComplete = false; // report to the captain
		Quest->bCanAbandon = false;
		Quest->Objectives.Add(Objective(TEXT("ReportToCaptain"), EDBObjectiveKind::Talk, TEXT("NPC_Captain"), 1,
			TEXT("Melde dich bei Hauptmann Kenji am Osttor")));
		Quest->Objectives.Add(Objective(TEXT("SlayDemons"), EDBObjectiveKind::Kill, TEXT("LesserDemon"), 3, TEXT("Erschlage die niederen Daemonen")));
		Quest->Reward.Xp = 400;
		Quest->Reward.Currency = 150;
		Quest->Reward.SkillPoints = 1;
		Quest->Reward.StoryFlags = {TEXT("Story.EastGateCleared")};
		Quest->Reward.Items = {Grant(TEXT("Bag_Adventurer"), 1)};
		Data.RegisterQuest(Quest);
		bAddedAny = true;
	}

	// ---- Dialogues ------------------------------------------------------------------------------
	using ECond = EDBDialogueCondition;
	using EEff = EDBDialogueEffect;
	auto Cond = [](ECond Kind, const TCHAR* Id, EDBQuestStatus Status = EDBQuestStatus::Inactive)
	{
		FDBDialogueCondition Result;
		Result.Kind = Kind;
		Result.Id = Id;
		Result.Status = Status;
		return Result;
	};
	auto Eff = [](EEff Kind, const TCHAR* Id)
	{
		FDBDialogueEffect Result;
		Result.Kind = Kind;
		Result.Id = Id;
		return Result;
	};
	auto Choice = [](const TCHAR* Text, const TCHAR* Next, TArray<FDBDialogueEffect> Effects = {})
	{
		FDBDialogueChoice Result;
		Result.Text = FText::FromString(Text);
		Result.NextNodeId = Next ? FName(Next) : NAME_None;
		Result.Effects = MoveTemp(Effects);
		return Result;
	};
	auto Node = [](const TCHAR* Id, const TCHAR* Speaker, const TCHAR* Text, TArray<FDBDialogueChoice> Choices = {},
		TArray<FDBDialogueEffect> OnEnter = {})
	{
		FDBDialogueNode Result;
		Result.NodeId = Id;
		Result.Speaker = FText::FromString(Speaker);
		Result.Text = FText::FromString(Text);
		Result.Choices = MoveTemp(Choices);
		Result.OnEnter = MoveTemp(OnEnter);
		return Result;
	};
	auto Entry = [](const TCHAR* NodeId, TArray<FDBDialogueCondition> Conditions)
	{
		FDBDialogueEntry Result;
		Result.NodeId = NodeId;
		Result.Conditions = MoveTemp(Conditions);
		return Result;
	};

	if (!Data.FindDialogue(TEXT("Dlg_King")))
	{
		const TCHAR* King = TEXT("Koenig Aoki");
		UDBDialogueDefinition* Dialogue = NewObject<UDBDialogueDefinition>(&Data, NAME_None, RF_Transient);
		Dialogue->DialogueId = TEXT("Dlg_King");
		Dialogue->Entries = {
			Entry(TEXT("TurnIn"), {Cond(ECond::QuestStatusIs, TEXT("MQ01_KingsSummons"), EDBQuestStatus::ReadyToTurnIn)}),
			Entry(TEXT("Greet"), {Cond(ECond::QuestStatusIs, TEXT("MQ01_KingsSummons"), EDBQuestStatus::Active),
									 Cond(ECond::LacksStoryFlag, TEXT("Story.KingBriefed"))}),
			Entry(TEXT("Waiting"), {Cond(ECond::QuestStatusIs, TEXT("MQ01_KingsSummons"), EDBQuestStatus::Active)}),
			Entry(TEXT("Hurry"), {Cond(ECond::QuestStatusIsNot, TEXT("MQ02_EastGate"), EDBQuestStatus::Inactive),
									 Cond(ECond::QuestStatusIsNot, TEXT("MQ02_EastGate"), EDBQuestStatus::Completed)}),
			Entry(TEXT("Done"), {Cond(ECond::QuestStatusIs, TEXT("MQ02_EastGate"), EDBQuestStatus::Completed)}),
			// Worlds where the first quest was finished before the turn-in existed.
			Entry(TEXT("Offer"), {Cond(ECond::QuestStatusIs, TEXT("MQ01_KingsSummons"), EDBQuestStatus::Completed),
									 Cond(ECond::QuestStatusIs, TEXT("MQ02_EastGate"), EDBQuestStatus::Inactive)}),
			Entry(TEXT("Idle"), {}),
		};
		Dialogue->Nodes = {
			Node(TEXT("Offer"), King, TEXT("Du hast dich an den Puppen bewaehrt. Nun braucht die Stadt deine Klinge."),
				{Choice(TEXT("Ich bin bereit."), TEXT("Mission"), {Eff(EEff::StartQuest, TEXT("MQ02_EastGate"))}),
					Choice(TEXT("Spaeter."), nullptr)}),
			Node(TEXT("Greet"), King,
				TEXT("Wanderer. Man sagt, in deinen Adern fliesst dunkles Blut - dasselbe Blut, das die Daemonen ueber dieses Land "
					 "gebracht hat. Vielleicht ist es auch unsere letzte Hoffnung."),
				{Choice(TEXT("Was verlangt Ihr von mir?"), TEXT("Task"), {Eff(EEff::SetStoryFlag, TEXT("Story.KingBriefed"))}),
					Choice(TEXT("Wer seid Ihr?"), TEXT("Lore")), Choice(TEXT("Ich gehe."), nullptr)},
				{Eff(EEff::ReportTalk, TEXT("NPC_King"))}),
			Node(TEXT("Lore"), King,
				TEXT("Aoki, letzter Koenig eines sterbenden Reiches. Vierzehn Vasallen des Daemonenkoenigs halten meine Provinzen. "
					 "Die Hauptstadt steht nur noch, weil der alte Schutzzauber haelt."),
				{Choice(TEXT("Zurueck."), TEXT("Greet"))}),
			Node(TEXT("Task"), King,
				TEXT("Beweise deine Klinge. Im Hof stehen drei Uebungspuppen - zerschlage sie, dann sprich wieder mit mir.")),
			Node(TEXT("Waiting"), King, TEXT("Die Uebungspuppen warten, Wanderer.")),
			Node(TEXT("TurnIn"), King, TEXT("Gut geschlagen. Du bist schneller, als deine Kleidung vermuten laesst."),
				{Choice(TEXT("Ich bin bereit fuer mehr."), TEXT("Mission"),
					{Eff(EEff::TurnInQuest, TEXT("MQ01_KingsSummons")), Eff(EEff::StartQuest, TEXT("MQ02_EastGate"))})}),
			Node(TEXT("Mission"), King,
				TEXT("Am Osttor sammeln sich Daemonen. Hauptmann Kenji haelt die Stellung. Geh zu ihm - und kehre lebend zurueck.")),
			Node(TEXT("Hurry"), King, TEXT("Das Osttor, Wanderer. Kenji braucht jede Klinge.")),
			Node(TEXT("Done"), King, TEXT("Das Osttor steht. Heute Nacht schlaeft die Stadt zum ersten Mal seit Monaten.")),
			Node(TEXT("Idle"), King, TEXT("Die Stadt haelt stand. Noch.")),
		};
		Data.RegisterDialogue(Dialogue);
		bAddedAny = true;
	}

	if (!Data.FindDialogue(TEXT("Dlg_Captain")))
	{
		const TCHAR* Captain = TEXT("Hauptmann Kenji");
		UDBDialogueDefinition* Dialogue = NewObject<UDBDialogueDefinition>(&Data, NAME_None, RF_Transient);
		Dialogue->DialogueId = TEXT("Dlg_Captain");
		Dialogue->Entries = {
			Entry(TEXT("TurnIn"), {Cond(ECond::QuestStatusIs, TEXT("MQ02_EastGate"), EDBQuestStatus::ReadyToTurnIn)}),
			Entry(TEXT("Fight"), {Cond(ECond::QuestStatusIs, TEXT("MQ02_EastGate"), EDBQuestStatus::Active)}),
			Entry(TEXT("After"), {Cond(ECond::QuestStatusIs, TEXT("MQ02_EastGate"), EDBQuestStatus::Completed)}),
			Entry(TEXT("Idle"), {}),
		};
		Dialogue->Nodes = {
			// Briefing reveals the demons (encounter spawner waits for this flag).
			Node(TEXT("Fight"), Captain,
				TEXT("Da draussen, zwischen den Baeumen! Niedere Daemonen - drei, vielleicht mehr. Halt sie auf, bevor sie das Tor erreichen."),
				{}, {Eff(EEff::ReportTalk, TEXT("NPC_Captain")), Eff(EEff::SetStoryFlag, TEXT("Story.CaptainBriefed"))}),
			Node(TEXT("TurnIn"), Captain, TEXT("Beim Blut meiner Ahnen ... du hast sie alle erschlagen."),
				{Choice(TEXT("Das Tor ist sicher."), TEXT("Thanks"), {Eff(EEff::TurnInQuest, TEXT("MQ02_EastGate"))})}),
			Node(TEXT("Thanks"), Captain, TEXT("Der Koenig wird davon hoeren. Nimm das - du hast es dir verdient.")),
			Node(TEXT("After"), Captain, TEXT("Ruhig heute. Dank dir.")),
			Node(TEXT("Idle"), Captain, TEXT("Ohne Befehl des Koenigs oeffne ich das Tor fuer niemanden.")),
		};
		Data.RegisterDialogue(Dialogue);
		bAddedAny = true;
	}

	return bAddedAny;
}
