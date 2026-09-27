#include "Abilities/DBGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Core/DBGameplayTags.h"

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
