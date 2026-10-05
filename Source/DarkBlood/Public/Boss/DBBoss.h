// Boss framework (Phase 10, docs/BOSS_FRAMEWORK.md).
// ADBBossCharacter: melee AI plus telegraphed special attacks from its definition (slam, charge, volley, hazard, summon),
// phases at health thresholds (the demon king's three forms), enrage, co-op scaling (more demons, split attention, area
// pressure, moderate health). On death it reports to the world state (vassals free their region), rewards everyone in
// the fight with skill points and personal loot. ADBBossTelegraph: warning ring / zone that hits after a delay.
// ADBBossArena: where a vassal or the demon king waits; a blood barrier closes while the fight lasts, DAS ENDE's arenas
// stay sealed until its conditions are met.
#pragma once

#include "Character/DBEnemyCharacter.h"
#include "Combat/DBCombatStatics.h"

#include "DBBoss.generated.h"

class ADBBossArena;
class UDBBossDefinition;
class UDBMeleeAIComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class DARKBLOOD_API ADBBossCharacter : public ADBEnemyCharacter
{
	GENERATED_BODY()

public:
	ADBBossCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, between deferred spawn and FinishSpawning: definition, players in the fight, stat multiplier (dungeon stage). */
	void Setup(const UDBBossDefinition* InDefinition, int32 InPlayerCount, float StatMultiplier = 1.f, ADBBossArena* InArena = nullptr);

	const UDBBossDefinition* GetDefinition() const;
	FName GetBossId() const { return BossId; }
	int32 GetPhase() const { return Phase; }
	float GetFightSeconds() const { return FightSeconds; }
	float GetHealthFraction() const;
	/** Server: demons it summoned that still live. */
	int32 CountLivingAdds() const;
	int32 GetSpecialAttacks() const { return SpecialAttacks; }

	static ADBBossCharacter* SpawnBoss(UWorld* World, const UDBBossDefinition* Definition, const FVector& Location, const FRotator& Rotation, int32 PlayerCount,
		float StatMultiplier = 1.f, ADBBossArena* Arena = nullptr);

protected:
	virtual void BeginPlay() override;
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageMagnitude) override;

private:
	UFUNCTION()
	void OnRep_Boss();
	void ApplyLook();
	void EnterPhase(int32 NewPhase);
	void UseSpecialAttack();
	TArray<APawn*> GetPlayersInFight(float Radius) const;
	FDBHitParams MakeHit(float Damage, float Poise, bool bKnockdown) const;
	void NotifyPlayers(const FText& Text, float Radius) const;

	UPROPERTY(ReplicatedUsing = OnRep_Boss)
	FName BossId;

	UPROPERTY(ReplicatedUsing = OnRep_Boss)
	int32 Phase = 0;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Boss")
	TObjectPtr<UDBMeleeAIComponent> MeleeAI;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Boss")
	TObjectPtr<UPointLightComponent> Aura;

	TWeakObjectPtr<ADBBossArena> Arena;
	TArray<TWeakObjectPtr<ADBEnemyCharacter>> Adds;
	int32 PlayerCount = 1;
	float BaseAttackPower = 0.f;
	float FightSeconds = 0.f;
	float SpecialCooldown = 4.f;
	float InvulnerableSeconds = 0.f;
	float ChargeSeconds = 0.f;
	TArray<TWeakObjectPtr<AActor>> ChargeHits;
	int32 SpecialAttacks = 0;
};

/** Telegraphed attack: a growing ring (warning), then the hit; zones keep burning for a while. */
UCLASS()
class DARKBLOOD_API ADBBossTelegraph : public AActor
{
	GENERATED_BODY()

public:
	ADBBossTelegraph();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: Duration 0 = a single hit when the warning ends; otherwise a hit every second for Duration. */
	void Arm(ADBBossCharacter* InBoss, float InRadius, float InDelay, float InDuration, const FDBHitParams& InHit, const FLinearColor& InColor);

private:
	UFUNCTION()
	void OnRep_Setup();
	void HitPlayers();

	UPROPERTY(ReplicatedUsing = OnRep_Setup)
	float Radius = 400.f;

	UPROPERTY(ReplicatedUsing = OnRep_Setup)
	float Delay = 1.f;

	UPROPERTY(ReplicatedUsing = OnRep_Setup)
	float Duration = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Setup)
	FLinearColor Color = FLinearColor::Red;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Boss")
	TObjectPtr<UStaticMeshComponent> Disc;

	TWeakObjectPtr<ADBBossCharacter> Boss;
	FDBHitParams Hit;
	float Age = 0.f;
	float NextHit = 0.f;
	bool bActive = false;
};

UENUM()
enum class EDBArenaState : uint8
{
	Idle,
	Fighting,
	Defeated,
};

/** Arena of a vassal or the demon king. */
UCLASS()
class DARKBLOOD_API ADBBossArena : public AActor
{
	GENERATED_BODY()

public:
	ADBBossArena();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetBoss(FName InBossId);
	FName GetBossId() const { return BossId; }
	EDBArenaState GetArenaState() const { return ArenaState; }
	ADBBossCharacter* GetBoss() const { return Boss.Get(); }

	/** Server: one arena per vassal and the demon king (idempotent). */
	static void SpawnArenas(UWorld* World);
	static ADBBossArena* Find(const UWorld* World, FName BossId);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_State();
	void BuildRing();
	void StartFight(const TArray<APawn*>& Players);
	void EndFight(bool bVictory);
	TArray<APawn*> GetPlayersInside(float Fraction) const;
	/** Empty when the arena may open; otherwise why it is sealed. */
	FText GetSealReason() const;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FName BossId;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	EDBArenaState ArenaState = EDBArenaState::Idle;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Boss")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Boss")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Posts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Barrier;

	TWeakObjectPtr<ADBBossCharacter> Boss;
	float UpdateTimer = 0.f;
	float EmptySeconds = 0.f;
	TMap<TWeakObjectPtr<APawn>, float> LastSealNotice;
	bool bRingBuilt = false;
	float VegetationTimer = 0.f;
	TSet<TWeakObjectPtr<AActor>> ClearedVegetation;
};
