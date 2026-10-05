// Region life (Phase 11, docs/REGIONS.md). The fourteen vassal regions and DAS ENDE are full of demons: packs roam
// around every player (more at night, fewer once the region is contested, none by day after liberation; level and stats
// from the region's band, DarkBlood::Rules::GetRegionalPackBudget). Each vassal region has a demon camp led by its
// commander (mid-boss); breaking it makes the region contested. Server-authoritative; camps build their look on every
// machine (static, unlit fire light only while the camp stands).
#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#include "DBRegionLife.generated.h"

class ADBBossCharacter;
class ADBEnemyCharacter;
class APlayerState;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace DBRegions
{
	/** 1..14 for "Region01".."Region14", 15 for DAS ENDE, 0 otherwise. */
	DARKBLOOD_API int32 GetRegionNumber(FName RegionId);
	/** What the region's demons are called ("Frostdaemon"). */
	DARKBLOOD_API FText GetDemonName(FName RegionId);
	/** Quest id of the region's demons ("Demon_Region07"). */
	DARKBLOOD_API FName GetDemonId(FName RegionId);
	/** Where the region's demon camp stands (world cm); zero outside the vassal regions. */
	DARKBLOOD_API FVector GetCampLocation(FName RegionId);
	/** Server: one demon of the region, level and stats from the region's recommended band. */
	DARKBLOOD_API ADBEnemyCharacter* SpawnDemon(UWorld* World, FName RegionId, const FVector& Location, const FRotator& Rotation, uint32 Seed, bool bElite);
}

/** Demon packs around the players (on the realm director). */
UCLASS(ClassGroup = DarkBlood)
class DARKBLOOD_API UDBRegionLifeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBRegionLifeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server: spawns a pack near the player now (tests); false when the region has none or no spot was found. */
	bool SpawnPackNear(APlayerState* Player, bool bIgnoreBudget);
	/** Server: packs and their demons (DBRegionDump). */
	void LogState() const;
	int32 CountLivingDemons() const;

private:
	struct FPack
	{
		TArray<TWeakObjectPtr<ADBEnemyCharacter>> Demons;
		FVector Center = FVector::ZeroVector;
		FName Region;
	};

	void UpdatePacks();
	int32 CountPacksNear(const FVector& Location, float Radius) const;

	TArray<FPack> Packs;
	TMap<TWeakObjectPtr<APlayerState>, double> NextPackAt;
	float UpdateTimer = 0.f;
	uint32 SpawnCounter = 0;
};

/** A vassal region's demon camp; its commander and guards appear when players come near. */
UCLASS()
class DARKBLOOD_API ADBDemonCamp : public AActor
{
	GENERATED_BODY()

public:
	ADBDemonCamp();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FName GetRegionId() const { return RegionId; }
	FName GetCommanderId() const;
	bool IsBroken() const { return bBroken; }
	ADBBossCharacter* GetCommander() const { return Commander.Get(); }

	/** Server: one camp per vassal region (idempotent). */
	static void SpawnCamps(UWorld* World);
	static ADBDemonCamp* Find(const UWorld* World, FName RegionId);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Camp();
	void BuildCamp();
	void StartEncounter(int32 Players);
	void EndEncounter();
	int32 CountPlayersWithin(float Radius) const;

	UPROPERTY(ReplicatedUsing = OnRep_Camp)
	FName RegionId;

	UPROPERTY(ReplicatedUsing = OnRep_Camp)
	bool bBroken = false;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Region")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Region")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Region")
	TObjectPtr<UPointLightComponent> FireLight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** Banners and fire: hidden once the camp is broken. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> StandingParts;

	TWeakObjectPtr<ADBBossCharacter> Commander;
	TArray<TWeakObjectPtr<ADBEnemyCharacter>> Guards;
	float UpdateTimer = 0.f;
	float EmptySeconds = 0.f;
	bool bBuilt = false;
	float VegetationTimer = 0.f;
	TSet<TWeakObjectPtr<AActor>> ClearedVegetation;
};
