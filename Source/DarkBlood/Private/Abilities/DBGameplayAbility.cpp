#include "Abilities/DBGameplayAbility.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBCombatEffects.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/DBCharacterBase.h"
#include "Core/DBGameplayTags.h"

#include "DarkBloodRules/Combat.h"

UDBGameplayAbility::UDBGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	// Dead characters cannot use abilities.
	ActivationBlockedTags.AddTag(DBTags::State_Dead);
}

void UDBGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	if (ActivationPolicy == EDBAbilityActivationPolicy::OnSpawn && ActorInfo && ActorInfo->IsNetAuthority() && !Spec.IsActive())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

bool UDBGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return false;
	}
	const float Stamina = ASC->GetNumericAttribute(UDBAttributeSet::GetStaminaAttribute());
	return DarkBlood::Rules::CanStartStaminaAction(Stamina, GetStaminaCost());
}

void UDBGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	if (GetStaminaCost() > 0.f && HasAuthority(&ActivationInfo))
	{
		SpendStamina(GetStaminaCost());
	}
}

void UDBGameplayAbility::SpendStamina(float Amount) const
{
	UDBAbilitySystemComponent* ASC = GetDBAbilitySystem();
	if (!ASC || Amount <= 0.f || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDBStaminaCostEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_StaminaCost, -Amount);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
	if (StaminaRegenDelaySeconds > 0.f)
	{
		ASC->AddTimedLooseTag(DBTags::State_StaminaRegenDelay, StaminaRegenDelaySeconds);
	}
}

ADBCharacterBase* UDBGameplayAbility::GetDBCharacter() const
{
	return Cast<ADBCharacterBase>(GetAvatarActorFromActorInfo());
}

UDBAbilitySystemComponent* UDBGameplayAbility::GetDBAbilitySystem() const
{
	return Cast<UDBAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

void UDBGameplayAbility::PlayOptionalMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage || !GetCurrentActorInfo() || !GetCurrentActorInfo()->GetAnimInstance())
	{
		return;
	}
	if (UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, PlayRate))
	{
		Task->ReadyForActivation();
	}
}
