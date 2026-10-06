#include "Boss/DBBossDefinition.h"

#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "UObject/StrongObjectPtr.h"
#include "World/DBDungeon.h"
#include "World/DBRealmLayout.h"

#include "DarkBloodRules/Region.h"
#include "DarkBloodRules/WorldState.h"

TArray<float> UDBBossDefinition::GetPhaseThresholds() const
{
	TArray<float> Thresholds;
	for (int32 Index = 1; Index < Phases.Num(); ++Index)
	{
		Thresholds.Add(Phases[Index].HealthThreshold);
	}
	return Thresholds;
}

int32 UDBBossDefinition::GetMechanics(int32 Phase) const
{
	int32 Mechanics = 0;
	for (int32 Index = 0; Index <= Phase && Index < Phases.Num(); ++Index)
	{
		Mechanics |= Phases[Index].Mechanics;
	}
	return Mechanics;
}

namespace
{
	using namespace EDBBossMechanic;

	struct FBossDef
	{
		const TCHAR* Id;
		const TCHAR* Name;
		const TCHAR* Title;
		EDBBossRank Rank;
		const TCHAR* Region;
		int32 Order;
		FGameplayTag Damage;
		FLinearColor Color;
		float Health;
		float Attack;
		float Armor;
		float Scale;
		int32 FirstMechanics;
		int32 SecondMechanics;
	};

