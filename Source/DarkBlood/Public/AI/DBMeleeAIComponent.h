// Minimal code-driven melee AI (server only): acquire -> chase -> attack -> recover, with a leash.
// Works without navigation data or behaviour-tree assets (steers directly towards the target), so enemies
// fight in any test map. Real encounters get StateTree/EQS-based AI in later phases; this stays as fallback.
#pragma once

#include "Components/ActorComponent.h"

#include "DBMeleeAIComponent.generated.h"

class ADBEnemyCharacter;

UENUM(BlueprintType)
enum class EDBMeleeAIState : uint8
{
	Idle,
	Chase,
	Attack,
	Recover,
	ReturnHome,
};

UCLASS(ClassGroup = (DarkBlood), meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBMeleeAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBMeleeAIComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	EDBMeleeAIState GetState() const { return State; }

	/** Aggro onto an attacker even outside the sight radius (called when damaged). */
	void NotifyAttackedBy(AActor* Attacker);

protected:
	virtual void BeginPlay() override;

	void SetState(EDBMeleeAIState NewState);
	AActor* FindTarget() const;
	bool CanSee(const AActor* Target) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|AI", meta = (Units = "cm"))
	float SightRadius = 1500.f;

	/** Target is dropped beyond this distance or when the enemy strays this far from home. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|AI", meta = (Units = "cm"))
	float LeashRadius = 2500.f;

	/** Starts an attack when the target is this close (should be below the attack's range). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|AI", meta = (Units = "cm"))
	float AttackDistance = 170.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|AI", meta = (Units = "s"))
	float MinAttackCooldown = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|AI", meta = (Units = "s"))
	float MaxAttackCooldown = 2.4f;

	/** How often a new target is searched while idle (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|AI", meta = (Units = "s"))
	float SearchInterval = 0.5f;

private:
	ADBEnemyCharacter* GetEnemy() const;
	void MoveTowards(const FVector& Location, float AcceptRadius);

	EDBMeleeAIState State = EDBMeleeAIState::Idle;
	TWeakObjectPtr<AActor> Target;
	FVector HomeLocation = FVector::ZeroVector;
	float StateTime = 0.f;
	float Cooldown = 0.f;
	float SearchTimer = 0.f;
};
