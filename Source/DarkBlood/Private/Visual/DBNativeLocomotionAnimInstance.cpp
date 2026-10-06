#include "Visual/DBNativeLocomotionAnimInstance.h"

#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Pawn.h"
#include "Visual/DBAnimationSetDefinition.h"
#include "Visual/DBCharacterVisualComponent.h"
#include "Visual/DBCharacterVisualDefinition.h"

FDBNativeLocomotionProxy::FDBNativeLocomotionProxy(UAnimInstance* InAnimInstance)
	: FAnimInstanceProxy(InAnimInstance)
{
}

void FDBNativeLocomotionProxy::Initialize(UAnimInstance* InAnimInstance)
{
	// Slot (montages) <- blend(idle, run) by speed.
	Idle.SetLoopAnimation(true);
	Run.SetLoopAnimation(true);
	Blend.A.SetLinkNode(&Idle);
	Blend.B.SetLinkNode(&Run);
	Blend.Alpha = 0.f;
	Slot.Source.SetLinkNode(&Blend);
	Slot.SlotName = FName(TEXT("DefaultSlot"));
	FAnimInstanceProxy::Initialize(InAnimInstance);
}

void FDBNativeLocomotionProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Append({&Slot, &Blend, &Idle, &Run});
}

void FDBNativeLocomotionProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UDBNativeLocomotionAnimInstance* Instance = Cast<UDBNativeLocomotionAnimInstance>(InAnimInstance))
	{
		Idle.SetSequence(Instance->GetIdle());
		Run.SetSequence(Instance->GetRun() ? Instance->GetRun() : Instance->GetIdle());
		Run.SetPlayRate(Instance->GetRunRate());
		Blend.Alpha = Instance->GetBlendAlpha();
	}
}

void UDBNativeLocomotionAnimInstance::ResolveAnimations()
{
	const APawn* Pawn = TryGetPawnOwner();
	const UDBCharacterVisualComponent* Visuals = Pawn ? Pawn->FindComponentByClass<UDBCharacterVisualComponent>() : nullptr;
	const UDBCharacterVisualDefinition* Profile = Visuals ? Visuals->GetActiveProfile() : nullptr;
	const UDBAnimationSetDefinition* Set = Profile ? Profile->AnimationSet.Get() : nullptr;
	if (!Set)
	{
		return;
	}
	IdleSequence = Set->IdleAnimation.LoadSynchronous();
	RunSequence = Set->RunAnimation.LoadSynchronous();
	RunSpeed = FMath::Max(50.f, Set->RunSpeed);
}

void UDBNativeLocomotionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!IdleSequence)
	{
		ResolveAnimations();
	}
	const APawn* Pawn = TryGetPawnOwner();
	const float Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;
	// Ease into the run over the first 40 % of the run speed; the run loop follows the actual speed.
	const float Target = FMath::Clamp(Speed / (RunSpeed * 0.4f), 0.f, 1.f);
	BlendAlpha = FMath::FInterpTo(BlendAlpha, Target, DeltaSeconds, 10.f);
	RunRate = FMath::Clamp(Speed / RunSpeed, 0.5f, 1.8f);
}
