#include "Data/DBDevelopmentContent.h"

#include "Abilities/DBAbilitySet.h"
#include "Abilities/DBClassAbilities.h"
#include "Abilities/DBCombatAbilities.h"
#include "Abilities/DBMeleeAttackAbility.h"
#include "Core/DBGameplayTags.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBDialogueDefinition.h"
#include "Data/DBEconomyDefinitions.h"
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

	/** DEVELOPMENT class kit: class moveset (light/heavy) plus the shared defensive and movement abilities. */
	UDBAbilitySet* CreateClassKit(UObject* Outer, TSubclassOf<UDBGameplayAbility> Light, TSubclassOf<UDBGameplayAbility> Heavy)
	{
		UDBAbilitySet* Set = NewObject<UDBAbilitySet>(Outer, NAME_None, RF_Transient);
		auto Add = [Set](TSubclassOf<UDBGameplayAbility> Ability, const FGameplayTag& InputTag)
		{
			FDBAbilitySetAbility Entry;
			Entry.Ability = Ability;
			Entry.InputTag = InputTag;
			Set->GrantedAbilities.Add(Entry);
		};
		Add(Light, DBTags::Input_LightAttack);
		Add(Heavy, DBTags::Input_HeavyAttack);
		Add(UDBAbility_Dodge::StaticClass(), DBTags::Input_Dodge);
		Add(UDBAbility_Block::StaticClass(), DBTags::Input_Block);
		Add(UDBAbility_Sprint::StaticClass(), DBTags::Input_Sprint);
		Add(UDBAbility_HitReact::StaticClass(), FGameplayTag()); // triggered by gameplay events
		return Set;
	}

	FDBSkillNode SkillNode(FName Id, const TCHAR* Name, const TCHAR* Description, int32 RequiredLevel, int32 MaxRank,
		TSubclassOf<UDBGameplayAbility> Ability = nullptr, FGameplayTag InputTag = FGameplayTag(), FVector2D Position = FVector2D::ZeroVector)
	{
		FDBSkillNode Node;
		Node.NodeId = Id;
		Node.DisplayName = FText::FromString(Name);
		Node.Description = FText::FromString(Description);
		Node.RequiredLevel = RequiredLevel;
		Node.MaxRank = MaxRank;
		Node.CostPerRank = 1;
		Node.GrantedAbility = Ability;
		Node.InputTag = InputTag;
		Node.UIPosition = Position;
		return Node;
	}

	FDBSkillNode DoubleJumpNode()
	{
		return SkillNode(DBSkillNodes::DoubleJump, TEXT("Doppelsprung"), TEXT("Ein zweiter Sprung in der Luft - erreicht Daecher, Felsen und Luftkaempfe."),
			2, 1, UDBAbility_DoubleJump::StaticClass());
	}
}

