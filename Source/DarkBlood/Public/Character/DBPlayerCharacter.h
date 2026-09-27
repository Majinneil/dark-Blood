// Third-person player character. Abilities, attributes and inventory live on the PlayerState.
#pragma once

#include "Character/DBCharacterBase.h"
#include "GameplayTagContainer.h"

#include "DBPlayerCharacter.generated.h"

class ADBPlayerState;
class UCameraComponent;
class UDBInteractionComponent;
class UDBLockOnComponent;
class USpringArmComponent;
class UTextRenderComponent;
class UDBInputConfig;
struct FInputActionValue;
struct FGameplayTag;

UCLASS()
class DARKBLOOD_API ADBPlayerCharacter : public ADBCharacterBase
{
	GENERATED_BODY()

public:
	ADBPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual FString GetCombatDisplayName() const override;
	virtual int32 GetCombatLevel() const override;
	virtual AActor* GetCombatFocusTarget() const override;

	UDBLockOnComponent* GetLockOn() const { return LockOn; }

	/** Development/testing: behaves exactly like pressing / releasing the bound input. */
	void PressAbilityInput(FGameplayTag InputTag) { Input_AbilityPressed(InputTag); }
	void ReleaseAbilityInput(FGameplayTag InputTag) { Input_AbilityReleased(InputTag); }

protected:
	void InitAbilityActorInfo();
	void OnDoubleJumpTagChanged(const FGameplayTag Tag, int32 NewCount);
	virtual void OnJumped_Implementation() override;
	void RefreshNameplate();

	UFUNCTION()
	void RefreshNameplateFromState(ADBPlayerState* ChangedState);

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_AbilityPressed(FGameplayTag InputTag);
	void Input_AbilityReleased(FGameplayTag InputTag);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dark Blood|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dark Blood|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dark Blood|Combat")
	TObjectPtr<UDBLockOnComponent> LockOn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dark Blood|Interaction")
	TObjectPtr<UDBInteractionComponent> Interaction;

	/** DEVELOPMENT nameplate (text render). Replaced by a UMG widget with party UI in Phase 3. */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Visuals")
	TObjectPtr<UTextRenderComponent> Nameplate;
};
