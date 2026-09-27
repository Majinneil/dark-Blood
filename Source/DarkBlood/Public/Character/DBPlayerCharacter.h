// Third-person player character. Abilities, attributes and inventory live on the PlayerState.
#pragma once

#include "Character/DBCharacterBase.h"

#include "DBPlayerCharacter.generated.h"

class ADBPlayerState;
class UCameraComponent;
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

protected:
	void InitAbilityActorInfo();
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

	/** DEVELOPMENT nameplate (text render). Replaced by a UMG widget with party UI in Phase 3. */
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Visuals")
	TObjectPtr<UTextRenderComponent> Nameplate;
};
