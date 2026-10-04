#include "World/DBWorldStateComponent.h"

#include "Core/DBGameSettings.h"
#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBRegionDefinition.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace R = DarkBlood::Rules;

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
}

void UDBWorldStateComponent::BeginPlay()
{
	Super::BeginPlay();

	const float MinutesPerDay = FMath::Max(1.f, UDBGameSettings::Get().RealMinutesPerGameDay);
	GameHoursPerRealSecond = 24.0 / (static_cast<double>(MinutesPerDay) * 60.0);

	if (GetOwner()->HasAuthority())
	{
		EnsureRegionsRegistered();
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
	OnWorldStateChanged.Broadcast();
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
