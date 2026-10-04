// Native base for DARK BLOOD Animation Blueprints (reparent a locomotion / motion-matching ABP to this class).
// Exposes the gameplay state as plain variables so the anim graph never queries gameplay systems itself:
// locomotion, combat states from GAS tags, lock-on, look-at target, emotion and a procedural blink.
// Read-only view of gameplay - the anim graph never drives damage or movement (see 13_ANIMATION_PIPELINE).
#pragma once

#include "Animation/AnimInstance.h"

#include "DBAnimInstance.generated.h"

UCLASS()
class DARKBLOOD_API UDBAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// ---- Locomotion ---------------------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Locomotion")
	float GroundSpeed = 0.f;

	/** Movement direction relative to the facing, -180..180 (strafing while locked on). */
	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Locomotion")
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Locomotion")
	float VerticalSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Locomotion")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Locomotion")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Locomotion")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Locomotion")
	bool bIsFlying = false;

	// ---- Combat (from gameplay tags) -------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Combat")
	bool bIsLockedOn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Combat")
	bool bIsBlocking = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Combat")
	bool bIsAttacking = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Combat")
	bool bIsDodging = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Combat")
	bool bIsStaggered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Combat")
	bool bIsKnockedDown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Combat")
	bool bIsDead = false;

	/** True while a combat stance is held (recently attacked, blocking or locked on): combat locomotion set. */
	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Combat")
	bool bInCombatStance = false;

	// ---- Face / head -------------------------------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Face")
	FVector LookAtLocation = FVector::ZeroVector;

	/** 0..1, eased in and out; 0 when the target is behind the character. */
	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Face")
	float LookAtAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Face")
	FName Emotion = TEXT("Neutral");

	/** 0 = open, 1 = closed. Drive a curve / morph target / MetaHuman face control with it. */
	UPROPERTY(BlueprintReadOnly, Category = "Dark Blood|Face")
	float BlinkAlpha = 0.f;

private:
	void UpdateBlink(float DeltaSeconds);

	float CombatStanceSeconds = 0.f;
	float BlinkTimer = 2.f;
	float BlinkPhase = -1.f;
};
