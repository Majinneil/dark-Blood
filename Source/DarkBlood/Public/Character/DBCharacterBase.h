// Shared base for player characters and (later) humanoid NPCs/enemies.
// Gameplay never depends on a specific mesh: visuals are swappable, see docs/VISUAL_PIPELINE.md.
#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"

#include "DBCharacterBase.generated.h"

class ADBCharacterBase;
class UDBAbilitySystemComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDBOnCharacterDied, ADBCharacterBase*, Character);

UCLASS(Abstract)
class DARKBLOOD_API ADBCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ADBCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDBAbilitySystemComponent* GetDBAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Character")
	bool IsDead() const { return bIsDead; }

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|Character")
	FDBOnCharacterDied OnDied;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: health reached zero. */
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageMagnitude);

	UFUNCTION()
	virtual void OnRep_IsDead();

	/** Local presentation of death (ragdoll/montage in Phase 2). Runs on server and clients. */
	virtual void PlayDeathPresentation();

	void BindToAttributeSet(UAbilitySystemComponent* ASC);

	/** Resolved by subclasses: players use the PlayerState's ASC, NPCs own one. */
	TWeakObjectPtr<UDBAbilitySystemComponent> CachedAbilitySystem;

	/**
	 * DEVELOPMENT PLACEHOLDER visual (engine cylinder) shown while no skeletal mesh is assigned.
	 * Hidden automatically as soon as a real mesh is set on the Mesh component.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Visuals")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

private:
	FDelegateHandle OutOfHealthHandle;
};