	UDBBossDefinition* Make(const FBossDef& Def)
	{
		UDBBossDefinition* Boss = NewObject<UDBBossDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Boss->BossId = Def.Id;
		Boss->DisplayName = FText::FromString(Def.Name);
		Boss->Title = FText::FromString(Def.Title);
		Boss->Rank = Def.Rank;
		Boss->RegionId = Def.Region;
		Boss->Order = Def.Order;
		Boss->DamageType = Def.Damage;
		Boss->Color = Def.Color;
		Boss->MaxHealth = Def.Health;
		Boss->AttackPower = Def.Attack;
		Boss->Armor = Def.Armor;
		Boss->Level = 20 + Def.Order * 4;
		Boss->XpReward = 1200 + Def.Order * 250;
		Boss->SkillPoints = 1;
		Boss->LootTableId = TEXT("LT_Vassal");
		// Head panel of the concept sheet (Tools/UE58/db_import_boss_portraits.py).
		const FString Portrait = FString::Printf(TEXT("/Game/DarkBlood/UI/Bosses/T_Portrait_%s.T_Portrait_%s"), Def.Id, Def.Id);
		Boss->Portrait = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Portrait));
		FDBBossPhase First;
		First.HealthThreshold = 1.f;
		First.Mechanics = Def.FirstMechanics;
		First.Scale = Def.Scale;
		First.Name = NSLOCTEXT("DarkBloodBoss", "PhaseOne", "Erste Phase");
		FDBBossPhase Second;
		Second.HealthThreshold = 0.5f;
		Second.Mechanics = Def.SecondMechanics;
		Second.Scale = Def.Scale * 1.1f;
		Second.Name = NSLOCTEXT("DarkBloodBoss", "PhaseTwo", "Entfesselt");
		Boss->Phases = {First, Second};
		return Boss;
	}

	TArray<UDBBossDefinition*> Build()
	{
		const FGameplayTag Blood = DBTags::Damage_Type_DarkBlood;
		const FGameplayTag Frost = DBTags::Damage_Type_Frost;
		const FGameplayTag Shadow = DBTags::Damage_Type_Shadow;
		const FGameplayTag Thunder = DBTags::Damage_Type_Lightning;
		const FGameplayTag Fire = DBTags::Damage_Type_Fire;
		const FGameplayTag Spirit = DBTags::Damage_Type_Spirit;
		const FGameplayTag Poison = DBTags::Damage_Type_Poison;
		const FGameplayTag Physical = DBTags::Damage_Type_Physical;
		const EDBBossRank V = EDBBossRank::Vassal;
		// The 16 vassals (one per vassal region, two guarding DAS ENDE) with their theme's attacks.
		const FBossDef Vassals[] = {
			{TEXT("V_Akakage"), TEXT("Akakage"), TEXT("Vasall des Blutes"), V, TEXT("Region01"), 1, Blood, FLinearColor(1.f, 0.08f, 0.06f), 2400.f, 28.f, 60.f, 1.3f, Charge | Hazard, Slam},
			{TEXT("V_Yukimaru"), TEXT("Yukimaru"), TEXT("Vasall des Frosts"), V, TEXT("Region02"), 2, Frost, FLinearColor(0.55f, 0.85f, 1.f), 2600.f, 30.f, 70.f, 1.3f, Volley | Hazard, Slam},
			{TEXT("V_Kurobane"), TEXT("Kurobane"), TEXT("Vasall des Schattens"), V, TEXT("Region03"), 3, Shadow, FLinearColor(0.4f, 0.2f, 0.7f), 2600.f, 32.f, 60.f, 1.3f, Charge | Summon, Volley},
			{TEXT("V_Raikyo"), TEXT("Raikyo"), TEXT("Vasall des Donners"), V, TEXT("Region04"), 4, Thunder, FLinearColor(0.6f, 0.75f, 1.f), 2800.f, 34.f, 70.f, 1.35f, Volley | Slam, Hazard},
			{TEXT("V_Enkazan"), TEXT("Enkazan"), TEXT("Vasall von Feuer und Asche"), V, TEXT("Region05"), 5, Fire, FLinearColor(1.f, 0.42f, 0.08f), 3200.f, 38.f, 90.f, 1.5f, Slam | Hazard, Charge},
			{TEXT("V_Shikotsu"), TEXT("Shikotsu"), TEXT("Vasall von Knochen und Tod"), V, TEXT("Region06"), 6, Spirit, FLinearColor(0.9f, 0.86f, 0.7f), 3000.f, 36.f, 80.f, 1.35f, Summon | Slam, Hazard},
			{TEXT("V_Dokuga"), TEXT("Dokuga"), TEXT("Vasall von Gift und Seuche"), V, TEXT("Region07"), 7, Poison, FLinearColor(0.45f, 0.95f, 0.2f), 3000.f, 36.f, 70.f, 1.3f, Hazard | Volley, Summon},
			{TEXT("V_Mugenrei"), TEXT("Mugenrei"), TEXT("Vasall der Illusion"), V, TEXT("Region08"), 8, Spirit, FLinearColor(0.85f, 0.4f, 1.f), 3100.f, 38.f, 60.f, 1.3f, Volley | Summon, Charge},
			{TEXT("V_Juragan"), TEXT("Juragan"), TEXT("Vasall der Bestien"), V, TEXT("Region09"), 9, Physical, FLinearColor(0.85f, 0.6f, 0.3f), 3400.f, 42.f, 80.f, 1.6f, Charge | Summon, Slam},
			{TEXT("V_Tetsukhan"), TEXT("Tetsukhan"), TEXT("Vasall des Eisens"), V, TEXT("Region10"), 10, Physical, FLinearColor(0.75f, 0.75f, 0.82f), 3800.f, 44.f, 140.f, 1.8f, Slam | Charge, Summon},
			{TEXT("V_Kujiraa"), TEXT("Kujiraa"), TEXT("Vasall der Tiefen"), V, TEXT("Region11"), 11, Frost, FLinearColor(0.15f, 0.5f, 1.f), 3500.f, 44.f, 90.f, 1.4f, Hazard | Volley, Slam},
			{TEXT("V_Hayate"), TEXT("Hayate"), TEXT("Vasall des Windes"), V, TEXT("Region12"), 12, Physical, FLinearColor(0.95f, 0.95f, 0.88f), 3400.f, 46.f, 60.f, 1.3f, Charge | Volley, Hazard},
			{TEXT("V_Kokuya"), TEXT("Kokuya"), TEXT("Vasall der Finsternis"), V, TEXT("Region13"), 13, Shadow, FLinearColor(0.7f, 0.04f, 0.1f), 3800.f, 48.f, 80.f, 1.4f, Summon | Hazard, Charge},
			{TEXT("V_Reikon"), TEXT("Reikon"), TEXT("Vasall der Leere"), V, TEXT("Region14"), 14, Blood, FLinearColor(0.55f, 0.45f, 0.95f), 4000.f, 50.f, 90.f, 1.4f, Volley | Hazard, Summon},
			{TEXT("V_Tsukigami"), TEXT("Tsukigami"), TEXT("Vasall des Blutmondes"), V, TEXT("TheEnd"), 15, Blood, FLinearColor(1.f, 0.12f, 0.22f), 4500.f, 54.f, 90.f, 1.45f, Charge | Volley, Hazard | Summon},
			{TEXT("V_Shirogane"), TEXT("Shirogane"), TEXT("Rechte Hand des Daemonenkoenigs"), V, TEXT("TheEnd"), 16, Physical, FLinearColor(0.85f, 0.87f, 0.95f), 5000.f, 58.f, 130.f, 1.7f, Slam | Charge | Summon, Volley},
		};
		TArray<UDBBossDefinition*> All;
		for (const FBossDef& Def : Vassals)
		{
			All.Add(Make(Def));
		}

		// Each vassal's own attack and words (Phase 12), in vassal order 1..16.
		struct FPersona
		{
			EDBBossSignature Signature;
			const TCHAR* Taunt;
			const TCHAR* PhaseTaunt;
		};
		using S = EDBBossSignature;
		static const FPersona Personas[] = {
			{S::BloodTrail, TEXT("Blut ist Erinnerung. Dein Blut wird meines."), TEXT("Sieh, was das Dunkle Blut aus mir macht!")},
			{S::FrostRings, TEXT("Still. Kalt. Ewig. So endet jeder, der hierher kommt."), TEXT("Erfriere!")},
			{S::ShadowStep, TEXT("Du siehst mich nicht. Du hast mich nie gesehen."), TEXT("Die Schatten gehorchen mir!")},
			{S::ThunderRain, TEXT("Der Himmel richtet dich!"), TEXT("Donner, zerreiss sie!")},
			{S::FlameWall, TEXT("Alles wird zu Asche."), TEXT("Brenne!")},
			{S::BoneArmy, TEXT("Deine Knochen werden meiner Armee dienen."), TEXT("Erhebt euch, Tote!")},
			{S::PlagueCloud, TEXT("Atme tief ein ..."), TEXT("Die Seuche kennt keine Gnade.")},
			{S::Illusions, TEXT("Bin ich hier? Oder dort?"), TEXT("Welcher von uns ist echt?")},
			{S::PackCall, TEXT("Das Rudel jagt!"), TEXT("Zerreisst sie!")},
			{S::IronSkin, TEXT("Eisen vergisst nicht. Eisen verzeiht nicht."), TEXT("Eisen bleibt!")},
			{S::TidalWave, TEXT("Die Tiefe nimmt alles zurueck."), TEXT("Die Flut kommt!")},
			{S::StormBlades, TEXT("Zu langsam."), TEXT("Der Sturm kennt keine Gnade!")},
			{S::NightNova, TEXT("Die Nacht ist ewig."), TEXT("Verschwinde in der Finsternis!")},
			{S::VoidPull, TEXT("Nichts bleibt. Nicht einmal du."), TEXT("Die Leere ruft dich!")},
			{S::BloodMoon, TEXT("Unter dem Blutmond falle ich nicht."), TEXT("Blutmond, erhebe dich!")},
			{S::KingsBanner, TEXT("Kein Sterblicher erreicht den Thron."), TEXT("Fuer den Koenig!")},
		};
		static_assert(UE_ARRAY_COUNT(Personas) == DarkBlood::Rules::NumVassals, "one persona per vassal");
		// Bodies from the free Epic Paragon characters (Fab, set up locally by Tools/UE58/db_setup_paragon.py), in
		// vassal order; a missing pack keeps the placeholder demon. Several fallbacks so every vassal gets a body.
		static const TArray<FString> Bodies[] = {
			{TEXT("CV_ParagonKwang_Kwang_GDC"), TEXT("CV_ParagonKwang*")},         // Akakage: blood samurai
			{TEXT("CV_ParagonAurora"), TEXT("CV_ParagonKwang_KwangAlbino")},     // Yukimaru: frost
			{TEXT("CV_ParagonKallari")},                                         // Kurobane: shadow
			{TEXT("CV_ParagonSteel"), TEXT("CV_ParagonGreystone")},              // Raikyo: thunder
			{TEXT("CV_ParagonFengMao"), TEXT("CV_ParagonKwang_KwangSunrise")},   // Enkazan: fire and ash
			{TEXT("CV_ParagonRevenant"), TEXT("CV_ParagonKhaimera")},            // Shikotsu: bone and death
			{TEXT("CV_ParagonMorigesh")},                                        // Dokuga: poison and plague
			{TEXT("CV_ParagonGideon"), TEXT("CV_ParagonMorigesh")},              // Mugenrei: illusion
			{TEXT("CV_ParagonGrux")},                                            // Juragan: beasts
			{TEXT("CV_ParagonRampage")},                                         // Tetsukhan: iron
			{TEXT("CV_ParagonTerra"), TEXT("CV_ParagonKwang_KwangRosewood")},    // Kujiraa: the depths
			{TEXT("CV_ParagonSunWukong"), TEXT("CV_ParagonWukong")},              // Hayate: wind
			{TEXT("CV_ParagonKhaimera")},                                        // Kokuya: darkness
			{TEXT("CV_ParagonGideon"), TEXT("CV_ParagonKallari")},               // Reikon: the void
			{TEXT("CV_ParagonCountess")},                                        // Tsukigami: blood moon
			{TEXT("CV_ParagonGreystone")},                                       // Shirogane: the king's right hand
		};
		static_assert(UE_ARRAY_COUNT(Bodies) == DarkBlood::Rules::NumVassals, "one body per vassal");
		for (UDBBossDefinition* Boss : All)
		{
			if (Boss->Order >= 1 && Boss->Order <= static_cast<int32>(UE_ARRAY_COUNT(Bodies)))
			{
				Boss->VisualProfiles = Bodies[Boss->Order - 1];
			}
		}
		for (UDBBossDefinition* Boss : All)
		{
			if (Boss->Order >= 1 && Boss->Order <= static_cast<int32>(UE_ARRAY_COUNT(Personas)))
			{
				const FPersona& Persona = Personas[Boss->Order - 1];
				Boss->Signature = Persona.Signature;
				Boss->Taunt = FText::FromString(Persona.Taunt);
				Boss->PhaseTaunt = FText::FromString(Persona.PhaseTaunt);
			}
		}

		// Region commanders (mid-bosses, Phase 11, docs/REGIONS.md): each vassal region's demon camp is led by one. Beating
		// it makes the region contested; it fights with its vassal's first-phase mechanics and calls its guards later.
		static const TCHAR* CommanderNames[] = {TEXT("Blutklinge"), TEXT("Frostwaechter"), TEXT("Schattenpfeil"), TEXT("Donnerrufer"), TEXT("Glutfaust"),
			TEXT("Knochenhauptmann"), TEXT("Seuchenbringer"), TEXT("Trugbild"), TEXT("Rudelfuehrer"), TEXT("Eisenhauptmann"), TEXT("Gezeitenhauptmann"),
			TEXT("Sturmklinge"), TEXT("Nachtschatten"), TEXT("Leerenhueter")};
		static TArray<FString> CommanderStrings; // FBossDef keeps raw pointers
		CommanderStrings.Reset(UE_ARRAY_COUNT(CommanderNames) * 2);
		for (const FBossDef& Def : Vassals)
		{
			if (Def.Order < 1 || Def.Order > DarkBlood::Rules::NumVassalRegions)
			{
				continue;
			}
			const FString& Id = CommanderStrings.Add_GetRef(FString::Printf(TEXT("MidBoss_%s"), Def.Region));
			const FString& Title = CommanderStrings.Add_GetRef(FString::Printf(TEXT("Hauptmann von %s"), Def.Name));
			FBossDef Commander = Def;
			Commander.Id = *Id;
			Commander.Name = CommanderNames[Def.Order - 1];
			Commander.Title = *Title;
			Commander.Rank = EDBBossRank::MidBoss;
			Commander.Health = Def.Health * DarkBlood::Rules::GetCommanderStrength(Def.Order);
			Commander.Attack = Def.Attack * 0.8f;
			Commander.Scale = 1.2f;
			Commander.SecondMechanics = Summon;
			UDBBossDefinition* Boss = Make(Commander);
			Boss->Order = 0;
			Boss->Level = FMath::Max(1, Boss->Level - 3);
			Boss->XpReward = FMath::RoundToInt(Boss->XpReward * 0.4f);
			Boss->SkillPoints = 0;
			Boss->LootTableId = TEXT("LT_Commander");
			Boss->Portrait.Reset();
			Boss->ArenaRadius = 1800.f;
			// Camp commanders: the armoured super minion.
			Boss->VisualProfiles = {TEXT("CV_ParagonMinions_Minion_Lane_Super_Dusk"), TEXT("CV_ParagonMinions_Minion_Lane_Super_Dawn"), TEXT("CV_ParagonKhaimera")};
			All.Add(Boss);
		}

		// The demon king: three forms (Daemonischer Kaiser, Dark-Blood-Korruption, vollstaendige Daemonenform).
		UDBBossDefinition* King = Make({TEXT("B_DemonKing"), TEXT("Der Daemonenkoenig"), TEXT("Herr des Dunklen Blutes"), EDBBossRank::DemonKing, TEXT("TheEnd"), 0,
			Blood, FLinearColor(0.95f, 0.04f, 0.04f), 12000.f, 65.f, 150.f, 1.8f, Charge | Slam, 0});
		King->Level = 100;
		King->XpReward = 20000;
		King->SkillPoints = 3;
		King->LootTableId = TEXT("LT_DemonKing");
		King->EnrageAfterSeconds = 420.f;
		King->ArenaRadius = 3400.f;
		FDBBossPhase Emperor;
		Emperor.HealthThreshold = 1.f;
		Emperor.Mechanics = Charge | Slam;
		Emperor.Scale = 1.8f;
		Emperor.Name = NSLOCTEXT("DarkBloodBoss", "KingForm1", "Form 1: Daemonischer Kaiser");
		FDBBossPhase Corruption;
		Corruption.HealthThreshold = 0.66f;
		Corruption.Mechanics = Hazard | Volley;
		Corruption.Scale = 2.1f;
		Corruption.Name = NSLOCTEXT("DarkBloodBoss", "KingForm2", "Form 2: Dark-Blood-Korruption");
		FDBBossPhase DemonForm;
		DemonForm.HealthThreshold = 0.33f;
		DemonForm.Mechanics = Summon;
		DemonForm.Scale = 2.6f;
		DemonForm.Name = NSLOCTEXT("DarkBloodBoss", "KingForm3", "Form 3: Vollstaendige Daemonenform");
		King->Phases = {Emperor, Corruption, DemonForm};
		// Phase 14: his own attack changes with every form; he speaks at every form.
		King->Signature = EDBBossSignature::Cataclysm;
		King->Taunt = FText::FromString(TEXT("Ihr seid weit gekommen, Sterbliche. Hier, vor meinem Thron, endet euer Weg."));
		King->PhaseTaunts = {FText::GetEmpty(), FText::FromString(TEXT("Spuert das Dunkle Blut, das diese Welt naehrt!")),
			FText::FromString(TEXT("Genug! Seht meine wahre Gestalt - und vergeht!"))};
		King->VisualProfiles = {TEXT("CV_ParagonSevarog")};
		All.Add(King);

		// The guardian of the dungeons (scaled by the dungeon's stage when spawned).
		UDBBossDefinition* Guardian = Make({TEXT("B_DungeonGuardian"), TEXT("Dungeon-Waechter"), TEXT("Hueter der Tiefe"), EDBBossRank::WorldBoss, TEXT(""), 0,
			Blood, FLinearColor(1.f, 0.25f, 0.15f), 650.f, 18.f, 40.f, 1.35f, Slam | Summon, Hazard});
		Guardian->Level = 10;
		Guardian->XpReward = 200;
		Guardian->SkillPoints = 0;
		Guardian->Portrait.Reset();
		Guardian->LootTableId = NAME_None; // the dungeon's hoard is the reward
		Guardian->VisualProfiles = {TEXT("CV_ParagonRampage"), TEXT("CV_ParagonGrux"), TEXT("CV_ParagonMinions_Minion_Lane_Super_Dusk")};
		All.Add(Guardian);
		return All;
	}
}

