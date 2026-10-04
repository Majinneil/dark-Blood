#include "World/DBWorldStateComponent.h"

#include "Core/DBGameSettings.h"
#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBRegionDefinition.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "World/DBRealmLayout.h"

namespace R = DarkBlood::Rules;

namespace
{
	/** Starting residents of each settlement type of the open world. */
	int32 GetStartingResidents(EDBSettlementType Type)
	{
		switch (Type)
		{
		case EDBSettlementType::Capital: return 900;
		case EDBSettlementType::GreatCity: return 700;
		case EDBSettlementType::HarborTown: return 450;
		case EDBSettlementType::MiningTown: return 300;
		case EDBSettlementType::CaravanTown: return 280;
		case EDBSettlementType::OasisTown: return 220;
		case EDBSettlementType::TavernTown: return 200;
		case EDBSettlementType::TempleSettlement: return 160;
		case EDBSettlementType::RiceVillage: return 160;
		case EDBSettlementType::Village: return 140;
		case EDBSettlementType::RiverSettlement: return 130;
		case EDBSettlementType::BorderOutpost: return 120;
		case EDBSettlementType::FishingVillage: return 110;
		case EDBSettlementType::MountainVillage: return 100;
		case EDBSettlementType::ForestSettlement: return 90;
		case EDBSettlementType::SnowSettlement: return 80;
		}
		return 100;
	}
}

UDBWorldStateComponent::UDBWorldStateComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;
}

void UDBWorldStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDBWorldStateComponent, ReplicatedTotalHours);
	DOREPLIFETIME(UDBWorldStateComponent, Regions);
	DOREPLIFETIME(UDBWorldStateComponent, StoryFlags);
	DOREPLIFETIME(UDBWorldStateComponent, DefeatedVassals);
	DOREPLIFETIME(UDBWorldStateComponent, bFinalRegionOpen);
	DOREPLIFETIME(UDBWorldStateComponent, Settlements);
}

void UDBWorldStateComponent::BeginPlay()
{
	Super::BeginPlay();

	const float MinutesPerDay = FMath::Max(1.f, UDBGameSettings::Get().RealMinutesPerGameDay);
	GameHoursPerRealSecond = 24.0 / (static_cast<double>(MinutesPerDay) * 60.0);

	if (GetOwner()->HasAuthority())
	{
		EnsureRegionsRegistered();
		EnsureSettlementsRegistered();
		ReplicatedTotalHours = State.Clock.TotalHours;
		SyncReplicatedView();
	}
	ClockAnchorHours = ReplicatedTotalHours;
	ClockAnchorWorldSeconds = GetWorld()->GetTimeSeconds();
}

void UDBWorldStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	State.Advance(DeltaTime * GameHoursPerRealSecond);
	ClockAnchorHours = State.Clock.TotalHours;
	ClockAnchorWorldSeconds = GetWorld()->GetTimeSeconds();

	// Settlements simulate whole game hours (one every two real minutes by default); usually nothing happens here.
	std::vector<R::FSettlementEvent> Events;
	State.AdvanceSettlements(Events);
	for (const R::FSettlementEvent& Event : Events)
	{
		const bool bAttack = Event.Kind == R::ESettlementEventKind::DemonAttack || Event.Kind == R::ESettlementEventKind::AttackRepelled;
		const FString Text = FString::Printf(TEXT("Settlement %hs: %hs %d (%hs) at day %d %02d:00"), Event.SettlementId.c_str(), R::ToString(Event.Kind),
			Event.Amount, R::ToString(Event.Building), static_cast<int32>(Event.AtHours / 24.0) + 1, static_cast<int32>(FMath::Fmod(Event.AtHours, 24.0)));
		if (bAttack)
		{
			UE_LOG(LogDBWorld, Display, TEXT("%s"), *Text);
			LastAttackHours.Add(DBBridge::ToFName(Event.SettlementId), static_cast<float>(Event.AtHours));
		}
		else
		{
			UE_LOG(LogDBWorld, Log, TEXT("%s"), *Text);
		}
		OnSettlementEvent.Broadcast(Event);
	}
	if (!Events.empty())
	{
		ViewSyncAccumulator = FMath::Max(ViewSyncAccumulator, 9.f); // refresh the view soon
	}

	ClockSendAccumulator += DeltaTime;
	if (ClockSendAccumulator >= UDBGameSettings::Get().ClockReplicationInterval)
	{
		ClockSendAccumulator = 0.f;
		ReplicatedTotalHours = State.Clock.TotalHours;
	}

	// Region recovery is slow; refresh the quantized view a few times per minute.
	ViewSyncAccumulator += DeltaTime;
	if (ViewSyncAccumulator >= 10.f)
	{
		ViewSyncAccumulator = 0.f;
		SyncReplicatedView();
	}
}

