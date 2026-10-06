// Animation without an Animation Blueprint, for characters that bring their own skeleton and animations (the Paragon
// heroes and minions): idle and run loops blended by ground speed (the run plays faster or slower with the speed),
// with the default montage slot on top so abilities play their montages (attack, hit, death) exactly like on the
// mannequin. The loops come from the active profile's animation set (IdleAnimation, RunAnimation, RunSpeed).
// One native node graph per character: cheap, no per-skeleton blueprint to author.
#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"

#include "DBNativeLocomotionAnimInstance.generated.h"

class UAnimSequenceBase;

USTRUCT()
struct FDBNativeLocomotionProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FDBNativeLocomotionProxy() = default;
	explicit FDBNativeLocomotionProxy(UAnimInstance* InAnimInstance);

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return &Slot; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override;

private:
	FAnimNode_SequencePlayer_Standalone Idle;
	FAnimNode_SequencePlayer_Standalone Run;
	FAnimNode_TwoWayBlend Blend;
	FAnimNode_Slot Slot;
};

UCLASS(Transient, NotBlueprintable)
class DARKBLOOD_API UDBNativeLocomotionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Read by the proxy before each update (game thread). */
	UAnimSequenceBase* GetIdle() const { return IdleSequence; }
	UAnimSequenceBase* GetRun() const { return RunSequence; }
	float GetBlendAlpha() const { return BlendAlpha; }
	float GetRunRate() const { return RunRate; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FDBNativeLocomotionProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }

private:
	/** Fetches the loops from the owner's active visual profile (once they are known). */
	void ResolveAnimations();

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> IdleSequence;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> RunSequence;

	float RunSpeed = 400.f;
	float BlendAlpha = 0.f;
	float RunRate = 1.f;
};
