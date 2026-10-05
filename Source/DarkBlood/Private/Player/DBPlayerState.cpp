#include "Player/DBPlayerState.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBGameplayAbility.h"
#include "Abilities/DBRegenerationEffect.h"
#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBRegionDefinition.h"
#include "Engine/World.h"
#include "Inventory/DBInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBProgressionComponent.h"
#include "Player/DBSurvivalComponent.h"
#include "Quest/DBQuestComponent.h"
#include "Quest/DBQuestSubsystem.h"
#include "World/DBRegionVolume.h"

namespace R = DarkBlood::Rules;

ADBPlayerState::ADBPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UDBAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDBAttributeSet>(TEXT("Attributes"));
	Progression = CreateDefaultSubobject<UDBProgressionComponent>(TEXT("Progression"));
	Inventory = CreateDefaultSubobject<UDBInventoryComponent>(TEXT("Inventory"));
	PersonalQuests = CreateDefaultSubobject<UDBQuestComponent>(TEXT("PersonalQuests"));
	Survival = CreateDefaultSubobject<UDBSurvivalComponent>(TEXT("Survival"));

	// GAS on the PlayerState needs a higher update rate than the default.
	SetNetUpdateFrequency(100.f);
}

void ADBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBPlayerState, Profile);
	DOREPLIFETIME(ADBPlayerState, bCharacterReady);
	DOREPLIFETIME(ADBPlayerState, CurrentRegionId); // party members on the map
	DOREPLIFETIME_CONDITION(ADBPlayerState, DiscoveredRegions, COND_OwnerOnly);
	DOREPLIFETIME(ADBPlayerState, Titles);
}

UAbilitySystemComponent* ADBPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FString ADBPlayerState::GetPlayerNameCustom() const
{
	return Profile.CharacterName.IsEmpty() ? Super::GetPlayerNameCustom() : Profile.CharacterName;
}

FString ADBPlayerState::GetCallName() const
{
	return DBBridge::ToFString(R::GetCallName(DBBridge::ToStd(Profile.CharacterName)));
}

void ADBPlayerState::ApplyCharacterRecord(const R::FCharacterRecord& Record)
{
	check(HasAuthority());

	FGuid CharacterId;
	FGuid::Parse(DBBridge::ToFString(Record.CharacterId), CharacterId);
	Profile.CharacterId = CharacterId;
	Profile.CharacterName = DBBridge::ToFString(Record.Name);
	Profile.ClassId = DBBridge::ToFName(Record.ClassId);
	Profile.Appearance = DBBridge::FromRules(Record.Appearance);

	Inventory->RestoreFromRecord(Record.Inventory, Record.Equipment, Record.Currency, Record.PendingDeliveries);
	PersonalQuests->RestoreFromRecord(Record.PersonalQuests);

	DiscoveredRegions.Reset();
	for (const std::string& Region : Record.DiscoveredRegions)
	{
		DiscoveredRegions.Add(DBBridge::ToFName(Region));
	}
	Titles.Reset();
	for (const std::string& Title : Record.Titles)
	{
		Titles.Add(DBBridge::ToFName(Title));
	}
	RespawnPointId = DBBridge::ToFName(Record.RespawnPointId);
	PlayTimeSecondsAtLoad = Record.PlayTimeSeconds;
	LoadedAtWorldSeconds = GetWorld()->GetTimeSeconds();

	// Class kit (abilities + effects) and passive regeneration.
	ClassAbilityHandles.TakeFromAbilitySystem(AbilitySystemComponent);
	if (const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this))
	{
		if (const UDBClassDefinition* ClassDefinition = Data->FindClass(Profile.ClassId))
		{
			if (ClassDefinition->BaseAbilitySet)
			{
				ClassDefinition->BaseAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &ClassAbilityHandles, this);
			}
		}
	}
	ClassAbilityHandles.Effects.Add(AbilitySystemComponent->ApplyGameplayEffectToSelf(
		GetDefault<UDBRegenerationEffect>(), 1.f, AbilitySystemComponent->MakeEffectContext()));

	// Progression last: it recalculates attributes from class, level and the restored gear.
	Progression->RestoreFromRecord(Record.Progression, Record.Skills);

	bCharacterReady = true;
	SetPlayerName(Profile.CharacterName);
	OnProfileChanged.Broadcast(this);
	UE_LOG(LogDarkBlood, Log, TEXT("Character '%s' (%s, level %d) ready"), *Profile.CharacterName, *Profile.ClassId.ToString(),
		Record.Progression.Level);
}

R::FCharacterRecord ADBPlayerState::BuildCharacterRecord() const
{
	R::FCharacterRecord Record;
	Record.CharacterId = DBBridge::ToStd(Profile.CharacterId.ToString(EGuidFormats::DigitsWithHyphens));
	Record.Name = DBBridge::ToStd(Profile.CharacterName);
	Record.ClassId = DBBridge::ToStd(Profile.ClassId);
	Record.Appearance = DBBridge::ToRules(Profile.Appearance);
	Record.Progression = Progression->GetState();
	Record.Skills = Progression->GetSkills();
	Record.Currency = Inventory->GetServerCurrency();
	Record.Inventory = Inventory->GetRulesInventory();
	Record.Equipment = Inventory->GetRulesEquipment();
	Record.PendingDeliveries = Inventory->GetPendingDeliveries();
	Record.PersonalQuests = PersonalQuests->GetLog();
	for (const FName& Region : DiscoveredRegions)
	{
		Record.DiscoveredRegions.insert(DBBridge::ToStd(Region));
	}
	for (const FName& Title : Titles)
	{
		Record.Titles.insert(DBBridge::ToStd(Title));
	}
	Record.RespawnPointId = DBBridge::ToStd(RespawnPointId);
	const double Elapsed = GetWorld() ? GetWorld()->GetTimeSeconds() - LoadedAtWorldSeconds : 0.0;
	Record.PlayTimeSeconds = PlayTimeSecondsAtLoad + static_cast<int64>(FMath::Max(0.0, Elapsed));
	return Record;
}