void UDBWorldStateComponent::EnsureRegionsRegistered()
{
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	if (!Data)
	{
		return;
	}
	for (const UDBRegionDefinition* Region : Data->GetAllRegions())
	{
		const std::string Id = DBBridge::ToStd(Region->RegionId);
		if (!State.FindRegion(Id))
		{
			State.AddRegion(Id, DBBridge::CastEnum<R::ERegionKind>(Region->Kind));
		}
	}
}

void UDBWorldStateComponent::RestoreFromRecord(const R::FWorldState& InState)
{
	State = InState;
	if (const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this))
	{
		State.SharedQuests.Reconcile(Data->GetQuestDatabase());
	}
	EnsureRegionsRegistered();
	EnsureSettlementsRegistered();
	ReplicatedTotalHours = State.Clock.TotalHours;
	ClockAnchorHours = State.Clock.TotalHours;
	ClockAnchorWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	SyncReplicatedView();
}

void UDBWorldStateComponent::SetStoryFlag(FName Flag)
{
	if (GetOwner()->HasAuthority() && !Flag.IsNone())
	{
		State.StoryFlags.insert(DBBridge::ToStd(Flag));
		SyncReplicatedView();
	}
}

void UDBWorldStateComponent::NotifyBossDefeated(FName BossId, EDBBossRank Rank, FName RegionId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	State.RecordBossDefeat(DBBridge::ToStd(BossId));

	const std::string Region = DBBridge::ToStd(RegionId);
	switch (Rank)
	{
	case EDBBossRank::MidBoss:
		State.MarkMidBossDefeated(Region);
		break;
	case EDBBossRank::Vassal:
		if (State.MarkVassalDefeated(Region))
		{
			UE_LOG(LogDBWorld, Log, TEXT("Region %s liberated (%d/%d vassals)"), *RegionId.ToString(), State.CountDefeatedVassals(),
				R::NumVassalRegions);
		}
		break;
	case EDBBossRank::DemonKing:
		State.StoryFlags.insert("Story.DemonKingDefeated");
		break;
	case EDBBossRank::WorldBoss:
		break;
	}

	SyncReplicatedView();
	MulticastBossDefeated(BossId, Rank);
}

void UDBWorldStateComponent::MulticastBossDefeated_Implementation(FName BossId, EDBBossRank Rank)
{
	OnBossDefeated.Broadcast(BossId, Rank);
}

void UDBWorldStateComponent::SetTimeOfDay(float Hour)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	const double CurrentDayStart = FMath::FloorToDouble(State.Clock.TotalHours / 24.0) * 24.0;
	double Target = CurrentDayStart + FMath::Clamp(static_cast<double>(Hour), 0.0, 23.999);
	if (Target < State.Clock.TotalHours)
	{
		Target += 24.0; // time never runs backwards; jump to the next day instead
	}
	State.Advance(Target - State.Clock.TotalHours);
	ReplicatedTotalHours = State.Clock.TotalHours;
	ClockAnchorHours = ReplicatedTotalHours;
	ClockAnchorWorldSeconds = GetWorld()->GetTimeSeconds();
	SyncReplicatedView();
}

double UDBWorldStateComponent::GetCurrentTotalHours() const
{
	const UWorld* World = GetWorld();
	const double Elapsed = World ? World->GetTimeSeconds() - ClockAnchorWorldSeconds : 0.0;
	return ClockAnchorHours + FMath::Max(0.0, Elapsed) * GameHoursPerRealSecond;
}

float UDBWorldStateComponent::GetTimeOfDay() const
{
	R::FWorldClock Clock;
	Clock.TotalHours = GetCurrentTotalHours();
	return Clock.GetTimeOfDay();
}

int32 UDBWorldStateComponent::GetDay() const
{
	R::FWorldClock Clock;
	Clock.TotalHours = GetCurrentTotalHours();
	return Clock.GetDay();
}

bool UDBWorldStateComponent::IsNight() const
{
	R::FWorldClock Clock;
	Clock.TotalHours = GetCurrentTotalHours();
	return Clock.IsNight();
}

bool UDBWorldStateComponent::GetRegionState(FName RegionId, FDBRegionStateView& OutState) const
{
	if (const FDBRegionStateView* Found = Regions.FindByPredicate([RegionId](const FDBRegionStateView& Entry) { return Entry.RegionId == RegionId; }))
	{
		OutState = *Found;
		return true;
	}
	return false;
}

