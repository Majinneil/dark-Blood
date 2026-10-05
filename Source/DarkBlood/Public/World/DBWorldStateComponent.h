// Shared world state (story flags, regions, vassals, bosses, time of day) on the GameState.
// Server owns DarkBlood::Rules::FWorldState; clients get a compact replicated view.
#pragma once

#include "Components/ActorComponent.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/WorldState.h"

#include "DBWorldStateComponent.generated.h"

USTRUCT(BlueprintType)
struct FDBRegionStateView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	EDBRegionControl Control = EDBRegionControl::Occupied;

	/** Quantized to 0..255 to save bandwidth. */
	UPROPERTY()
	uint8 DemonInfluenceByte = 255;

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	bool bMidBossDefeated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Region")
	bool bVassalDefeated = false;
};

/** Compact replicated view of one simulated settlement (the numbers live in the rules core on the server). */
USTRUCT(BlueprintType)
struct FDBSettlementView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FName SettlementId;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	int32 Population = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	int32 Guards = 0;

	/** Days of food in store. */
	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	int32 FoodDays = 0;

	/** 0..255 */
	UPROPERTY()
	uint8 ProsperityByte = 128;

	UPROPERTY()
	uint8 SecurityByte = 128;

	UPROPERTY()
	uint8 ThreatByte = 128;

	/** Lowest building condition, 0..255. */
	UPROPERTY()
	uint8 WorstConditionByte = 255;

	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	bool bHungry = false;

	/** Game hour of the last demon attack (-1: none). */
	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	float LastAttackHours = -1.f;

	float GetProsperity() const { return ProsperityByte / 255.f; }
	float GetSecurity() const { return SecurityByte / 255.f; }
	float GetThreat() const { return ThreatByte / 255.f; }
	float GetWorstCondition() const { return WorstConditionByte / 255.f; }
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDBOnSettlementEvent, const DarkBlood::Rules::FSettlementEvent&);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDBOnWorldStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDBOnBossDefeated, FName, BossId, EDBBossRank, Rank);

UCLASS(ClassGroup = (DarkBlood), meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBWorldStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBWorldStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Server API ---------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|World")
	void SetStoryFlag(FName Flag);

	/** Records a boss kill and applies its world consequences (mid-boss: contested, vassal: liberated). Idempotent. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|World")
	void NotifyBossDefeated(FName BossId, EDBBossRank Rank, FName RegionId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|World")
	void SetTimeOfDay(float Hour);

	/** Makes sure every region definition has a state entry (new content added to an old save). */
	void EnsureRegionsRegistered();

	/** Makes sure every settlement of the open world has a simulation entry (new worlds, version 1 saves). */
	void EnsureSettlementsRegistered();

	/** Development: lets time pass (the settlements catch up on the next tick). */
	void SkipHours(double Hours);

	/** Server: everything the settlement simulation reports (attacks, births, projects ...). */
	FDBOnSettlementEvent OnSettlementEvent;

	bool GetSettlementView(FName SettlementId, FDBSettlementView& OutView) const;

	/** Server: a dungeon's guardian room was cleared (saved; demons return after three game days). */
	void NotifyDungeonCleared(FName DungeonId);
	/** Server: whether a dungeon is cleared right now. */
	bool IsDungeonCleared(FName DungeonId) const;
	const TArray<FDBSettlementView>& GetSettlementViews() const { return Settlements; }

	void RestoreFromRecord(const DarkBlood::Rules::FWorldState& InState);
	const DarkBlood::Rules::FWorldState& GetRulesState() const { return State; }

	// ---- Replicated view ----------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	bool HasStoryFlag(FName Flag) const { return StoryFlags.Contains(Flag); }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	float GetTimeOfDay() const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	int32 GetDay() const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	bool IsNight() const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	bool GetRegionState(FName RegionId, FDBRegionStateView& OutState) const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	float GetDemonInfluence(FName RegionId) const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	int32 GetDefeatedVassalCount() const { return DefeatedVassals; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	bool IsFinalRegionOpen() const { return bFinalRegionOpen; }

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|World")
	FDBOnWorldStateChanged OnWorldStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|World")
	FDBOnBossDefeated OnBossDefeated;

private:
	UFUNCTION() void OnRep_View();
	UFUNCTION() void OnRep_Clock();
	UFUNCTION(NetMulticast, Reliable) void MulticastBossDefeated(FName BossId, EDBBossRank Rank);

	double GetCurrentTotalHours() const;
	void SyncReplicatedView();

	DarkBlood::Rules::FWorldState State;
	double GameHoursPerRealSecond = 0.0;
	float ClockSendAccumulator = 0.f;
	float ViewSyncAccumulator = 0.f;

	/** Client-side extrapolation anchor. */
	double ClockAnchorHours = 8.0;
	double ClockAnchorWorldSeconds = 0.0;

	UPROPERTY(ReplicatedUsing = OnRep_Clock)
	double ReplicatedTotalHours = 8.0;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	TArray<FDBRegionStateView> Regions;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	TArray<FName> StoryFlags;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	int32 DefeatedVassals = 0;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	bool bFinalRegionOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	TArray<FDBSettlementView> Settlements;

	/** Server: game hour of each settlement's last demon attack (for the view). */
	TMap<FName, float> LastAttackHours;
};
