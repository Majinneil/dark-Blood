#include "Combat/DBWeaponEffects.h"

#include "AbilitySystemComponent.h"
#include "Abilities/DBCombatEffects.h"
#include "Character/DBCharacterBase.h"
#include "Combat/DBCombatStatics.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBItemDefinition.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Inventory/DBInventoryComponent.h"
#include "Player/DBPlayerState.h"

namespace
{
	constexpr float ChainRadius = 600.f;

	FGameplayTag DamageTypeOf(const FDBWeaponEffect& Effect)
	{
		return Effect.DamageType.IsValid() ? Effect.DamageType : DBTags::Damage_Type_Physical;
	}

	/** State tag granted while a damage-over-time effect of this type runs (one at a time per type). */
	FGameplayTag DamageOverTimeState(const FGameplayTag& DamageType)
	{
		if (DamageType == DBTags::Damage_Type_Fire)
		{
			return DBTags::State_Burning;
		}
		if (DamageType == DBTags::Damage_Type_Poison)
		{
			return DBTags::State_Poisoned;
		}
		if (DamageType == DBTags::Damage_Type_Physical)
		{
			return DBTags::State_Bleeding;
		}
		return DBTags::State_Afflicted;
	}

	/** The victim avoided the hit entirely (i-frames, parry) or is already dead: no weapon effects. */
	bool AvoidedHit(const UAbilitySystemComponent& TargetASC)
	{
		return TargetASC.HasMatchingGameplayTag(DBTags::State_Invulnerable) || TargetASC.HasMatchingGameplayTag(DBTags::State_ParryWindow) ||
			   TargetASC.HasMatchingGameplayTag(DBTags::State_CounterStance) || TargetASC.HasMatchingGameplayTag(DBTags::State_Dead);
	}

	FDBHitParams ExtraHit(const FDBHitParams& Hit, const FDBWeaponEffect& Effect)
	{
		FDBHitParams Extra;
		Extra.BaseDamage = Hit.BaseDamage * Effect.Magnitude;
		Extra.DamageType = DamageTypeOf(Effect);
		Extra.AttackerLevel = Hit.AttackerLevel;
		Extra.bUnparryable = true; // the main hit already decided about the parry
		return Extra;
	}

	void ApplyDamageOverTime(UAbilitySystemComponent& SourceASC, UAbilitySystemComponent& TargetASC, const FDBHitParams& Hit,
		const FDBWeaponEffect& Effect)
	{
		const FGameplayTag DamageType = DamageTypeOf(Effect);
		const FGameplayTag State = DamageOverTimeState(DamageType);
		if (TargetASC.HasMatchingGameplayTag(State) || Effect.Duration <= 0.f || Effect.Magnitude <= 0.f)
		{
			return;
		}
		const FGameplayEffectSpecHandle Spec = SourceASC.MakeOutgoingSpec(
			UDBDamageOverTimeEffect::StaticClass(), static_cast<float>(FMath::Max(1, Hit.AttackerLevel)), SourceASC.MakeEffectContext());
		if (!Spec.IsValid())
		{
			return;
		}
		Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_Damage, Effect.Magnitude);
		Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_PoiseDamage, 0.f);
		Spec.Data->SetDuration(Effect.Duration, true);
		Spec.Data->AddDynamicAssetTag(DamageType);
		Spec.Data->AddDynamicAssetTag(DBTags::Damage_OverTime);
		Spec.Data->AddDynamicAssetTag(DBTags::Damage_Unblockable);
		Spec.Data->AddDynamicAssetTag(DBTags::Damage_Unparryable);
		Spec.Data->DynamicGrantedTags.AddTag(State);
		SourceASC.ApplyGameplayEffectSpecToTarget(*Spec.Data, &TargetASC);
	}

	void Heal(UAbilitySystemComponent& ASC, float Amount)
	{
		if (Amount <= 0.f)
		{
			return;
		}
		const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(UDBHealEffect::StaticClass(), 1.f, ASC.MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_Magnitude, Amount);
			ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}

	ADBCharacterBase* FindChainTarget(ADBCharacterBase& Attacker, const ADBCharacterBase& From)
	{
		UWorld* World = Attacker.GetWorld();
		if (!World)
		{
			return nullptr;
		}
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DBWeaponChain), false, &Attacker);
		World->OverlapMultiByObjectType(Overlaps, From.GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(ChainRadius), Params);
		ADBCharacterBase* Best = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		for (const FOverlapResult& Overlap : Overlaps)
		{
			ADBCharacterBase* Candidate = Cast<ADBCharacterBase>(Overlap.GetActor());
			if (!Candidate || Candidate == &From || !DBCombat::CanTarget(&Attacker, Candidate))
			{
				continue;
			}
			const float Distance = FVector::DistSquared(Candidate->GetActorLocation(), From.GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Candidate;
			}
		}
		return Best;
	}
}