float UDBWorldStateComponent::GetDemonInfluence(FName RegionId) const
{
	FDBRegionStateView View;
	return GetRegionState(RegionId, View) ? View.DemonInfluenceByte / 255.f : 0.f;
}

void UDBWorldStateComponent::SyncReplicatedView()
{
	Regions.Reset();
	for (const R::FRegionState& Region : State.GetRegions())
	{
		FDBRegionStateView& View = Regions.AddDefaulted_GetRef();
		View.RegionId = DBBridge::ToFName(Region.RegionId);
		View.Control = DBBridge::CastEnum<EDBRegionControl>(Region.Control);
		View.DemonInfluenceByte = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Region.DemonInfluence * 255.f), 0, 255));
		View.bMidBossDefeated = Region.bMidBossDefeated;
		View.bVassalDefeated = Region.bVassalDefeated;
	}

	StoryFlags.Reset();
	for (const std::string& Flag : State.StoryFlags)
	{
		StoryFlags.Add(DBBridge::ToFName(Flag));
	}

	DefeatedVassals = State.CountDefeatedVassals();
	bFinalRegionOpen = State.IsFinalRegionOpen();

	Settlements.Reset();
	auto ToByte = [](float Value) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value * 255.f), 0, 255)); };
	for (const R::FSettlementState& Settlement : State.Settlements)
	{
		FDBSettlementView& View = Settlements.AddDefaulted_GetRef();
		View.SettlementId = DBBridge::ToFName(Settlement.SettlementId);
		View.Population = Settlement.GetPopulation();
		View.Guards = Settlement.Guards;
		View.FoodDays = View.Population > 0 ? FMath::FloorToInt(Settlement.Stocks.Food / View.Population) : 0;
		View.ProsperityByte = ToByte(Settlement.Prosperity);
		View.SecurityByte = ToByte(Settlement.Security);
		View.ThreatByte = ToByte(Settlement.Threat);
		float Worst = 1.f;
		for (const R::FSettlementBuilding& Building : Settlement.Buildings)
		{
			Worst = FMath::Min(Worst, Building.Condition);
		}
		View.WorstConditionByte = ToByte(Worst);
		View.bHungry = Settlement.HungryHours > 0;
		const float* Attack = LastAttackHours.Find(View.SettlementId);
		View.LastAttackHours = Attack ? *Attack : -1.f;
	}
	OnWorldStateChanged.Broadcast();
}

void UDBWorldStateComponent::EnsureSettlementsRegistered()
{
	const TArray<FDBRealmRegion>& RealmRegions = DBRealm::GetRegions();
	for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
	{
		const std::string Id = TCHAR_TO_UTF8(Site.Name);
		if (State.FindSettlement(Id))
		{
			continue;
		}
		const int32 RegionIndex = DBRealm::FindRegionIndex(Site.Center.X, Site.Center.Y);
		const FName RegionId = RealmRegions.IsValidIndex(RegionIndex) ? RealmRegions[RegionIndex].RegionId : FName(TEXT("Capital"));
		// The capital and its harbor carry the story: they can suffer but never fall.
		const bool bStoryProtected = Site.Type == EDBSettlementType::Capital || FCString::Stricmp(Site.Name, TEXT("Hauptstadthafen")) == 0;
		State.AddSettlement(R::MakeSettlement(Id, DBBridge::ToStd(RegionId), GetStartingResidents(Site.Type), bStoryProtected, FCrc::StrCrc32(Site.Name)));
	}
}

void UDBWorldStateComponent::SkipHours(double Hours)
{
	if (!GetOwner()->HasAuthority() || Hours <= 0.0)
	{
		return;
	}
	State.Advance(Hours);
	ReplicatedTotalHours = State.Clock.TotalHours;
	ClockAnchorHours = ReplicatedTotalHours;
	ClockAnchorWorldSeconds = GetWorld()->GetTimeSeconds();
	SyncReplicatedView();
}

bool UDBWorldStateComponent::GetSettlementView(FName SettlementId, FDBSettlementView& OutView) const
{
	for (const FDBSettlementView& View : Settlements)
	{
		if (View.SettlementId == SettlementId)
		{
			OutView = View;
			return true;
		}
	}
	return false;
}

void UDBWorldStateComponent::OnRep_View()
{
	OnWorldStateChanged.Broadcast();
}

void UDBWorldStateComponent::OnRep_Clock()
{
	ClockAnchorHours = ReplicatedTotalHours;
	ClockAnchorWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}
