// Mage "Schutzkreis": a protective circle on the ground. Demons are pushed out and burned by spiritual
// damage; allies inside take less damage (State.Warded) and, at rank 2, are healed. Server-driven, replicated
// for visuals. Bosses (vassals) break through: they are not pushed (KnockbackScale 0), only burned.
#pragma once

#include "GameFramework/Actor.h"

#include "DBWardingCircle.generated.h"

class UStaticMeshComponent;

UCLASS()
class DARKBLOOD_API ADBWardingCircle : public AActor
{
	GENERATED_BODY()

public:
	ADBWardingCircle();

	virtual void Tick(float DeltaSeconds) override;

	/** Server: configure after spawning. */
	void Setup(AActor* InCaster, float InRadius, float InDuration, float InDamagePerSecond, float InHealPerSecond);

protected:
	UFUNCTION()
	void OnRep_Radius();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(ReplicatedUsing = OnRep_Radius)
	float Radius = 800.f;

private:
	TWeakObjectPtr<AActor> Caster;
	float DamagePerSecond = 8.f;
	float HealPerSecond = 0.f;
	float PulseTimer = 0.f;
	static constexpr float PulseInterval = 0.25f;
};
