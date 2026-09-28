// Per-player persistent state: character identity, GAS, progression, inventory, personal quests.
// Living on the PlayerState (not the pawn) means nothing is lost when the character dies.
#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "Abilities/DBAbilitySet.h"
#include "Core/DBTypes.h"

#include "DarkBloodRules/Records.h"

#include "DBPlayerState.generated.h"

class ADBPlayerState;
class ADBRegionVolume;
class UDBAbilitySystemComponent;
class UDBAttributeSet;
class UDBGameplayAbility;
class UDBInventoryComponent;
class UDBProgressionComponent;
class UDBQuestComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDBOnProfileChanged, ADBPlayerState*, PlayerState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDBOnRegionChanged, ADBPlayerState*, PlayerState, FName, RegionId);

UCLASS()
class DARKBLOOD_API ADBPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADBPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	/** Players are shown with their character name, never with the account name. */
	virtual FString GetPlayerNameCustom() const override;

	UDBAbilitySystemComponent* GetDBAbilitySystemComponent() const { return AbilitySystemComponent; }
	const UDBAttributeSet* GetAttributeSet() const { return AttributeSet; }
	UDBProgressionComponent* GetProgression() const { return Progression; }
	UDBInventoryComponent* GetInventory() const { return Inventory; }
	UDBQuestComponent* GetPersonalQuests() const { return PersonalQuests; }

	const FDBCharacterProfile& GetProfile() const { return Profile; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Character", meta = (DisplayName = "Get Profile"))
	FDBCharacterProfile K2_GetProfile() const { return Profile; }

	/** Short name NPCs use ("Jin Akagi" -> "Jin"). */
	UFUNCTION(BlueprintPure, Category = "Dark Blood|Character")
	FString GetCallName() const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Character")
	bool IsCharacterReady() const { return bCharacterReady; }

	// ---- Persistence (server) -----------------------------------------------------------------

	/** Applies a validated character record to all components and grants class abilities. */
	void ApplyCharacterRecord(const DarkBlood::Rules::FCharacterRecord& Record);
	DarkBlood::Rules::FCharacterRecord BuildCharacterRecord() const;

	void GrantSkillAbility(TSubclassOf<UDBGameplayAbility> AbilityClass, int32 Level, const FGameplayTag& InputTag);

	FName GetRespawnPointId() const { return RespawnPointId; }
	void SetRespawnPointId(FName InRespawnPointId) { RespawnPointId = InRespawnPointId; }

	// ---- Regions ------------------------------------------------------------------------------

	void EnterRegionVolume(ADBRegionVolume* Volume);
	void ExitRegionVolume(ADBRegionVolume* Volume);

	/** Open world: region of the realm layout at the player's position (volumes win where they overlap). */
	void SetRealmRegion(FName RegionId);

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	FName GetCurrentRegionId() const { return CurrentRegionId; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	bool HasDiscoveredRegion(FName RegionId) const { return DiscoveredRegions.Contains(RegionId); }

	/** Danger of the current region relative to this character ("GEFAHRENSTUFE"). Returns false outside any region. */
	UFUNCTION(BlueprintPure, Category = "Dark Blood|World")
	bool GetCurrentDanger(EDBDangerTier& OutTier, int32& OutRecommendedMin, int32& OutRecommendedMax, int32& OutPower) const;

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|Character")
	FDBOnProfileChanged OnProfileChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|World")
	FDBOnRegionChanged OnRegionChanged;

private:
	UFUNCTION() void OnRep_Profile();
	UFUNCTION() void OnRep_CurrentRegion();

	void UpdateCurrentRegion();
	void DiscoverRegion(FName RegionId);

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood")
	TObjectPtr<UDBAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UDBAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood")
	TObjectPtr<UDBProgressionComponent> Progression;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood")
	TObjectPtr<UDBInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood")
	TObjectPtr<UDBQuestComponent> PersonalQuests;

	UPROPERTY(ReplicatedUsing = OnRep_Profile)
	FDBCharacterProfile Profile;

	UPROPERTY(Replicated)
	bool bCharacterReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentRegion)
	FName CurrentRegionId;

	UPROPERTY(Replicated)
	TArray<FName> DiscoveredRegions;

	UPROPERTY(Replicated)
	TArray<FName> Titles;

	/** Server only. */
	FName RespawnPointId;
	int64 PlayTimeSecondsAtLoad = 0;
	double LoadedAtWorldSeconds = 0.0;
	FDBAbilitySetHandles ClassAbilityHandles;
	TArray<TWeakObjectPtr<ADBRegionVolume>> OverlappingRegionVolumes;

	FName RealmRegionId;
};