namespace DBWeaponEffects
{
	const UDBItemDefinition* GetMainHandWeapon(const ADBCharacterBase* Character)
	{
		const ADBPlayerState* PlayerState = Character ? Character->GetPlayerState<ADBPlayerState>() : nullptr;
		const UDBInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventory() : nullptr;
		const FName ItemId = Inventory ? Inventory->GetEquipped(EDBEquipSlot::MainHand).ItemId : NAME_None;
		const UDBGameDataSubsystem* Data = ItemId.IsNone() ? nullptr : UDBGameDataSubsystem::Get(Character);
		return Data ? Data->FindItem(ItemId) : nullptr;
	}

	void ModifyOutgoingHit(const ADBCharacterBase* Attacker, FDBHitParams& Hit)
	{
		const UDBItemDefinition* Weapon = GetMainHandWeapon(Attacker);
		if (!Weapon || !Attacker->HasAuthority())
		{
			return;
		}
		for (const FDBWeaponEffect& Effect : Weapon->WeaponEffects)
		{
			if (Effect.Kind == EDBWeaponEffectKind::PoiseBreak && FMath::FRand() < Effect.Chance)
			{
				Hit.PoiseDamage *= 1.f + Effect.Magnitude;
			}
		}
	}

	void ApplyOnHit(ADBCharacterBase* Attacker, ADBCharacterBase* Target, const FDBHitParams& Hit)
	{
		const UDBItemDefinition* Weapon = GetMainHandWeapon(Attacker);
		if (!Weapon || Weapon->WeaponEffects.IsEmpty() || !Target || !Attacker->HasAuthority())
		{
			return;
		}
		UAbilitySystemComponent* SourceASC = Attacker->GetAbilitySystemComponent();
		UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
		if (!SourceASC || !TargetASC || AvoidedHit(*TargetASC))
		{
			return;
		}
		for (const FDBWeaponEffect& Effect : Weapon->WeaponEffects)
		{
			if (Effect.Kind == EDBWeaponEffectKind::PoiseBreak || FMath::FRand() >= Effect.Chance)
			{
				continue;
			}
			switch (Effect.Kind)
			{
			case EDBWeaponEffectKind::ElementalDamage:
				DBCombat::ApplyHit(SourceASC, TargetASC, ExtraHit(Hit, Effect));
				break;
			case EDBWeaponEffectKind::DamageOverTime:
				ApplyDamageOverTime(*SourceASC, *TargetASC, Hit, Effect);
				break;
			case EDBWeaponEffectKind::Lifesteal:
				Heal(*SourceASC, Hit.BaseDamage * Effect.Magnitude);
				break;
			case EDBWeaponEffectKind::ChainStrike:
				if (ADBCharacterBase* Next = FindChainTarget(*Attacker, *Target))
				{
					DBCombat::ApplyHit(SourceASC, Next->GetAbilitySystemComponent(), ExtraHit(Hit, Effect));
				}
				break;
			default:
				break;
			}
			UE_LOG(LogDBCombat, Log, TEXT("%s: Waffeneffekt %s (%s) -> %s"), *DBCombat::GetCombatName(Attacker),
				*StaticEnum<EDBWeaponEffectKind>()->GetNameStringByValue(static_cast<int64>(Effect.Kind)), *DamageTypeOf(Effect).ToString(),
				*DBCombat::GetCombatName(Target));
		}
	}
}
