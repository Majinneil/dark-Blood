// Boss definitions (Phase 10, docs/BOSS_FRAMEWORK.md): the 16 vassals, the demon king (three forms) and the dungeon
// guardian. Designs after References/DARK_BLOOD_16_VASALLEN_UND_DAEMONENKOENIG.pdf. Gameplay only knows ids, numbers and
// mechanics; the visual profile is a soft reference (placeholder: tinted demon body, aura, scale) until the hero models
// exist. DEVELOPMENT definitions are created in code (DBBosses::GetAll) until authored data assets replace them.
#pragma once

#include "Core/DBTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "DBBossDefinition.generated.h"

class UTexture2D;

/** Special attacks a boss can use (bit flags). */
namespace EDBBossMechanic
{
	constexpr int32 Slam = 1 << 0;    // telegraphed ring around the boss, knockdown
	constexpr int32 Charge = 1 << 1;  // dash at the target, hits everyone on the way
	constexpr int32 Volley = 1 << 2;  // elemental projectiles
	constexpr int32 Hazard = 1 << 3;  // burning / freezing zone under the target (under everyone with 3+ players)
	constexpr int32 Summon = 1 << 4;  // demons join the fight (more with more players)
}

USTRUCT(BlueprintType)
struct FDBBossPhase
{
	GENERATED_BODY()

	/** The phase begins once health falls to this fraction (first phase: 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float HealthThreshold = 1.f;

	/** Mechanics added in this phase (EDBBossMechanic flags; earlier phases' mechanics stay). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	int32 Mechanics = 0;

	/** Body scale in this phase (the demon king grows with every form). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float Scale = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FText Name;
};

UCLASS(BlueprintType)
class DARKBLOOD_API UDBBossDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("DBBoss"), BossId); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FName BossId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	EDBBossRank Rank = EDBBossRank::Vassal;

	/** Region the boss rules (vassals free it); its arena stands there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FName RegionId;

	/** Vassal number 1..16 (0 for others). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	int32 Order = 0;

	/** Damage.Type.* of the special attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FGameplayTag DamageType;

	/** Theme color: aura, telegraphs, projectiles, nameplate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FLinearColor Color = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stats")
	int32 Level = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stats")
	float MaxHealth = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stats")
	float AttackPower = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stats")
	float Armor = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Stats")
	float MaxPoise = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Rewards")
	int32 XpReward = 1500;

	/** Skill points for every player in the fight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Rewards")
	int32 SkillPoints = 1;

	/** Personal loot of every player nearby (guaranteed drops + rolls). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Rewards")
	FName LootTableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TArray<FDBBossPhase> Phases;

	/** Damage rises after this long (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float EnrageAfterSeconds = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Arena")
	float ArenaRadius = 2600.f;

	/** Portrait for the boss bar (concept art) and the hero visual profile once modelled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Visuals")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Visuals")
	FName VisualProfileId = TEXT("CV_Enemy_LesserDemon");

	/** Health thresholds of the phases after the first (for DarkBlood::Rules::EvaluateBossPhase). */
	TArray<float> GetPhaseThresholds() const;
	/** Mechanics available in a phase (cumulative). */
	int32 GetMechanics(int32 Phase) const;
};

namespace DBBosses
{
	/** Every boss definition (DEVELOPMENT: built in code). */
	DARKBLOOD_API const TArray<UDBBossDefinition*>& GetAll();
	DARKBLOOD_API const UDBBossDefinition* Find(FName BossId);
	/** By id, name prefix or vassal number. */
	DARKBLOOD_API const UDBBossDefinition* FindByName(const FString& IdOrName);
	/** Where the boss' arena stands (world, cm); zero for bosses without an arena (dungeon guardian). */
	DARKBLOOD_API FVector GetArenaLocation(const UDBBossDefinition& Boss);
}
