// Active tier of the settlement simulation (Phase 7, server): settlements near a player get villagers - as many as the
// abstract population supports, fewer at night - and lose them again when nobody is near. Demon attacks reported by the
// simulation become real fights when players are in the settlement. Lives on ADBRealmDirector.
#pragma once

#include "Components/ActorComponent.h"

#include "DBSettlementLifeComponent.generated.h"

class ADBVillagerCharacter;

namespace DarkBlood::Rules
{
	struct FSettlementEvent;
}

UCLASS()
class DARKBLOOD_API UDBSettlementLifeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBSettlementLifeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts a demon attack fight in the settlement (name prefix). False when unknown or no player is there. */
	bool StartAttackEncounter(const FString& SettlementName);

	/** Villagers currently in the settlement (name prefix); -1 when unknown. */
	int32 CountVillagers(const FString& SettlementName) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FSiteLife
	{
		TArray<TWeakObjectPtr<ADBVillagerCharacter>> Villagers;
		bool bActive = false;
		int32 SpawnCounter = 0;
	};

	void UpdateSites();
	void OnSettlementEvent(const DarkBlood::Rules::FSettlementEvent& Event);
	int32 FindSite(const FString& NamePrefix) const;
	float GetNearestPlayerDistance(int32 SiteIndex) const;
	void SpawnVillager(int32 SiteIndex);
	void SpawnAttackers(int32 SiteIndex);

	TArray<FSiteLife> Sites;
	float UpdateTimer = 0.f;
	FDelegateHandle EventHandle;
};
