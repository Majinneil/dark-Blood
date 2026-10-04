// Target lock for the local player: picks a target in front of the camera (rules core scoring + line of
// sight), keeps the camera on it and makes the character strafe. The server only receives the target as a
// hint for facing attacks; it never trusts the client for damage.
#pragma once

#include "Components/ActorComponent.h"

#include "DBLockOnComponent.generated.h"

UCLASS(ClassGroup = (DarkBlood), meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBLockOnComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Local: lock onto the best target, or release the current one. */
	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Lock-On")
	void ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Lock-On")
	void ClearLockOn();

	/** Local: switch to the next target to the left (Direction < 0) or right (> 0). */
	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Lock-On")
	bool SwitchTarget(float Direction);

	/** Local: feed horizontal look input while locked; a quick flick switches targets. */
	void AddSwitchInput(float YawInput);

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Lock-On")
	AActor* GetLockTarget() const { return LockTarget.Get(); }

	bool IsLockedOn() const { return LockTarget.IsValid(); }

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetLockTarget(AActor* NewTarget);

	/** Best candidate; with SideFilter != 0 only candidates on that side of the current view. */
	AActor* FindBestTarget(float SideFilter, const AActor* Exclude) const;

	void SetLockTarget(AActor* NewTarget);
	void ApplyStrafeMode(bool bLocked);

	/** Camera turn speed towards the target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Lock-On")
	float CameraInterpSpeed = 10.f;

	/** Downward camera pitch while locked (degrees). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Lock-On")
	float LockedPitch = -15.f;

	/** Accumulated look input that counts as a flick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Lock-On")
	float SwitchThreshold = 5.f;

private:
	TWeakObjectPtr<AActor> LockTarget;
	float SwitchAccumulator = 0.f;
	float SwitchCooldown = 0.f;
};
