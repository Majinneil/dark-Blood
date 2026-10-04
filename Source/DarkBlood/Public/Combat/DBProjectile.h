// Server-authoritative projectile (mage bolts). Replicated so everyone sees it; only the server applies hits.
#pragma once

#include "GameFramework/Actor.h"
#include "Combat/DBCombatStatics.h"

#include "DBProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class DARKBLOOD_API ADBProjectile : public AActor
{
	GENERATED_BODY()

public:
	ADBProjectile();

	/** Server: configure right after spawning. ChainCount = additional enemies the hit jumps to. */
	void Launch(AActor* InShooter, const FDBHitParams& InHit, float Speed, int32 InChainCount, const FLinearColor& Color);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBlocked(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnRep_Color();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(ReplicatedUsing = OnRep_Color)
	FLinearColor Color = FLinearColor::White;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void HitTarget(AActor* Target);

	TWeakObjectPtr<AActor> Shooter;
	FDBHitParams HitParams;
	int32 ChainCount = 0;
	TArray<TWeakObjectPtr<AActor>> AlreadyHit;
	bool bSpent = false;
};