void ADBPlayerState::GrantSkillAbility(TSubclassOf<UDBGameplayAbility> AbilityClass, int32 Level, const FGameplayTag& InputTag)
{
	if (!HasAuthority() || !AbilityClass)
	{
		return;
	}
	// Upgrading a node raises the level of the already granted ability instead of granting it twice.
	for (FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass)
		{
			Spec.Level = Level;
			AbilitySystemComponent->MarkAbilitySpecDirty(Spec);
			// Passives apply their bonus on activation: restart them so the new rank takes effect.
			const UDBGameplayAbility* Ability = Cast<UDBGameplayAbility>(Spec.Ability);
			if (Ability && Ability->GetActivationPolicy() == EDBAbilityActivationPolicy::OnSpawn && Spec.IsActive())
			{
				const FGameplayAbilitySpecHandle Handle = Spec.Handle;
				AbilitySystemComponent->CancelAbilityHandle(Handle);
				AbilitySystemComponent->TryActivateAbility(Handle);
			}
			return;
		}
	}
	FGameplayAbilitySpec Spec(AbilityClass->GetDefaultObject<UDBGameplayAbility>(), Level);
	Spec.SourceObject = this;
	if (InputTag.IsValid())
	{
		Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
	}
	AbilitySystemComponent->GiveAbility(Spec);
}

void ADBPlayerState::EnterRegionVolume(ADBRegionVolume* Volume)
{
	OverlappingRegionVolumes.AddUnique(Volume);
	UpdateCurrentRegion();
}

void ADBPlayerState::ExitRegionVolume(ADBRegionVolume* Volume)
{
	OverlappingRegionVolumes.Remove(Volume);
	UpdateCurrentRegion();
}

void ADBPlayerState::SetRealmRegion(FName RegionId)
{
	if (RealmRegionId != RegionId)
	{
		RealmRegionId = RegionId;
		UpdateCurrentRegion();
	}
}

void ADBPlayerState::UpdateCurrentRegion()
{
	const ADBRegionVolume* Best = nullptr;
	OverlappingRegionVolumes.RemoveAll([](const TWeakObjectPtr<ADBRegionVolume>& Volume) { return !Volume.IsValid(); });
	for (const TWeakObjectPtr<ADBRegionVolume>& Volume : OverlappingRegionVolumes)
	{
		if (Volume->Region && (!Best || Volume->Priority > Best->Priority))
		{
			Best = Volume.Get();
		}
	}

	const FName NewRegion = Best ? Best->Region->RegionId : RealmRegionId;
	if (NewRegion != CurrentRegionId)
	{
		CurrentRegionId = NewRegion;
		DiscoverRegion(NewRegion);
		// Entering a vassal region starts its liberation quest for the party (once; Phase 11).
		const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
		const UDBRegionDefinition* Region = Data ? Data->FindRegion(NewRegion) : nullptr;
		UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this);
		if (HasAuthority() && Region && Region->Kind == EDBRegionKind::VassalRegion && Quests)
		{
			Quests->StartQuest(FName(*(TEXT("RQ_") + NewRegion.ToString())), this);
		}
		// "Reach" objectives name a region (e.g. entering DAS ENDE, Phase 13).
		if (HasAuthority() && Quests && !NewRegion.IsNone())
		{
			Quests->ReportEvent(EDBObjectiveKind::Reach, NewRegion, 1, this);
		}
		OnRegionChanged.Broadcast(this, CurrentRegionId);
	}
}

void ADBPlayerState::DiscoverRegion(FName RegionId)
{
	if (!RegionId.IsNone() && !DiscoveredRegions.Contains(RegionId))
	{
		DiscoveredRegions.Add(RegionId);
		// Discovery XP (Entdeckung) - tuned via data in Phase 6.
		Progression->AwardXp(50);
	}
}

bool ADBPlayerState::GetCurrentDanger(EDBDangerTier& OutTier, int32& OutRecommendedMin, int32& OutRecommendedMax, int32& OutPower) const
{
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	const UDBRegionDefinition* Region = Data ? Data->FindRegion(CurrentRegionId) : nullptr;
	if (!Region)
	{
		return false;
	}
	OutRecommendedMin = Region->RecommendedPowerMin;
	OutRecommendedMax = Region->RecommendedPowerMax;
	OutPower = Progression->GetPowerRating();
	OutTier = DBBridge::CastEnum<EDBDangerTier>(R::AssessDanger(OutPower, OutRecommendedMin, OutRecommendedMax));
	return true;
}

void ADBPlayerState::OnRep_Profile()
{
	OnProfileChanged.Broadcast(this);
}

void ADBPlayerState::OnRep_CurrentRegion()
{
	OnRegionChanged.Broadcast(this, CurrentRegionId);
}
