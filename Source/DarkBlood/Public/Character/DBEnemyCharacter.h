// Base for enemies and other combat NPCs: owns its ability system (players keep theirs on the PlayerState).
// Kills grant XP to the killer and report a Kill quest event with EnemyId.
#pragma once

#include "Character/DBCharacterBase.h"

#include "DBEnemyCharacter.generated.h"

class UDBAttributeSet;
class UDBGameplayAbility;
class UTextRenderComponent;
struct FOnAttributeChangeData;

/** Endgame multipliers of one enemy (New Game+ cycle, echo rank or Abyss floor; DarkBloodRules/Endgame.h). */
struct FDBEndgameScale
{
	float Health = 1.f;
	float Damage = 1.f;
	float Experience = 1.f;
	float RarityBonus = 0.f;
	int32 LevelBonus = 0;
};

UCLASS()
class DARKBLOOD_API ADBEnemyCharacter : public ADBCharacterBase
{
	GENERATED_BODY()

public:
	ADBEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	/** Returns the owned component directly: attribute rep-notifies can arrive on clients before BeginPlay. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual FString GetCombatDisplayName() const override;
	virtual int32 GetCombatLevel() const override { return Level; }
	virtual AActor* GetCombatFocusTarget() const override { return CurrentTarget.Get(); }
	virtual void Tick(float DeltaSeconds) override;

	/** Server: turn to Target and perform the attack ability. Returns false if the attack could not start. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Enemy")
	bool AttackTarget(AActor* Target);

	/** Server: nearest living player within Radius (cm). */
	AActor* FindNearestPlayer(float Radius) const;

	/** Server: a hit from Attacker landed (AI aggro). */
	virtual void OnAttackedBy(AActor* Attacker);

	FName GetEnemyId() const { return EnemyId; }

	/** Server, between a deferred spawn and FinishSpawning: level, stats times StatMultiplier (health, armor, attack, poise,
	 *  XP), shown name and quest id (regional demons, camp guards). Name and level replicate for the nameplate. */
	void ConfigureSpawn(int32 InLevel, float StatMultiplier, const FText& InDisplayName, FName InEnemyId, FName InLootTableId = NAME_None);

	/** Server, between a deferred spawn and FinishSpawning: endgame multipliers of an echo or Abyss enemy (docs/ENDGAME.md).
	 *  Without it every demon takes the scale of the world's New Game+ cycle when it begins play. */
	void SetEndgameScale(const FDBEndgameScale& Scale);

	/** Added to the rarity of its loot (New Game+, echoes, the Abyss). */
	float GetLootRarityBonus() const { return LootRarityBonus; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageMagnitude) override;
	virtual void PlayDeathPresentation() override;
	virtual void PlayRevivePresentation() override;

	void InitializeCombatState();
	/** Server, at BeginPlay: the endgame multipliers (set by SetEndgameScale or the world's cycle) on stats, level and rewards. */
	void ApplyEndgameScale();
	void OnHealthChanged(const FOnAttributeChangeData& Change);
	void RefreshNameplate();
	void RespawnInPlace();

	/** Quest target id reported on death (Kill objectives), e.g. "TrainingDummy". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy")
	FName EnemyId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Dark Blood|Enemy")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Dark Blood|Enemy|Stats", meta = (ClampMin = 1))
	int32 Level = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy|Stats", meta = (ClampMin = 1))
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy|Stats", meta = (ClampMin = 0))
	float MaxPoise = 40.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy|Stats", meta = (ClampMin = 0))
	float Armor = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy|Stats", meta = (ClampMin = 0))
	float AttackPower = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy|Rewards", meta = (ClampMin = 0))
	int32 XpReward = 10;

	/** Personal loot: every player nearby rolls this table on its own (co-op friendly, no loot stealing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy|Rewards")
	FName LootTableId;

	/** Abilities granted on spawn (a hit reaction is always granted). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy|Abilities")
	TArray<TSubclassOf<UDBGameplayAbility>> Abilities;

	/** Ability used by AttackTarget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy|Abilities")
	TSubclassOf<UDBGameplayAbility> AttackAbility;

	/** Revive at the spawn point after death (training dummies) instead of being removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy")
	bool bRespawnInPlace = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Enemy", meta = (ClampMin = 0.1, Units = "s"))
	float RespawnSeconds = 5.f;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Enemy")
	TObjectPtr<UDBAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Enemy")
	TObjectPtr<UDBAttributeSet> Attributes;

	/** DEVELOPMENT nameplate with name and health. Replaced by a widget with the UI pass. */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Visuals")
	TObjectPtr<UTextRenderComponent> Nameplate;

	TWeakObjectPtr<AActor> CurrentTarget;

private:
	FTransform SpawnTransform;
	bool bEndgameScaleSet = false;
	FDBEndgameScale Endgame;
	float LootRarityBonus = 0.f;
	FTimerHandle RespawnTimer;
};
