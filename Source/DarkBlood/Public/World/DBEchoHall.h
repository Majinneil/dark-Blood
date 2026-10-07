// The Hall of Echoes (Phase 16, docs/ENDGAME.md): a ring of seventeen memorial stones floating over the western sea,
// one for every vassal and the demon king. A stone wakes once its boss has fallen in this world (in this or an earlier
// New Game+ cycle). Touching it calls the boss back as an echo - a pale, spirit-blue copy that grows stronger with every
// echo already beaten (DarkBlood::Rules::GetEchoScale). An echo changes nothing in the world: no region, no story, no
// skill points - only experience, loot and the echo rank. A gate outside the capital leads in (a site of DBDungeon).
#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/DBInteractable.h"

#include "DBEchoHall.generated.h"

class ADBBossCharacter;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace DBEchoHall
{
	/** Center of the hall floor (world cm). */
	DARKBLOOD_API FVector GetCenter();
	/** Server: the hall, its stones and the way out (idempotent). */
	DARKBLOOD_API void SpawnHall(UWorld* World);
	/** Server: moves a player into the hall (sealed until the demon king fell once). */
	DARKBLOOD_API bool Enter(APlayerController* User);
	/** The hall's radius (cm); players beyond it count as gone. */
	constexpr float Radius = 4600.f;
}

UCLASS()
class DARKBLOOD_API ADBEchoHall : public AActor
{
	GENERATED_BODY()

public:
	ADBEchoHall();

	virtual void Tick(float DeltaSeconds) override;

	static ADBEchoHall* Find(const UWorld* World);

	/** Server: calls the echo of a remembered boss (one echo at a time). */
	bool SummonEcho(FName BossId, APlayerController* User);
	ADBBossCharacter* GetEcho() const { return Echo.Get(); }
	TArray<APawn*> GetPlayersInside() const;

protected:
	virtual void BeginPlay() override;

private:
	/** Every machine: floor, pillars, lights. */
	void Build();
	/** Server: stones and the exit. */
	void SpawnFixtures();

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Echoes")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	TWeakObjectPtr<ADBBossCharacter> Echo;
	float UpdateTimer = 0.f;
	float EmptySeconds = 0.f;
};

/** One memorial stone: dark while its boss is unbeaten, glowing in its colour once remembered. */
UCLASS()
class DARKBLOOD_API ADBEchoStone : public AActor, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBEchoStone();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override { return User != nullptr && bRemembered; }
	virtual void Interact(APlayerController* User) override;
	virtual float GetInteractionRange() const override { return 400.f; }

	void Setup(FName InBossId);
	FName GetBossId() const { return BossId; }
	int32 GetEchoRank() const { return EchoRank; }
	bool IsRemembered() const { return bRemembered; }

private:
	UFUNCTION()
	void OnRep_Stone();

	UPROPERTY(ReplicatedUsing = OnRep_Stone)
	FName BossId;

	UPROPERTY(ReplicatedUsing = OnRep_Stone)
	bool bRemembered = false;

	UPROPERTY(ReplicatedUsing = OnRep_Stone)
	int32 EchoRank = 0;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Echoes")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Echoes")
	TObjectPtr<UStaticMeshComponent> Pillar;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Echoes")
	TObjectPtr<UStaticMeshComponent> Flame;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Echoes")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlameMaterial;

	float UpdateTimer = 0.f;
};
