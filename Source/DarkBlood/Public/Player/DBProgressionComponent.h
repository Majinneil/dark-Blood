// Level, XP, skill points and skill tree ranks of one character. Lives on the PlayerState so it
// survives death/respawn. All mutations are server-only; clients receive replicated values.
#pragma once

#include "Components/ActorComponent.h"

#include "DarkBloodRules/Progression.h"
#include "DarkBloodRules/SkillTree.h"

#include "DBProgressionComponent.generated.h"

class ADBPlayerState;
class UDBProgressionComponent;
class UDBClassDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDBOnProgressionChanged, UDBProgressionComponent*, Progression);

USTRUCT(BlueprintType)
struct FDBSkillRank
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	FName NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	int32 Rank = 0;
};

UCLASS(ClassGroup = (DarkBlood), meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBProgressionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static const DarkBlood::Rules::FProgressionRules& GetRules();

	// ---- Server API ---------------------------------------------------------------------------

	/** Grants XP (quests, bosses, dungeons, discovery, bounties, demon hunting). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Progression")
	void AwardXp(int64 Amount);

	/** Grants skill points (bosses and important quests). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Progression")
	void AwardSkillPoints(int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Progression")
	void SetLevelForDevelopment(int32 NewLevel);

	/**
	 * Recomputes attribute base values from class growth, level and gear via the rules core.
	 * bRestoreVitals fills health/stamina/mana (level-up, respawn, load).
	 */
	void RecalculateAttributes(bool bRestoreVitals);

	void RestoreFromRecord(const DarkBlood::Rules::FProgressionState& State, const DarkBlood::Rules::FSkillTreeState& Skills);
	const DarkBlood::Rules::FProgressionState& GetState() const { return State; }
	const DarkBlood::Rules::FSkillTreeState& GetSkills() const { return Skills; }

	// ---- Client requests ----------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Progression")
	void RequestUnlockSkill(FName NodeId);

	// ---- Replicated view ----------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Progression")
	int32 GetLevel() const { return Level; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Progression")
	int64 GetXpIntoLevel() const { return XpIntoLevel; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Progression")
	int64 GetXpToNextLevel() const { return XpToNextLevel; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Progression")
	int32 GetUnspentSkillPoints() const { return UnspentSkillPoints; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Progression")
	int32 GetSkillRank(FName NodeId) const;

	/** Power rating ("Staerke") used for region danger display. */
	UFUNCTION(BlueprintPure, Category = "Dark Blood|Progression")
	int32 GetPowerRating() const;

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|Progression")
	FDBOnProgressionChanged OnProgressionChanged;

private:
	UFUNCTION(Server, Reliable)
	void ServerUnlockSkill(FName NodeId);

	UFUNCTION()
	void OnRep_Progression();

	ADBPlayerState* GetOwningPlayerState() const;
	const UDBClassDefinition* GetClassDefinition() const;
	void PushReplicatedState();

	DarkBlood::Rules::FProgressionState State;
	DarkBlood::Rules::FSkillTreeState Skills;

	UPROPERTY(ReplicatedUsing = OnRep_Progression)
	int32 Level = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Progression)
	int64 XpIntoLevel = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression)
	int64 XpToNextLevel = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression)
	int32 UnspentSkillPoints = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progression)
	TArray<FDBSkillRank> SkillRanks;

	/** Gear score is replicated so every client can show the correct power rating. */
	UPROPERTY(ReplicatedUsing = OnRep_Progression)
	float GearScore = 0.f;
};
