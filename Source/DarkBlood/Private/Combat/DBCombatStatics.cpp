#include "Combat/DBCombatStatics.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBCombatEffects.h"
#include "Character/DBCharacterBase.h"
#include "Core/DBGameplayTags.h"
#include "GameFramework/PlayerState.h"

namespace R = DarkBlood::Rules;

namespace DBCombat
{
	FString GetCombatName(const AActor* Actor)
	{
		if (const ADBCharacterBase* Character = Cast<ADBCharacterBase>(Actor))
		{
			return Character->GetCombatDisplayName();
		}
		if (const APlayerState* PlayerState = Cast<APlayerState>(Actor))
		{
			return PlayerState->GetPlayerName();
		}
		return GetNameSafe(Actor);
	}

	bool CanTarget(const AActor* Attacker, const AActor* Target)
	{
		const ADBCharacterBase* Source = Cast<ADBCharacterBase>(Attacker);
		const ADBCharacterBase* Victim = Cast<ADBCharacterBase>(Target);
		if (!Source || !Victim || Source == Victim || Victim->IsDead())
		{
			return false;
		}
		return Source->GetTeam() != EDBTeam::Neutral && Victim->GetTeam() != EDBTeam::Neutral && Source->GetTeam() != Victim->GetTeam();
	}

	bool ApplyHit(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, const FDBHitParams& Params)
	{
		if (!SourceASC || !TargetASC || !SourceASC->IsOwnerActorAuthoritative() || Params.BaseDamage <= 0.f)
		{
			return false;
		}
		const FGameplayEffectSpecHandle Spec =
			SourceASC->MakeOutgoingSpec(UDBDamageEffect::StaticClass(), static_cast<float>(FMath::Max(1, Params.AttackerLevel)), SourceASC->MakeEffectContext());
		if (!Spec.IsValid())
		{
			return false;
		}
		Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_Damage, Params.BaseDamage);
		Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_PoiseDamage, FMath::Max(0.f, Params.PoiseDamage));
		Spec.Data->AddDynamicAssetTag(Params.DamageType.IsValid() ? Params.DamageType : DBTags::Damage_Type_Physical);
		if (Params.bKnockdown)
		{
			Spec.Data->AddDynamicAssetTag(DBTags::Damage_Knockdown);
		}
		if (Params.bUnblockable)
		{
			Spec.Data->AddDynamicAssetTag(DBTags::Damage_Unblockable);
		}
		if (Params.bUnparryable)
		{
			Spec.Data->AddDynamicAssetTag(DBTags::Damage_Unparryable);
		}
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		return true;
	}

	void SendHitReact(UDBAbilitySystemComponent* VictimASC, R::EHitReaction Reaction, const AActor* Causer)
	{
		if (!VictimASC || Reaction == R::EHitReaction::None)
		{
			return;
		}
		FGameplayEventData Payload;
		Payload.EventTag = DBTags::Event_Combat_HitReact;
		Payload.EventMagnitude = static_cast<float>(Reaction);
		Payload.Instigator = Causer;
		Payload.Target = VictimASC->GetAvatarActor();
		VictimASC->SendGameplayEventDeferred(DBTags::Event_Combat_HitReact, Payload);
	}

	const R::FPoiseRules& GetPoiseRules()
	{
		static const R::FPoiseRules Rules;
		return Rules;
	}

	const R::FLockOnRules& GetLockOnRules()
	{
		static const R::FLockOnRules Rules;
		return Rules;
	}
}
