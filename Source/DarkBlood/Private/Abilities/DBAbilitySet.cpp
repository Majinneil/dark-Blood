#include "Abilities/DBAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "Abilities/DBGameplayAbility.h"
#include "DarkBlood.h"
#include "GameplayEffect.h"

void FDBAbilitySetHandles::TakeFromAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	for (const FGameplayAbilitySpecHandle& Handle : Abilities)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}
	for (const FActiveGameplayEffectHandle& Handle : Effects)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	Abilities.Reset();
	Effects.Reset();
}

void UDBAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, FDBAbilitySetHandles* OutHandles, UObject* SourceObject) const
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FDBAbilitySetAbility& Entry : GrantedAbilities)
	{
		if (!Entry.Ability)
		{
			UE_LOG(LogDarkBlood, Warning, TEXT("Ability set %s has an empty ability entry"), *GetName());
			continue;
		}
		FGameplayAbilitySpec Spec(Entry.Ability->GetDefaultObject<UDBGameplayAbility>(), Entry.AbilityLevel);
		Spec.SourceObject = SourceObject;
		if (Entry.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (OutHandles)
		{
			OutHandles->Abilities.Add(Handle);
		}
	}

	for (const FDBAbilitySetEffect& Entry : GrantedEffects)
	{
		if (!Entry.Effect)
		{
			continue;
		}
		const FActiveGameplayEffectHandle Handle =
			ASC->ApplyGameplayEffectToSelf(Entry.Effect->GetDefaultObject<UGameplayEffect>(), Entry.EffectLevel, ASC->MakeEffectContext());
		if (OutHandles)
		{
			OutHandles->Effects.Add(Handle);
		}
	}
}
