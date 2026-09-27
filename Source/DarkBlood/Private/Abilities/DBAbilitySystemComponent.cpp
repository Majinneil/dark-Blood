#include "Abilities/DBAbilitySystemComponent.h"

#include "Abilities/DBGameplayAbility.h"
#include "Engine/World.h"
#include "TimerManager.h"

UDBAbilitySystemComponent::UDBAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

void UDBAbilitySystemComponent::AddTimedLooseTag(const FGameplayTag& Tag, float Seconds)
{
	UWorld* World = GetWorld();
	if (!Tag.IsValid() || !World)
	{
		return;
	}
	SetLooseGameplayTagCount(Tag, 1);
	FTimerHandle& Handle = TimedTagTimers.FindOrAdd(Tag);
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Tag]()
	{
		SetLooseGameplayTagCount(Tag, 0);
	}), FMath::Max(Seconds, 0.001f), false);
}

float UDBAbilitySystemComponent::GetTimedTagRemaining(const FGameplayTag& Tag) const
{
	const FTimerHandle* Handle = TimedTagTimers.Find(Tag);
	const UWorld* World = GetWorld();
	if (!Handle || !World || !HasMatchingGameplayTag(Tag))
	{
		return 0.f;
	}
	return FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(*Handle));
}

void UDBAbilitySystemComponent::SendGameplayEventDeferred(const FGameplayTag& EventTag, const FGameplayEventData& Payload)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, EventTag, Payload]()
		{
			FGameplayEventData Copy = Payload;
			HandleGameplayEvent(EventTag, &Copy);
		}));
	}
}

void UDBAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputPressedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.AddUnique(Spec.Handle);
		}
	}
}

void UDBAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(Spec.Handle);
			InputHeldSpecHandles.Remove(Spec.Handle);
		}
	}
}

void UDBAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	TArray<FGameplayAbilitySpecHandle, TInlineAllocator<8>> ToActivate;

	auto GetPolicy = [](const FGameplayAbilitySpec& Spec)
	{
		const UDBGameplayAbility* Ability = Cast<UDBGameplayAbility>(Spec.Ability);
		return Ability ? Ability->GetActivationPolicy() : EDBAbilityActivationPolicy::OnInputTriggered;
	};

	auto SendInputEvent = [this](FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType)
	{
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		const FPredictionKey Key = Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
		InvokeReplicatedEvent(EventType, Spec.Handle, Key);
	};

	// Held input keeps "while active" abilities running.
	for (const FGameplayAbilitySpecHandle& Handle : InputHeldSpecHandles)
	{
		const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (Spec && !Spec->IsActive() && GetPolicy(*Spec) == EDBAbilityActivationPolicy::WhileInputActive)
		{
			ToActivate.AddUnique(Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}
		Spec->InputPressed = true;
		if (Spec->IsActive())
		{
			AbilitySpecInputPressed(*Spec);
			SendInputEvent(*Spec, EAbilityGenericReplicatedEvent::InputPressed);
		}
		else if (GetPolicy(*Spec) == EDBAbilityActivationPolicy::OnInputTriggered)
		{
			ToActivate.AddUnique(Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : ToActivate)
	{
		TryActivateAbility(Handle);
	}

	for (const FGameplayAbilitySpecHandle& Handle : InputReleasedSpecHandles)
	{
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}
		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			AbilitySpecInputReleased(*Spec);
			SendInputEvent(*Spec, EAbilityGenericReplicatedEvent::InputReleased);
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UDBAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

void UDBAbilitySystemComponent::TryActivatePassiveAbilities()
{
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		const UDBGameplayAbility* Ability = Cast<UDBGameplayAbility>(Spec.Ability);
		if (Ability && Ability->GetActivationPolicy() == EDBAbilityActivationPolicy::OnSpawn && !Spec.IsActive())
		{
			TryActivateAbility(Spec.Handle);
		}
	}
}
