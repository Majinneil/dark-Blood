// Shared base for player characters and (later) humanoid NPCs/enemies.
// Gameplay never depends on a specific mesh: visuals are swappable, see docs/VISUAL_PIPELINE.md.
#pragma once

#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"
#include "GameFramework/Character.h"

#include "DBCharacterBase.generated.h"

class ADBCharacterBase;
class UDBAbilitySystemComponent;
class UDBCharacterVisualComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDBOnCharacterDied, ADBCharacterBase*, Character);

/** Combat affiliation. Neutral characters are never valid targets. */
UENUM(BlueprintType)
enum class EDBTeam : uint8
{
	Neutral,
	Players,
	Demons,
};

UCLASS(Abstract)
class DARKBLOOD_API ADBCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	ADBCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDBAbilitySystemComponent* GetDBAbilitySystemComponent() const;

	/** Combat cues (hit, block, parry, stagger) reach the presentation here on every machine (DBCombatFeedback). */
	virtual void HandleGameplayCue(AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Character")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Character")
	EDBTeam GetTeam() const { return Team; }

	/** Name shown in logs, nameplates and the lock-on UI. */
	virtual FString GetCombatDisplayName() const;

	/** Level used by the damage formula (armor mitigation). */
	virtual int32 GetCombatLevel() const { return 1; }

	/** Actor attacks turn towards (lock-on target for players, current target for AI). */
	virtual AActor* GetCombatFocusTarget() const { return nullptr; }

	/** Actions that ignore movement input (attacking, staggered, dodging ...). */
	bool IsMovementInputBlocked() const;

	float GetKnockbackScale() const { return KnockbackScale; }

	/** Presentation layer (body mesh, animation set, appearance). Never read by gameplay. */
	UDBCharacterVisualComponent* GetVisuals() const { return Visuals; }

	/** Shows/hides the greybox body; it stays hidden while a real body mesh is applied. */
	void SetPlaceholderVisible(bool bVisible);

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|Character")
	FDBOnCharacterDied OnDied;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: health reached zero. */
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageMagnitude);

	UFUNCTION()
	virtual void OnRep_IsDead();

	/** Local presentation of death (ragdoll/montage once animations exist). Runs on server and clients. */
	virtual void PlayDeathPresentation();

	/** Server: brings a dead character back (enemies/dummies that respawn in place; players get a new pawn). */
	void Revive();

	/** Undoes PlayDeathPresentation. Runs on server and clients. */
	virtual void PlayRevivePresentation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Combat")
	EDBTeam Team = EDBTeam::Neutral;

	/** Multiplier for knockback from hit reactions (0 = anchored, e.g. training dummies, large bosses). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Combat", meta = (ClampMin = 0))
	float KnockbackScale = 1.f;

	void BindToAttributeSet(UAbilitySystemComponent* ASC);

	/** Resolved by subclasses: players use the PlayerState's ASC, NPCs own one. */
	TWeakObjectPtr<UDBAbilitySystemComponent> CachedAbilitySystem;

	/**
	 * DEVELOPMENT PLACEHOLDER visual (engine cylinder) shown while no skeletal mesh is assigned.
	 * Hidden automatically as soon as a real mesh is set on the Mesh component.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Visuals")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dark Blood|Visuals")
	TObjectPtr<UDBCharacterVisualComponent> Visuals;

	/** Tint of the placeholder body (players blue, enemies red, NPCs gold). */
	UPROPERTY(EditDefaultsOnly, Category = "Dark Blood|Visuals")
	FLinearColor PlaceholderColor = FLinearColor(0.5f, 0.5f, 0.5f);

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

private:
	FDelegateHandle OutOfHealthHandle;
};
