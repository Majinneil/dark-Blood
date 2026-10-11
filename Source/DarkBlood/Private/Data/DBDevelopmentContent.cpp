#include "Data/DBDevelopmentContent.h"

#include "Abilities/DBAbilitySet.h"
#include "Abilities/DBClassAbilities.h"
#include "Abilities/DBCombatAbilities.h"
#include "Abilities/DBMeleeAttackAbility.h"
#include "Boss/DBBossDefinition.h"
#include "Core/DBGameplayTags.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBDialogueDefinition.h"
#include "Data/DBEconomyDefinitions.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBItemDefinition.h"
#include "Data/DBQuestDefinition.h"
#include "Data/DBRegionDefinition.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInterface.h"
#include "Visual/DBAnimationSetDefinition.h"

#include "DarkBloodRules/WorldState.h"
#include "Visual/DBCharacterVisualDefinition.h"

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

	// Katana model + finishes: the free Fab "Corrupted Dark Katana", imported locally by Tools/UE58/db_import_fab.py
	// (Standard license, not in the repo). Without it the hand shows a plain steel blade.
	auto KatanaVisual = [](UDBItemDefinition& D, const TCHAR* Finish)
	{
		D.WorldMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/DarkBlood/Dev/FabWeapons/Katana/SM_Katana_Corrupted.SM_Katana_Corrupted")));
		const FString Material = FString::Printf(TEXT("MI_DB_Katana_%s"), Finish);
		D.WorldMaterial =
			TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(FString::Printf(TEXT("/Game/DarkBlood/Dev/FabWeapons/Katana/%s.%s"), *Material, *Material)));
	};

	auto Weapon = [&](FName Id, const TCHAR* Name, FName ClassId)
	{
		AddItem(Id, Name, EDBItemCategory::Weapon, 1, [&KatanaVisual, Id, ClassId](UDBItemDefinition& D)
		{
			if (Id == TEXT("Katana_Dev"))
			{
				KatanaVisual(D, TEXT("Steel"));
			}
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
			if (Id == TEXT("Katana_Tamahagane"))
			{
				KatanaVisual(D, TEXT("Tamahagane"));
			}
		});
	};
	Forged(TEXT("Katana_Tamahagane"), TEXT("Tamahagane-Katana"), TEXT("Warrior"), 5, [](FDBItemStats& S) { S.AttackPower = 14.f; S.CritChance = 0.02f; });
	Forged(TEXT("Kunai_Shadowsteel"), TEXT("Schattenstahl-Kunai"), TEXT("Shadowrunner"), 5, [](FDBItemStats& S) { S.AttackPower = 10.f; S.CritChance = 0.06f; });
	Forged(TEXT("Staff_Ember"), TEXT("Glutstab"), TEXT("Mage"), 5, [](FDBItemStats& S) { S.SpellPower = 16.f; S.FireResistance = 0.1f; S.MaxMana = 20.f; });
	Forged(TEXT("Handwraps_Iron"), TEXT("Eisenbandagen"), TEXT("Monk"), 5, [](FDBItemStats& S) { S.AttackPower = 11.f; S.SpiritResistance = 0.1f; });

	// Katana collection (Warrior): each blade has its own strengths and on-hit effects (see DBWeaponEffects).
	auto Effect = [](EDBWeaponEffectKind Kind, const FGameplayTag& Type, float Chance, float Magnitude, float Duration = 0.f)
	{
		FDBWeaponEffect Result;
		Result.Kind = Kind;
		Result.DamageType = Type;
		Result.Chance = Chance;
		Result.Magnitude = Magnitude;
		Result.Duration = Duration;
		return Result;
	};
	auto Katana = [&](FName Id, const TCHAR* Name, const TCHAR* Finish, EDBItemRarity Rarity, int32 Level, const TCHAR* Description,
		TFunctionRef<void(FDBItemStats&)> SetStats, TArray<FDBWeaponEffect> Effects)
	{
		AddItem(Id, Name, EDBItemCategory::Weapon, 1, [&](UDBItemDefinition& D)
		{
			D.EquipSlot = EDBEquipSlot::MainHand;
			D.ItemLevel = Level;
			D.RequiredLevel = Level;
			D.MaxDurability = 140 + Level * 10;
			D.BaseValue = 150 * Level;
			D.Rarity = Rarity;
			D.AllowedClasses = {TEXT("Warrior")};
			D.Description = FText::FromString(Description);
			SetStats(D.Stats);
			D.WeaponEffects = Effects;
			KatanaVisual(D, Finish);
		});
	};
	using EFx = EDBWeaponEffectKind;
	Katana(TEXT("Katana_Homura"), TEXT("Homura - Flammenklinge"), TEXT("Homura"), EDBItemRarity::Rare, 6,
		TEXT("Jeder Treffer +20 % Feuerschaden. 30 %: Brand (4 Feuerschaden pro Sekunde, 4 s)."),
		[](FDBItemStats& S) { S.AttackPower = 16.f; S.FireResistance = 0.05f; },
		{Effect(EFx::ElementalDamage, DBTags::Damage_Type_Fire, 1.f, 0.2f), Effect(EFx::DamageOverTime, DBTags::Damage_Type_Fire, 0.3f, 4.f, 4.f)});
	Katana(TEXT("Katana_Yukiore"), TEXT("Yukiore - Frostbiss"), TEXT("Yukiore"), EDBItemRarity::Rare, 6,
		TEXT("Jeder Treffer +20 % Frostschaden und +50 % Haltungsschaden: Gegner geraten schneller ins Taumeln."),
		[](FDBItemStats& S) { S.AttackPower = 15.f; S.FrostResistance = 0.1f; },
		{Effect(EFx::ElementalDamage, DBTags::Damage_Type_Frost, 1.f, 0.2f), Effect(EFx::PoiseBreak, DBTags::Damage_Type_Frost, 1.f, 0.5f)});
	Katana(TEXT("Katana_Dokuga"), TEXT("Dokuga - Giftzahn"), TEXT("Dokuga"), EDBItemRarity::Rare, 8,
		TEXT("50 %: Gift (5 Giftschaden pro Sekunde, 6 s)."),
		[](FDBItemStats& S) { S.AttackPower = 17.f; S.PoisonResistance = 0.1f; },
		{Effect(EFx::DamageOverTime, DBTags::Damage_Type_Poison, 0.5f, 5.f, 6.f)});
	Katana(TEXT("Katana_Raikiri"), TEXT("Raikiri - Donnerschneider"), TEXT("Raikiri"), EDBItemRarity::Epic, 10,
		TEXT("25 %: Blitzschlag (+60 % Blitzschaden). 35 %: der Blitz springt auf einen weiteren Gegner in 6 m ueber (40 %)."),
		[](FDBItemStats& S) { S.AttackPower = 20.f; S.CritChance = 0.08f; S.LightningResistance = 0.1f; },
		{Effect(EFx::ElementalDamage, DBTags::Damage_Type_Lightning, 0.25f, 0.6f), Effect(EFx::ChainStrike, DBTags::Damage_Type_Lightning, 0.35f, 0.4f)});
	Katana(TEXT("Katana_Chishio"), TEXT("Chishio - Blutdurst"), TEXT("Chishio"), EDBItemRarity::Epic, 10,
		TEXT("Jeder Treffer heilt dich um 8 % des Schadens. 40 %: Blutung (4 Schaden pro Sekunde, 5 s)."),
		[](FDBItemStats& S) { S.AttackPower = 21.f; S.MaxHealth = 20.f; },
		{Effect(EFx::Lifesteal, FGameplayTag(), 1.f, 0.08f), Effect(EFx::DamageOverTime, DBTags::Damage_Type_Physical, 0.4f, 4.f, 5.f)});
	Katana(TEXT("Katana_Kagekiri"), TEXT("Kagekiri - Schattenschnitt"), TEXT("Kagekiri"), EDBItemRarity::Epic, 12,
		TEXT("Jeder Treffer +30 % Schattenschaden. Hohe kritische Trefferchance."),
		[](FDBItemStats& S) { S.AttackPower = 22.f; S.CritChance = 0.12f; S.ShadowResistance = 0.1f; },
		{Effect(EFx::ElementalDamage, DBTags::Damage_Type_Shadow, 1.f, 0.3f)});
	Katana(TEXT("Katana_Reiha"), TEXT("Reiha - Geisterklinge"), TEXT("Reiha"), EDBItemRarity::Epic, 12,
		TEXT("Jeder Treffer +35 % Geistschaden - die Klinge der Daemonenjaeger. +20 Ausdauer."),
		[](FDBItemStats& S) { S.AttackPower = 19.f; S.MaxStamina = 20.f; S.SpiritResistance = 0.15f; },
		{Effect(EFx::ElementalDamage, DBTags::Damage_Type_Spirit, 1.f, 0.35f)});
	Katana(TEXT("Katana_Kegare"), TEXT("Kegare - Verdorbenes Katana"), TEXT("Kegare"), EDBItemRarity::Demonic, 15,
		TEXT("Vom Dunklen Blut verdorben: +40 % Dunkelblut-Schaden, 12 % Lebensraub, 20 %: Verderbnis (6 Schaden pro Sekunde, 5 s). ")
		TEXT("Der Traeger wird anfaelliger fuer Dunkelblut."),
		[](FDBItemStats& S) { S.AttackPower = 30.f; S.CritChance = 0.05f; S.DarkBloodResistance = -0.1f; },
		{Effect(EFx::ElementalDamage, DBTags::Damage_Type_DarkBlood, 1.f, 0.4f), Effect(EFx::Lifesteal, FGameplayTag(), 1.f, 0.12f),
			Effect(EFx::DamageOverTime, DBTags::Damage_Type_DarkBlood, 0.2f, 6.f, 5.f)});

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
	AddItem(TEXT("RiceBall"), TEXT("Onigiri"), EDBItemCategory::Food, 20, [](UDBItemDefinition& D) { D.HealAmount = 40.f; D.StaminaAmount = 30.f; D.SatietyAmount = 35.f; });
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
	// Names from the world map (docs/VisualPack/Reference/DarkBlood_Weltkarte.png, placed by DBRealmLayout); the vassal themes stay.
	const TCHAR* Themes[] = {TEXT("Blut"), TEXT("Frost"), TEXT("Schatten"), TEXT("Donner"), TEXT("Feuer / Asche"), TEXT("Knochen / Tod"),
		TEXT("Gift / Seuche"), TEXT("Wahnsinn / Illusion"), TEXT("Bestien"), TEXT("Eisen"), TEXT("Meer"), TEXT("Wind / Himmel"),
		TEXT("Finsternis"), TEXT("Schwarze Festung")};
	const TCHAR* Names[] = {TEXT("Kirschbluetental"), TEXT("Eisoede"), TEXT("Bambuswaelder"), TEXT("Nebelberge"), TEXT("Feuergebirge"),
		TEXT("Wuestenlande"), TEXT("Flusslande"), TEXT("Wald der Geister"), TEXT("Reisfelder"), TEXT("Grossstadt"), TEXT("Kuestenland"),
		TEXT("Himmelstempel"), TEXT("Daemonenoede"), TEXT("Vasallenfestung")};
	static_assert(UE_ARRAY_COUNT(Themes) == UE_ARRAY_COUNT(Names), "one name per vassal region");
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Themes)); ++Index)
	{
		const int32 Number = Index + 1;
		const int32 Min = 4 * Number - 1;
		AddRegion(FName(*FString::Printf(TEXT("Region%02d"), Number)), Names[Index], Themes[Index], EDBRegionKind::VassalRegion, Min, Min + 6);
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
	Recipe(TEXT("R_Katana_Homura"), TEXT("Katana_Homura"), 1, {In(TEXT("Tamahagane"), 6), In(TEXT("DemonHorn"), 3), In(TEXT("SpiritPaper"), 1)}, 150, 6, Forge);
	Recipe(TEXT("R_Katana_Yukiore"), TEXT("Katana_Yukiore"), 1, {In(TEXT("Tamahagane"), 6), In(TEXT("SpiritPaper"), 3)}, 150, 6, Forge);
	Recipe(TEXT("R_Katana_Dokuga"), TEXT("Katana_Dokuga"), 1, {In(TEXT("Tamahagane"), 5), In(TEXT("DemonOre"), 3), In(TEXT("Leather"), 2)}, 200, 8, Forge);
	Recipe(TEXT("R_Katana_Raikiri"), TEXT("Katana_Raikiri"), 1, {In(TEXT("Tamahagane"), 8), In(TEXT("DemonOre"), 4), In(TEXT("SpiritPaper"), 2)}, 350, 10, Forge);
	Recipe(TEXT("R_Katana_Chishio"), TEXT("Katana_Chishio"), 1, {In(TEXT("Tamahagane"), 8), In(TEXT("DemonHorn"), 5)}, 350, 10, Forge);
	Recipe(TEXT("R_Katana_Kagekiri"), TEXT("Katana_Kagekiri"), 1, {In(TEXT("Tamahagane"), 8), In(TEXT("DemonOre"), 5), In(TEXT("Leather"), 2)}, 450, 12, Forge);
	Recipe(TEXT("R_Katana_Reiha"), TEXT("Katana_Reiha"), 1, {In(TEXT("Tamahagane"), 8), In(TEXT("SpiritPaper"), 6)}, 450, 12, Forge);
	Recipe(TEXT("R_Katana_Kegare"), TEXT("Katana_Kegare"), 1, {In(TEXT("Tamahagane"), 10), In(TEXT("DemonOre"), 10), In(TEXT("DemonHorn"), 10)}, 800, 15, Forge);
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
	// Dungeons (Phase 9): treasure rooms and the guardian's hoard.
	Loot(TEXT("LT_DungeonChest"), {Grant(TEXT("HealingDraught"), 2)},
		{LootEntry(TEXT("Tamahagane"), 3, 2, 4), LootEntry(TEXT("DemonOre"), 2, 1, 2), LootEntry(TEXT("SpiritPaper"), 2, 1, 2), LootEntry(TEXT("RiceBall"), 2, 2, 3)},
		2, 1, 60, 120);
	// Bosses (Phase 10): every player in the fight gets the guaranteed drops and rolls on its own.
	Loot(TEXT("LT_Vassal"), {Grant(TEXT("DemonOre"), 4), Grant(TEXT("SpiritPaper"), 3), Grant(TEXT("HealingDraught"), 3)},
		{LootEntry(TEXT("Tamahagane"), 2, 4, 8), LootEntry(TEXT("DemonHorn"), 2, 3, 5)}, 2, 1, 300, 500);
	// Regions (Phase 11): the commander of a demon camp.
	Loot(TEXT("LT_Commander"), {Grant(TEXT("DemonOre"), 2), Grant(TEXT("HealingDraught"), 2)},
		{LootEntry(TEXT("Tamahagane"), 2, 2, 4), LootEntry(TEXT("DemonHorn"), 2, 1, 3), LootEntry(TEXT("SpiritPaper"), 1, 1, 2)}, 2, 1, 120, 240);
	Loot(TEXT("LT_DemonKing"), {Grant(TEXT("DemonOre"), 10), Grant(TEXT("SpiritPaper"), 10), Grant(TEXT("HealingDraught"), 5)},
		{LootEntry(TEXT("Tamahagane"), 1, 10, 15)}, 2, 1, 3000, 5000);
	Loot(TEXT("LT_DungeonGuardian"), {Grant(TEXT("DemonOre"), 2), Grant(TEXT("SpiritPaper"), 2)},
		{LootEntry(TEXT("Tamahagane"), 2, 3, 5), LootEntry(TEXT("HealingDraught"), 2, 1, 2), LootEntry(TEXT("DemonHorn"), 1, 2, 3)}, 2, 1, 120, 220);

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

	// A personal bounty (Phase 19): each player hunts on their own account; kills of the group count within 150 m.
	if (!Data.FindQuest(TEXT("SQ_DemonHunt")))
	{
		UDBQuestDefinition* Quest = NewObject<UDBQuestDefinition>(&Data, NAME_None, RF_Transient);
		Quest->QuestId = TEXT("SQ_DemonHunt");
		Quest->Title = FText::FromString(TEXT("Daemonenjagd [DEV]"));
		Quest->Category = EDBQuestCategory::Bounty;
		Quest->Scope = EDBQuestScope::Personal;
		Quest->RegionId = TEXT("Capital");
		Quest->bAutoComplete = true;
		Quest->bCanAbandon = true;
		Quest->Objectives.Add(Objective(TEXT("HuntDemons"), EDBObjectiveKind::Kill, TEXT("LesserDemon"), 3, TEXT("Erschlage niedere Daemonen")));
		Quest->Reward.Xp = 150;
		Quest->Reward.Currency = 60;
		Data.RegisterQuest(Quest);
		bAddedAny = true;
	}

	// ---- Region liberation quests (Phase 11, docs/REGIONS.md) ------------------------------------
	// Start when the party first enters a vassal region: thin the region's demons, break its demon camp (commander),
	// defeat its vassal. Objectives in any order; shared progress for the party.
	for (const UDBBossDefinition* Vassal : DBBosses::GetAll())
	{
		if (Vassal->Rank != EDBBossRank::Vassal || Vassal->Order > DarkBlood::Rules::NumVassalRegions)
		{
			continue;
		}
		const FString Region = Vassal->RegionId.ToString();
		const FName QuestId(*(TEXT("RQ_") + Region));
		if (Data.FindQuest(QuestId))
		{
			continue;
		}
		const UDBRegionDefinition* RegionDef = Data.FindRegion(Vassal->RegionId);
		const UDBBossDefinition* Commander = DBBosses::Find(FName(*(TEXT("MidBoss_") + Region)));
		UDBQuestDefinition* Quest = NewObject<UDBQuestDefinition>(&Data, NAME_None, RF_Transient);
		Quest->QuestId = QuestId;
		Quest->Title = FText::Format(NSLOCTEXT("DarkBloodQuests", "Liberation", "Befreiung: {0}"), RegionDef ? RegionDef->DisplayName : FText::FromString(Region));
		Quest->Category = EDBQuestCategory::Regional;
		Quest->Scope = EDBQuestScope::Shared;
		Quest->RegionId = Vassal->RegionId;
		Quest->bSequential = false;
		Quest->bAutoComplete = true;
		Quest->bCanAbandon = false;
		FDBQuestObjective Demons;
		Demons.ObjectiveId = TEXT("ThinTheDemons");
		Demons.Kind = EDBObjectiveKind::Kill;
		Demons.Target = FName(*(TEXT("Demon_") + Region));
		Demons.Required = 6;
		Demons.Description = NSLOCTEXT("DarkBloodQuests", "ThinTheDemons", "Erschlage Daemonen im Gebiet");
		Quest->Objectives.Add(Demons);
		FDBQuestObjective Camp;
		Camp.ObjectiveId = TEXT("BreakTheCamp");
		Camp.Kind = EDBObjectiveKind::Kill;
		Camp.Target = Commander ? Commander->BossId : FName(*(TEXT("MidBoss_") + Region));
		Camp.Required = 1;
		Camp.Description = FText::Format(NSLOCTEXT("DarkBloodQuests", "BreakTheCamp", "Zerschlage das Daemonenlager ({0})"),
			Commander ? Commander->DisplayName : FText::GetEmpty());
		Quest->Objectives.Add(Camp);
		FDBQuestObjective Lord;
		Lord.ObjectiveId = TEXT("DefeatTheVassal");
		Lord.Kind = EDBObjectiveKind::Kill;
		Lord.Target = Vassal->BossId;
		Lord.Required = 1;
		Lord.Description = FText::Format(NSLOCTEXT("DarkBloodQuests", "DefeatTheVassal", "Besiege {0}, {1}"), Vassal->DisplayName, Vassal->Title);
		Quest->Objectives.Add(Lord);
		Quest->Reward.Xp = 600 + Vassal->Order * 250;
		Quest->Reward.Currency = 200 + Vassal->Order * 60;
		Quest->Reward.SkillPoints = 1;
		Quest->Reward.StoryFlags = {FName(*(TEXT("Story.Liberated.") + Region))};
		Data.RegisterQuest(Quest);
		bAddedAny = true;
	}

	// ---- DAS ENDE (Phase 13/14, docs/THE_END.md): started in turn by the Gate of the End once its seal breaks ----
	auto Kill = [](const TCHAR* Id, const TCHAR* Target, const FText& Description, EDBObjectiveKind Kind = EDBObjectiveKind::Kill)
	{
		FDBQuestObjective Result;
		Result.ObjectiveId = Id;
		Result.Kind = Kind;
		Result.Target = Target;
		Result.Required = 1;
		Result.Description = Description;
		return Result;
	};
	auto MainQuest = [&](const TCHAR* Id, const TCHAR* Title, const TCHAR* Previous, TArray<FDBQuestObjective> Objectives, int64 Xp, int32 SkillPoints,
		const TCHAR* Flag)
	{
		if (Data.FindQuest(Id))
		{
			return;
		}
		UDBQuestDefinition* Quest = NewObject<UDBQuestDefinition>(&Data, NAME_None, RF_Transient);
		Quest->QuestId = Id;
		Quest->Title = FText::FromString(Title);
		Quest->Category = EDBQuestCategory::Main;
		Quest->Scope = EDBQuestScope::Shared;
		Quest->RegionId = TEXT("TheEnd");
		if (Previous)
		{
			Quest->PrerequisiteQuests = {Previous};
		}
		Quest->bSequential = false;
		Quest->bAutoComplete = true;
		Quest->bCanAbandon = false;
		Quest->Objectives = MoveTemp(Objectives);
		Quest->Reward.Xp = Xp;
		Quest->Reward.Currency = Xp / 10;
		Quest->Reward.SkillPoints = SkillPoints;
		Quest->Reward.StoryFlags = {FName(Flag)};
		Data.RegisterQuest(Quest);
		bAddedAny = true;
	};
	MainQuest(TEXT("MQ10_TheEndSeal"), TEXT("Das Siegel des Endes"), nullptr,
		{Kill(TEXT("RestAtBastion"), TEXT("Bastion_TheEnd"), NSLOCTEXT("DarkBloodQuests", "RestAtBastion", "Raste in der Letzten Bastion"), EDBObjectiveKind::Interact),
			Kill(TEXT("EnterTheEnd"), TEXT("TheEnd"), NSLOCTEXT("DarkBloodQuests", "EnterTheEnd", "Schreite durch das Tor des Endes"), EDBObjectiveKind::Reach)},
		4000, 1, TEXT("Story.TheEndEntered"));
	MainQuest(TEXT("MQ11_ThroneGuardians"), TEXT("Die Waechter des Throns"), TEXT("MQ10_TheEndSeal"),
		{Kill(TEXT("Tsukigami"), TEXT("V_Tsukigami"), NSLOCTEXT("DarkBloodQuests", "KillTsukigami", "Besiege Tsukigami, Vasall des Blutmondes")),
			Kill(TEXT("Shirogane"), TEXT("V_Shirogane"), NSLOCTEXT("DarkBloodQuests", "KillShirogane", "Besiege Shirogane, Rechte Hand des Daemonenkoenigs"))},
		8000, 2, TEXT("Story.ThroneOpen"));
	MainQuest(TEXT("MQ12_DemonKing"), TEXT("Der Daemonenkoenig"), TEXT("MQ11_ThroneGuardians"),
		{Kill(TEXT("DemonKing"), TEXT("B_DemonKing"), NSLOCTEXT("DarkBloodQuests", "KillKing", "Stuerze den Daemonenkoenig"))},
		20000, 3, TEXT("Story.KingSlain"));
	MainQuest(TEXT("MQ13_Paradise"), TEXT("Das Paradies"), TEXT("MQ12_DemonKing"),
		{Kill(TEXT("ThroughTheGate"), TEXT("ParadiseGate"), NSLOCTEXT("DarkBloodQuests", "ThroughTheGate", "Schreite durch die Pforte aus Licht am Thron"),
			 EDBObjectiveKind::Interact),
			Kill(TEXT("PeaceShrine"), TEXT("PeaceShrine"), NSLOCTEXT("DarkBloodQuests", "PeaceShrine", "Verweile am Schrein des Friedens"), EDBObjectiveKind::Interact)},
		5000, 1, TEXT("Story.Finale"));

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

	// ---- Character visuals (DEV: engine template mannequin, see Tools/UE58/Setup-DevMannequin.ps1) ------
	// Profiles only reference assets softly: without the local mannequin copy every character keeps its
	// greybox body. Real profiles (MetaHuman, custom demons) are assets under /Game/DarkBlood/Characters/Profiles.
	UDBAnimationSetDefinition* DevAnimations = Data.FindAnimationSet(TEXT("AS_Dev_Mannequin"));
	if (!DevAnimations)
	{
		DevAnimations = NewObject<UDBAnimationSetDefinition>(&Data, NAME_None, RF_Transient);
		DevAnimations->AnimationSetId = TEXT("AS_Dev_Mannequin");
		DevAnimations->AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
		auto Montage = [](const TCHAR* Name)
		{
			return TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(FString::Printf(TEXT("/Game/DarkBlood/Dev/Mannequin/%s.%s"), Name, Name)));
		};
		auto AddAnim = [&](const FGameplayTag& Key, TArray<TSoftObjectPtr<UAnimMontage>> Montages, bool bFit = true)
		{
			FDBAnimationEntry& New = DevAnimations->Entries.AddDefaulted_GetRef();
			New.Key = Key;
			New.Montages = MoveTemp(Montages);
			New.bFitToActionDuration = bFit;
		};
		const TArray<TSoftObjectPtr<UAnimMontage>> Combo = {Montage(TEXT("AM_DB_Dev_Attack_01")), Montage(TEXT("AM_DB_Dev_Attack_02")),
			Montage(TEXT("AM_DB_Dev_Attack_03"))};
		// Free Fab animations imported locally by Tools/UE58/db_import_fab.py (Standard license: not in the repo).
		auto FabMontage = [](const TCHAR* Folder, const TCHAR* Name)
		{
			return TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(FString::Printf(TEXT("/Game/DarkBlood/Dev/FabAnims/%s/%s.%s"), Folder, Name, Name)));
		};
		const bool bFabFight = FPackageName::DoesPackageExist(TEXT("/Game/DarkBlood/Dev/FabAnims/Fight/AM_DB_Fight_Attack_Sword_A"));
		const bool bFabRolls = FPackageName::DoesPackageExist(TEXT("/Game/DarkBlood/Dev/FabAnims/Rolls/AM_DB_Roll_0_front"));
		const TArray<TSoftObjectPtr<UAnimMontage>> SwordCombo = bFabFight
			? TArray<TSoftObjectPtr<UAnimMontage>>{FabMontage(TEXT("Fight"), TEXT("AM_DB_Fight_Attack_Sword_A")), FabMontage(TEXT("Fight"), TEXT("AM_DB_Fight_Attack_Sword_B")),
				Montage(TEXT("AM_DB_Dev_Attack_03"))}
			: Combo;
		AddAnim(DBTags::Anim_Attack, SwordCombo);
		AddAnim(DBTags::Anim_Attack_Light, SwordCombo);
		AddAnim(DBTags::Anim_Attack_Heavy, {Montage(TEXT("AM_DB_Dev_Attack_Heavy"))});
		AddAnim(DBTags::Anim_Attack_Charged, {bFabFight ? FabMontage(TEXT("Fight"), TEXT("AM_DB_Fight_Attack_Sword_B")) : Montage(TEXT("AM_DB_Dev_Attack_Heavy"))});
		if (bFabRolls)
		{
			static const TCHAR* Rolls[] = {TEXT("AM_DB_Roll_0_front"), TEXT("AM_DB_Roll_1_front_right_45"), TEXT("AM_DB_Roll_2_right"),
				TEXT("AM_DB_Roll_3_back_right_45"), TEXT("AM_DB_Roll_4_back"), TEXT("AM_DB_Roll_5_back_left_45"), TEXT("AM_DB_Roll_6_left"),
				TEXT("AM_DB_Roll_7_front_left_45")};
			TArray<TSoftObjectPtr<UAnimMontage>> RollMontages;
			for (const TCHAR* Roll : Rolls)
			{
				RollMontages.Add(FabMontage(TEXT("Rolls"), Roll));
			}
			AddAnim(DBTags::Anim_Dodge, RollMontages);
		}
		else
		{
			AddAnim(DBTags::Anim_Dodge, {Montage(TEXT("AM_DB_Dev_Dodge"))});
		}
		AddAnim(DBTags::Anim_HitReact, {Montage(TEXT("AM_DB_Dev_HitReact"))});
		AddAnim(DBTags::Anim_Knockdown, {Montage(TEXT("AM_DB_Dev_HitReact_Heavy"))});
		AddAnim(DBTags::Anim_Death, {Montage(TEXT("AM_DB_Dev_Death")), Montage(TEXT("AM_DB_Dev_Death_Back"))}, false);
		Data.RegisterAnimationSet(DevAnimations);
		bAddedAny = true;
	}

	auto AddVisual = [&](FName Id, EDBVisualQualityTier Tier, const TCHAR* MeshPath, const FLinearColor& Tint, float Scale = 1.f,
		const TCHAR* BodyMaterial = nullptr) -> UDBCharacterVisualDefinition*
	{
		if (Data.FindCharacterVisual(Id))
		{
			return nullptr;
		}
		UDBCharacterVisualDefinition* Def = NewObject<UDBCharacterVisualDefinition>(&Data, NAME_None, RF_Transient);
		Def->ProfileId = Id;
		Def->QualityTier = Tier;
		Def->bDevelopmentPlaceholder = true;
		Def->BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath));
		Def->AnimationSet = DevAnimations;
		Def->MeshTransform = FTransform(FRotator(0.f, -90.f, 0.f), FVector(0.f, 0.f, -88.f), FVector(Scale));
		Def->OutfitTintParameter = TEXT("Paint Tint");
		Def->OutfitTint = Tint;
		if (BodyMaterial)
		{
			const TSoftObjectPtr<UMaterialInterface> Material{FSoftObjectPath(BodyMaterial)};
			Def->BodyMaterials = {Material, Material};
		}
		Data.RegisterCharacterVisual(Def);
		bAddedAny = true;
		return Def;
	};
	const TCHAR* Manny = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	const TCHAR* Quinn = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");
	const TMap<FName, FLinearColor> ClassTints = {
		{TEXT("Warrior"), FLinearColor(0.45f, 0.05f, 0.04f)},
		{TEXT("Shadowrunner"), FLinearColor(0.05f, 0.06f, 0.1f)},
		{TEXT("Mage"), FLinearColor(0.2f, 0.06f, 0.35f)},
		{TEXT("Monk"), FLinearColor(0.6f, 0.35f, 0.05f)},
	};
	for (const TPair<FName, const TCHAR*>& Body : TArray<TPair<FName, const TCHAR*>>{{TEXT("CV_Player_TypeA"), Manny}, {TEXT("CV_Player_TypeB"), Quinn}})
	{
		if (UDBCharacterVisualDefinition* Player = AddVisual(Body.Key, EDBVisualQualityTier::Player, Body.Value, FLinearColor(0.45f, 0.05f, 0.04f)))
		{
			Player->ClassOutfitTints = ClassTints;
		}
	}
	AddVisual(TEXT("CV_NPC_King"), EDBVisualQualityTier::Hero, Manny, FLinearColor(0.75f, 0.52f, 0.12f));
	AddVisual(TEXT("CV_NPC_Captain"), EDBVisualQualityTier::ImportantNpc, Manny, FLinearColor(0.12f, 0.14f, 0.18f));
	AddVisual(TEXT("CV_NPC_Default"), EDBVisualQualityTier::ImportantNpc, Quinn, FLinearColor(0.5f, 0.44f, 0.34f));
	AddVisual(TEXT("CV_Enemy_LesserDemon"), EDBVisualQualityTier::Crowd, Manny, FLinearColor(0.3f, 0.02f, 0.02f), 1.12f,
		TEXT("/Game/DarkBlood/Art/Materials/DarkBlood/MI_DB_DarkBlood_Veins.MI_DB_DarkBlood_Veins"));

	// MetaHuman builds (Tools/UE58/db_metahuman_cast.py): BP_<Name> under /Game/DarkBlood/Characters/MetaHumans/Build becomes
	// CV_MH_<Name>. The hidden mannequin plays the animation set, the MetaHuman body follows it (VisualActorClass).
	// The editor scans assets in the background at startup: the character folders are scanned right here.
	IAssetRegistry::GetChecked().ScanPathsSynchronous({TEXT("/Game/DarkBlood/Characters/MetaHumans/Build"), TEXT("/Game/DarkBlood/Characters/Hyper3D")}, false);
	TArray<FAssetData> MetaHumanBuilds;
	IAssetRegistry::GetChecked().GetAssetsByPath(TEXT("/Game/DarkBlood/Characters/MetaHumans/Build"), MetaHumanBuilds, true);
	for (const FAssetData& Asset : MetaHumanBuilds)
	{
		FString Name = Asset.AssetName.ToString();
		if (Asset.AssetClassPath != UBlueprint::StaticClass()->GetClassPathName() || !Name.RemoveFromStart(TEXT("BP_")))
		{
			continue;
		}
		const FString ClassPath = Asset.PackageName.ToString() + TEXT(".") + Asset.AssetName.ToString() + TEXT("_C");
		if (UDBCharacterVisualDefinition* Body = AddVisual(FName(TEXT("CV_MH_") + Name), EDBVisualQualityTier::Hero, Manny, FLinearColor::White))
		{
			Body->bDevelopmentPlaceholder = false;
			Body->OutfitTintParameter = NAME_None;
			Body->bProceduralBlink = false;
			Body->VisualActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath));
		}
	}

	// Authored bodies from Hyper3D Rodin (Tools/UE58/blender_rig_hyper3d.py + db_import_hyper3d.py): SK_<Name> on the
	// mannequin skeleton under /Game/DarkBlood/Characters/Hyper3D/<Name> becomes CV_Hyper3D_<Name> with the mannequin
	// animations; bosses list these first (DBBossDefinition).
	TArray<FAssetData> AuthoredBodies;
	IAssetRegistry::GetChecked().GetAssetsByPath(TEXT("/Game/DarkBlood/Characters/Hyper3D"), AuthoredBodies, true);
	for (const FAssetData& Asset : AuthoredBodies)
	{
		FString Name = Asset.AssetName.ToString();
		if (Asset.AssetClassPath != USkeletalMesh::StaticClass()->GetClassPathName() || !Name.RemoveFromStart(TEXT("SK_")))
		{
			continue;
		}
		const FString MeshPath = Asset.GetSoftObjectPath().ToString();
		if (UDBCharacterVisualDefinition* Body = AddVisual(FName(TEXT("CV_Hyper3D_") + Name), EDBVisualQualityTier::Hero, *MeshPath, FLinearColor::White))
		{
			Body->bDevelopmentPlaceholder = false;
			Body->OutfitTintParameter = NAME_None;
			Body->bProceduralBlink = false;
		}
	}

	return bAddedAny;
}