const TArray<UDBBossDefinition*>& DBBosses::GetAll()
{
	static TArray<TStrongObjectPtr<UDBBossDefinition>> Owned;
	static TArray<UDBBossDefinition*> All;
	if (All.Num() == 0)
	{
		for (UDBBossDefinition* Boss : Build())
		{
			Owned.Emplace(Boss);
			All.Add(Boss);
		}
	}
	return All;
}

const UDBBossDefinition* DBBosses::Find(FName BossId)
{
	for (const UDBBossDefinition* Boss : GetAll())
	{
		if (Boss->BossId == BossId)
		{
			return Boss;
		}
	}
	return nullptr;
}

const UDBBossDefinition* DBBosses::FindByName(const FString& IdOrName)
{
	const int32 Number = IdOrName.IsNumeric() ? FCString::Atoi(*IdOrName) : -1;
	for (const UDBBossDefinition* Boss : GetAll())
	{
		if (Boss->BossId.ToString().Equals(IdOrName, ESearchCase::IgnoreCase) || Boss->DisplayName.ToString().StartsWith(IdOrName, ESearchCase::IgnoreCase) ||
			(Number > 0 && Boss->Order == Number) || (IdOrName.Equals(TEXT("King"), ESearchCase::IgnoreCase) && Boss->Rank == EDBBossRank::DemonKing))
		{
			return Boss;
		}
	}
	return nullptr;
}