bool FDBDevelopmentContent::RegisterMissing(UDBGameDataSubsystem& Data)
{
	bool bAddedAny = false;

	// ---- Classes --------------------------------------------------------------------------------
	auto AddClass = [&](FName Id, const FGameplayTag& Tag, const TCHAR* Name, const FDBStatBlock& Base, const FDBStatBlock& PerLevel,
		TArray<FDBItemGrant> StartItems, const TCHAR* Description, UDBAbilitySet* Kit, TArray<FDBSkillNode> Tree)
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
		Def->BaseAbilitySet = Kit;
		Def->SkillTree = MoveTemp(Tree);
		Data.RegisterClass(Def);
		bAddedAny = true;
	};

	AddClass(TEXT("Warrior"), DBTags::Class_Warrior, TEXT("Krieger"), Stats(14, 8, 3, 5, 14, 12), Stats(2.0f, 0.8f, 0.2f, 0.5f, 2.0f, 1.5f),
		{Grant(TEXT("Katana_Dev"), 1), Grant(TEXT("RiceBall"), 5), Grant(TEXT("HealingDraught"), 2)},
		TEXT("Schwertkaempfer der Frontlinie. Viel Leben und Ausdauer, starke Haltungen, haelt Treffer aus, die andere umwerfen."),
		CreateClassKit(&Data, UDBAbility_LightCombo::StaticClass(), UDBAbility_HeavyAttack::StaticClass()),
		{
			SkillNode(DBSkillNodes::IronStance, TEXT("Eiserne Haltung"),
				TEXT("Taste 1: Haltung an/aus. +40 Ruestung und Poise, Blocken kostet halb so viel Ausdauer, 20 % langsamer. Rang 2: Poise-Schaden halbiert."),
				1, 2, UDBAbility_IronStance::StaticClass(), DBTags::Input_Ability1),
			DoubleJumpNode(),
			SkillNode(DBSkillNodes::CounterSlash, TEXT("Konterschnitt"),
				TEXT("Konter nach perfekter Parade verursachen pro Rang einen weiteren vollen Schadensanteil (x2 -> x3 -> x4)."), 3, 2),
			SkillNode(DBSkillNodes::Bloodlust, TEXT("Blutrausch"), TEXT("Jeder Nahkampftreffer stellt 3 Ausdauer pro Rang wieder her."), 4, 2),
			SkillNode(DBSkillNodes::Breakthrough, TEXT("Durchbruch"),
				TEXT("Taste 2: Sturmangriff durch die Gegner (6 m): 30 Schaden und Knockdown. Rang 2: Abklingzeit 6 -> 4 s."),
				5, 2, UDBAbility_Breakthrough::StaticClass(), DBTags::Input_Ability2),
		});
	AddClass(TEXT("Shadowrunner"), DBTags::Class_Shadowrunner, TEXT("Schattenlaeufer"), Stats(8, 15, 5, 6, 9, 14),
		Stats(0.9f, 2.2f, 0.4f, 0.6f, 1.2f, 1.8f), {Grant(TEXT("Kunai_Dev"), 1), Grant(TEXT("RiceBall"), 5), Grant(TEXT("HealingDraught"), 2)},
		TEXT("Schneller Kaempfer aus den Schatten. Kunai, Schattenmal und Teleport, hohe Kritchance, wenig Ruestung."),
		CreateClassKit(&Data, UDBAbility_KunaiCombo::StaticClass(), UDBAbility_HeavyAttack::StaticClass()),
		{
			SkillNode(DBSkillNodes::ShadowMark, TEXT("Schattenmal"),
				TEXT("Taste 1: Gegner markieren. Erneut: hinter ihn teleportieren - kurz unverwundbar, naechster Treffer +50 %. Rang 2: Ankunft verursacht 15 Schattenschaden."),
				1, 2, UDBAbility_ShadowMark::StaticClass(), DBTags::Input_Ability1),
			DoubleJumpNode(),
			SkillNode(DBSkillNodes::Ambush, TEXT("Hinterhalt"), TEXT("Treffer von hinten verursachen +50 % Schaden pro Rang."), 3, 2),
			SkillNode(DBSkillNodes::LightFooted, TEXT("Leichtfuessig"), TEXT("Ausweichen kostet nur halb so viel Ausdauer."), 4, 1),
			SkillNode(DBSkillNodes::SmokeVeil, TEXT("Rauchschleier"),
				TEXT("Taste 2: 4 s im Rauch - Gegner verlieren dich aus den Augen. Angreifen beendet den Schleier."),
				5, 1, UDBAbility_SmokeVeil::StaticClass(), DBTags::Input_Ability2),
		});
	AddClass(TEXT("Mage"), DBTags::Class_Mage, TEXT("Magier"), Stats(3, 6, 16, 12, 8, 8), Stats(0.2f, 0.6f, 2.4f, 1.6f, 1.0f, 1.0f),
		{Grant(TEXT("Staff_Dev"), 1), Grant(TEXT("RiceBall"), 5), Grant(TEXT("HealingDraught"), 2)},
		TEXT("Gelehrter der alten Zauber. Elementarmagie, Schutzkreis und Flug - maechtig auf Distanz, verletzlich im Nahkampf."),
		CreateClassKit(&Data, UDBAbility_MagicBolt::StaticClass(), UDBAbility_FrostLance::StaticClass()),
		{
			SkillNode(DBSkillNodes::WardingCircle, TEXT("Schutzkreis"),
				TEXT("Taste 1: Schutzkreis (8 m, 10 s). Daemonen werden hinausgedraengt und verbrannt, Verbuendete nehmen 30 % weniger Schaden. Rang 2: heilt Verbuendete (5/s)."),
				1, 2, UDBAbility_WardingCircle::StaticClass(), DBTags::Input_Ability1),
			DoubleJumpNode(),
			SkillNode(DBSkillNodes::ChainBolt, TEXT("Kettenblitz"), TEXT("Magiegeschosse springen pro Rang auf einen weiteren Gegner (60 % Schaden)."), 3, 2),
			SkillNode(DBSkillNodes::ManaFlow, TEXT("Manafluss"), TEXT("+1,5 Manaregeneration pro Sekunde und Rang."), 4, 2, UDBAbility_ManaFlow::StaticClass()),
			SkillNode(DBSkillNodes::Flight, TEXT("Flug"), TEXT("Taste 2: Fliegen in Blickrichtung (8 Mana/s). Erneut druecken zum Landen."),
				6, 1, UDBAbility_Flight::StaticClass(), DBTags::Input_Ability2),
		});
	AddClass(TEXT("Monk"), DBTags::Class_Monk, TEXT("Moench"), Stats(11, 12, 4, 12, 11, 13), Stats(1.4f, 1.4f, 0.3f, 1.4f, 1.4f, 1.6f),
		{Grant(TEXT("Handwraps_Dev"), 1), Grant(TEXT("RiceBall"), 5), Grant(TEXT("HealingDraught"), 2)},
		TEXT("Kriegermoench mit blossen Faeusten. Konter, Luftkampf und geistige Kraft gegen Daemonen."),
		CreateClassKit(&Data, UDBAbility_FistCombo::StaticClass(), UDBAbility_PalmStrike::StaticClass()),
		{
			SkillNode(DBSkillNodes::CounterStance, TEXT("Konterhaltung"),
				TEXT("Taste 1: 0,6 s Konterhaltung - der naechste Treffer wird abgefangen und mit 25 Geistschaden beantwortet. Rang 2: Konter +50 %."),
				1, 2, UDBAbility_CounterStance::StaticClass(), DBTags::Input_Ability1),
			DoubleJumpNode(),
			SkillNode(DBSkillNodes::InnerCalm, TEXT("Innere Ruhe"), TEXT("Perfektes Parierfenster +0,1 s pro Rang."), 3, 2),
			SkillNode(DBSkillNodes::IronBody, TEXT("Eisenkoerper"), TEXT("+20 maximale Poise pro Rang."), 4, 2, UDBAbility_IronBody::StaticClass()),
			SkillNode(DBSkillNodes::SkyKick, TEXT("Himmelstritt"),
				TEXT("Taste 2: Tritt dich und Gegner vor dir in die Luft - weiter mit Luftangriffen."),
				5, 1, UDBAbility_SkyKick::StaticClass(), DBTags::Input_Ability2),
		});

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
			D.BaseValue = 60;
			D.AllowedClasses = {ClassId};
			// Training weapons: a small bonus to the class' main power.
			D.Stats.AttackPower = ClassId == TEXT("Mage") ? 0.f : 4.f;
			D.Stats.SpellPower = ClassId == TEXT("Mage") ? 5.f : 0.f;
		});
	};
	Weapon(TEXT("Katana_Dev"), TEXT("Uebungskatana"), TEXT("Warrior"));
	Weapon(TEXT("Kunai_Dev"), TEXT("Uebungskunai"), TEXT("Shadowrunner"));
	Weapon(TEXT("Staff_Dev"), TEXT("Uebungsstab"), TEXT("Mage"));
	Weapon(TEXT("Handwraps_Dev"), TEXT("Faustbandagen"), TEXT("Monk"));

	// Forged class weapons (Phase 5): better stats, crafted at the forge.
	auto Forged = [&](FName Id, const TCHAR* Name, FName ClassId, int32 Level, TFunctionRef<void(FDBItemStats&)> SetStats)
	{
		AddItem(Id, Name, EDBItemCategory::Weapon, 1, [&](UDBItemDefinition& D)
		{
			D.EquipSlot = EDBEquipSlot::MainHand;
			D.ItemLevel = Level;
			D.RequiredLevel = Level;
			D.MaxDurability = 160;
			D.BaseValue = 400;
			D.Rarity = EDBItemRarity::Uncommon;
			D.AllowedClasses = {ClassId};
			SetStats(D.Stats);
		});
	};
	Forged(TEXT("Katana_Tamahagane"), TEXT("Tamahagane-Katana"), TEXT("Warrior"), 5, [](FDBItemStats& S) { S.AttackPower = 14.f; S.CritChance = 0.02f; });
	Forged(TEXT("Kunai_Shadowsteel"), TEXT("Schattenstahl-Kunai"), TEXT("Shadowrunner"), 5, [](FDBItemStats& S) { S.AttackPower = 10.f; S.CritChance = 0.06f; });
	Forged(TEXT("Staff_Ember"), TEXT("Glutstab"), TEXT("Mage"), 5, [](FDBItemStats& S) { S.SpellPower = 16.f; S.FireResistance = 0.1f; S.MaxMana = 20.f; });
	Forged(TEXT("Handwraps_Iron"), TEXT("Eisenbandagen"), TEXT("Monk"), 5, [](FDBItemStats& S) { S.AttackPower = 11.f; S.SpiritResistance = 0.1f; });

	// Armor (all classes)
	auto Armor = [&](FName Id, const TCHAR* Name, EDBEquipSlot Slot, int32 Level, TFunctionRef<void(FDBItemStats&)> SetStats)
	{
		AddItem(Id, Name, EDBItemCategory::Armor, 1, [&](UDBItemDefinition& D)
		{
			D.EquipSlot = Slot;
			D.ItemLevel = Level;
			D.RequiredLevel = Level;
			D.MaxDurability = 120;
			D.BaseValue = 150;
			SetStats(D.Stats);
		});
	};
	Armor(TEXT("Helm_Ashigaru"), TEXT("Ashigaru-Helm"), EDBEquipSlot::Head, 1, [](FDBItemStats& S) { S.Armor = 8.f; S.MaxHealth = 15.f; });
	Armor(TEXT("Do_Ashigaru"), TEXT("Ashigaru-Brustpanzer"), EDBEquipSlot::Chest, 3, [](FDBItemStats& S) { S.Armor = 18.f; S.MaxHealth = 30.f; });
	Armor(TEXT("Kote_Leather"), TEXT("Lederne Kote"), EDBEquipSlot::Hands, 2, [](FDBItemStats& S) { S.Armor = 5.f; S.CritChance = 0.01f; });
	Armor(TEXT("Suneate_Iron"), TEXT("Eisen-Suneate"), EDBEquipSlot::Legs, 3, [](FDBItemStats& S) { S.Armor = 10.f; });
	Armor(TEXT("Waraji"), TEXT("Waraji-Sandalen"), EDBEquipSlot::Feet, 1, [](FDBItemStats& S) { S.Armor = 3.f; S.MaxStamina = 12.f; });
	AddItem(TEXT("Amulet_Ward"), TEXT("Schutzamulett"), EDBItemCategory::Armor, 1, [](UDBItemDefinition& D)
	{
		D.EquipSlot = EDBEquipSlot::Accessory1;
		D.ItemLevel = 6;
		D.RequiredLevel = 6;
		D.BaseValue = 300;
		D.Rarity = EDBItemRarity::Rare;
		D.Stats.SpiritResistance = 0.15f;
		D.Stats.ShadowResistance = 0.1f;
		D.Stats.MaxMana = 20.f;
	});

	AddItem(TEXT("Tamahagane"), TEXT("Tamahagane"), EDBItemCategory::Material, 99, [](UDBItemDefinition&) {});
	AddItem(TEXT("DemonOre"), TEXT("Daemonenerz"), EDBItemCategory::Material, 99, [](UDBItemDefinition& D) { D.Rarity = EDBItemRarity::Rare; });
	AddItem(TEXT("DemonHorn"), TEXT("Daemonenhorn"), EDBItemCategory::Material, 99, [](UDBItemDefinition& D) { D.Rarity = EDBItemRarity::Uncommon; });
	AddItem(TEXT("Leather"), TEXT("Leder"), EDBItemCategory::Material, 99, [](UDBItemDefinition&) {});
	AddItem(TEXT("SpiritPaper"), TEXT("Geisterpapier"), EDBItemCategory::Material, 99, [](UDBItemDefinition&) {});
	AddItem(TEXT("RiceBall"), TEXT("Onigiri"), EDBItemCategory::Food, 20, [](UDBItemDefinition& D) { D.HealAmount = 40.f; D.StaminaAmount = 30.f; });
	AddItem(TEXT("HealingDraught"), TEXT("Heiltrank"), EDBItemCategory::Potion, 10, [](UDBItemDefinition& D) { D.HealAmount = 120.f; D.BaseValue = 25; });
	AddItem(TEXT("ManaTea"), TEXT("Geistertee"), EDBItemCategory::Potion, 10, [](UDBItemDefinition& D) { D.ManaAmount = 60.f; D.BaseValue = 25; });
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

	// ---- Recipes (forge) and loot tables ----------------------------------------------------------
	auto Recipe = [&](FName Id, FName Output, int32 OutputCount, TArray<FDBRecipeIngredient> Inputs, int64 Mon, int32 Level, FName Station)
	{
		if (Data.FindRecipe(Id))
		{
			return;
		}
		UDBRecipeDefinition* Def = NewObject<UDBRecipeDefinition>(&Data, NAME_None, RF_Transient);
		Def->RecipeId = Id;
		Def->OutputItemId = Output;
		Def->OutputCount = OutputCount;
		Def->Inputs = MoveTemp(Inputs);
		Def->CurrencyCost = Mon;
		Def->RequiredLevel = Level;
		Def->StationId = Station;
		Data.RegisterRecipe(Def);
		bAddedAny = true;
	};
	auto In = [](const TCHAR* ItemId, int32 Count)
	{
		FDBRecipeIngredient Ingredient;
		Ingredient.ItemId = ItemId;
		Ingredient.Count = Count;
		return Ingredient;
	};
	const FName Forge(TEXT("Forge"));
	Recipe(TEXT("R_Katana_Tamahagane"), TEXT("Katana_Tamahagane"), 1, {In(TEXT("Tamahagane"), 6), In(TEXT("DemonHorn"), 2)}, 80, 5, Forge);
	Recipe(TEXT("R_Kunai_Shadowsteel"), TEXT("Kunai_Shadowsteel"), 1, {In(TEXT("Tamahagane"), 4), In(TEXT("DemonOre"), 2)}, 70, 5, Forge);
	Recipe(TEXT("R_Staff_Ember"), TEXT("Staff_Ember"), 1, {In(TEXT("SpiritPaper"), 3), In(TEXT("DemonHorn"), 2)}, 70, 5, Forge);
	Recipe(TEXT("R_Handwraps_Iron"), TEXT("Handwraps_Iron"), 1, {In(TEXT("Leather"), 3), In(TEXT("Tamahagane"), 2)}, 60, 5, Forge);
	Recipe(TEXT("R_Helm_Ashigaru"), TEXT("Helm_Ashigaru"), 1, {In(TEXT("Tamahagane"), 3), In(TEXT("Leather"), 1)}, 40, 1, Forge);
	Recipe(TEXT("R_Do_Ashigaru"), TEXT("Do_Ashigaru"), 1, {In(TEXT("Tamahagane"), 5), In(TEXT("Leather"), 3)}, 70, 3, Forge);
	Recipe(TEXT("R_Waraji"), TEXT("Waraji"), 1, {In(TEXT("Leather"), 2)}, 15, 1, Forge);
	Recipe(TEXT("R_Amulet_Ward"), TEXT("Amulet_Ward"), 1, {In(TEXT("DemonOre"), 3), In(TEXT("SpiritPaper"), 2)}, 120, 6, Forge);
	// Simple brewing works anywhere.
	Recipe(TEXT("R_HealingDraught"), TEXT("HealingDraught"), 2, {In(TEXT("SpiritPaper"), 1), In(TEXT("RiceBall"), 1)}, 0, 1, NAME_None);

	auto Loot = [&](FName Id, TArray<FDBItemGrant> Guaranteed, TArray<FDBLootEntry> Entries, int32 Rolls, int32 Nothing, int64 MonMin, int64 MonMax)
	{
		if (Data.FindLootTable(Id))
		{
			return;
		}
		UDBLootTableDefinition* Def = NewObject<UDBLootTableDefinition>(&Data, NAME_None, RF_Transient);
		Def->LootTableId = Id;
		Def->Guaranteed = MoveTemp(Guaranteed);
		Def->Entries = MoveTemp(Entries);
		Def->Rolls = Rolls;
		Def->NothingWeight = Nothing;
		Def->CurrencyMin = MonMin;
		Def->CurrencyMax = MonMax;
		Data.RegisterLootTable(Def);
		bAddedAny = true;
	};
	auto LootEntry = [](const TCHAR* ItemId, int32 Weight, int32 Min, int32 Max)
	{
		FDBLootEntry Result;
		Result.ItemId = ItemId;
		Result.Weight = Weight;
		Result.MinCount = Min;
		Result.MaxCount = Max;
		return Result;
	};
	Loot(TEXT("LT_LesserDemon"), {Grant(TEXT("DemonHorn"), 1)},
		{LootEntry(TEXT("Tamahagane"), 3, 1, 2), LootEntry(TEXT("Leather"), 2, 1, 2), LootEntry(TEXT("SpiritPaper"), 2, 1, 1), LootEntry(TEXT("DemonOre"), 1, 1, 1),
			LootEntry(TEXT("HealingDraught"), 1, 1, 1)},
		2, 1, 5, 15);
	Loot(TEXT("LT_TrainingDummy"), {}, {LootEntry(TEXT("Leather"), 1, 1, 1), LootEntry(TEXT("Tamahagane"), 1, 1, 1)}, 1, 2, 0, 3);
	Loot(TEXT("LT_Chest_Courtyard"), {Grant(TEXT("Helm_Ashigaru"), 1), Grant(TEXT("HealingDraught"), 2), Grant(TEXT("Tamahagane"), 3)},
		{LootEntry(TEXT("Leather"), 1, 1, 2)}, 1, 0, 40, 60);

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