FVector DBBosses::FindOpenGround(const FVector2D& Base, double RadiusMeters, const TArray<FVector2D>& Taken, double MinDistanceMeters, const FString& What)
{
	const double Radius = RadiusMeters;
	const double Clearance = Radius + 150.0;
	FVector2D Best = Base;
	double BestScore = TNumericLimits<double>::Max();
	double BestHeight = DBRealm::SampleHeight(Base.X, Base.Y);
	// First pass: the whole ring on dry land; if nothing qualifies, only its center (coasts).
	for (int32 Pass = 0; Pass < 2 && BestScore == TNumericLimits<double>::Max(); ++Pass)
	for (int32 Ring = 0; Ring <= 14; ++Ring)
	{
		const int32 Steps = Ring == 0 ? 1 : 6 + Ring * 2;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * Step / Steps + Ring * 0.37;
			const FVector2D Candidate = Base + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Ring * 60.0);
			const bool bClearOfTowns = !DBRealm::GetSettlements().ContainsByPredicate([&](const FDBRealmSettlement& Site)
			{
				return FVector2D::Distance(Candidate, Site.Center) < Site.Radius + Clearance;
			});
			const bool bClearOfGates = !DBDungeon::GetSites().ContainsByPredicate([&](const FDBDungeonSite& Site)
			{
				return FVector2D::Distance(Candidate, Site.Entrance) < Clearance;
			});
			const bool bClearOfArenas = !Taken.ContainsByPredicate([&](const FVector2D& Other) { return FVector2D::Distance(Candidate, Other) < MinDistanceMeters; });
			if (!bClearOfArenas || !bClearOfTowns || !bClearOfGates || !DBRealm::IsInside(Candidate.X * 1.06, Candidate.Y * 1.06))
			{
				continue;
			}
			// Height spread over the ring: center, half radius and edge.
			double Low = TNumericLimits<double>::Max();
			double High = TNumericLimits<double>::Lowest();
			double Sum = 0.0;
			int32 Samples = 0;
			for (const double Fraction : {0.0, 0.5, 1.0})
			{
				const int32 Points = Fraction == 0.0 ? 1 : 12;
				for (int32 Point = 0; Point < Points; ++Point)
				{
					const double PointAngle = 2.0 * UE_DOUBLE_PI * Point / Points;
					const double Height = DBRealm::SampleHeight(Candidate.X + FMath::Cos(PointAngle) * Radius * Fraction, Candidate.Y + FMath::Sin(PointAngle) * Radius * Fraction);
					Low = FMath::Min(Low, Height);
					High = FMath::Max(High, Height);
					Sum += Height;
					++Samples;
				}
			}
			if (Pass == 0 ? Low < 1.5 : DBRealm::SampleHeight(Candidate.X, Candidate.Y) < 1.0)
			{
				continue; // water inside the ring
			}
			const double Score = (High - Low) + FVector2D::Distance(Candidate, Base) * 0.01;
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = Candidate;
				BestHeight = Sum / Samples;
			}
		}
	}
	if (BestScore == TNumericLimits<double>::Max())
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("%s: no dry, open ground near (%.0f, %.0f) m"), *What, Base.X, Base.Y);
	}
	const FVector Location(Best.X * 100.0, Best.Y * 100.0, BestHeight * 100.0);
	return Location;
}

FVector DBBosses::GetArenaLocation(const UDBBossDefinition& Boss)
{
	if (Boss.Rank != EDBBossRank::Vassal && Boss.Rank != EDBBossRank::DemonKing)
	{
		return FVector::ZeroVector;
	}
	// Deterministic (server, clients and the map agree) but not cheap: computed once per boss.
	static TMap<FName, FVector> Cache;
	if (const FVector* Known = Cache.Find(Boss.BossId))
	{
		return *Known;
	}
	const FDBRealmRegion* Region = DBRealm::GetRegions().FindByPredicate([&Boss](const FDBRealmRegion& Candidate) { return Candidate.RegionId == Boss.RegionId; });
	const FVector2D Center = Region ? Region->Center : FVector2D::ZeroVector;
	// DAS ENDE holds three arenas: the two guardian vassals on the way, the king at its heart.
	FVector2D Offset(-450.0, 350.0);
	if (Boss.Rank == EDBBossRank::DemonKing)
	{
		Offset = FVector2D(0.0, 0.0);
	}
	else if (Boss.BossId == FName(TEXT("V_Tsukigami")))
	{
		Offset = FVector2D(-900.0, -500.0);
	}
	else if (Boss.BossId == FName(TEXT("V_Shirogane")))
	{
		Offset = FVector2D(-700.0, 600.0);
	}
	else if (Boss.BossId == FName(TEXT("V_Kujiraa")))
	{
		Offset = FVector2D(650.0, 250.0); // the coast faces west: inland, past the harbor town
	}
	// The flattest dry spot near the intended place, inside the realm, away from settlements and dungeon gates: players
	// must not slide out of the ring (DAS ENDE's ridges are steep).
	const FVector2D Base = Center + Offset;
	// Arenas sharing a region keep their distance (the king first, then the two guardians).
	TArray<FVector2D> Taken;
	if (Boss.Rank == EDBBossRank::Vassal && Boss.Order > DarkBlood::Rules::NumVassalRegions)
	{
		for (const UDBBossDefinition* Other : GetAll())
		{
			if (Other->RegionId == Boss.RegionId && (Other->Rank == EDBBossRank::DemonKing || (Other->Rank == EDBBossRank::Vassal && Other->Order < Boss.Order)))
			{
				Taken.Add(FVector2D(GetArenaLocation(*Other)) / 100.0);
			}
		}
	}
	const FVector Location = FindOpenGround(Base, Boss.ArenaRadius / 100.0, Taken, 350.0, TEXT("Boss arena ") + Boss.BossId.ToString());
	Cache.Add(Boss.BossId, Location);
	return Location;
}
