#include "Abilities/DBAttributeSet.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Combat/DBCombatStatics.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include "DarkBloodRules/Combat.h"

namespace R = DarkBlood::Rules;

float UDBAttributeSet::GetPoiseRecoverDelaySeconds()
{
	return R::FPoiseRules().RecoverDelaySeconds;
}

UDBAttributeSet::UDBAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitMana(50.f);
	InitMaxMana(50.f);
	InitCritChance(0.05f);
	InitPoise(50.f);
	InitMaxPoise(50.f);
}

void UDBAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
	// Poise is visible to everyone (boss/enemy poise bars).
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, MaxPoise, COND_None, REPNOTIFY_Always);
	// Secondary stats only matter to the owning player's UI.
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, HealthRegen, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, StaminaRegen, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, ManaRegen, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, AttackPower, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, SpellPower, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, CritChance, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDBAttributeSet, Armor, COND_OwnerOnly, REPNOTIFY_Always);
}

void UDBAttributeSet::ClampToMax(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute() || Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetCritChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	}
}

void UDBAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampToMax(Attribute, NewValue);
}

void UDBAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampToMax(Attribute, NewValue);
}

void UDBAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}
	// Lowering a maximum must never leave the current value above it.
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		ASC->ApplyModToAttribute(GetHealthAttribute(), EGameplayModOp::Override, NewValue);
	}
	else if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
	{
		ASC->ApplyModToAttribute(GetStaminaAttribute(), EGameplayModOp::Override, NewValue);
	}
	else if (Attribute == GetMaxManaAttribute() && GetMana() > NewValue)
	{
		ASC->ApplyModToAttribute(GetManaAttribute(), EGameplayModOp::Override, NewValue);
	}
	else if (Attribute == GetMaxPoiseAttribute() && GetPoise() > NewValue)
	{
		ASC->ApplyModToAttribute(GetPoiseAttribute(), EGameplayModOp::Override, NewValue);
	}

	if (Attribute == GetHealthAttribute() && NewValue > 0.f)
	{
		bOutOfHealth = false;
	}
}

void UDBAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Damage > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth()));
		}

		if (GetHealth() <= 0.f && !bOutOfHealth)
		{
			bOutOfHealth = true;
			const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
			OnOutOfHealth.Broadcast(Context.GetOriginalInstigator(), Context.GetEffectCauser(), Damage);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingPoiseDamageAttribute())
	{
		HandleIncomingPoiseDamage(Data);
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
		HandleGuardBreak(Data);
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseAttribute())
	{
		SetPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
	}
	else if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
	}
}

void UDBAttributeSet::HandleIncomingPoiseDamage(const FGameplayEffectModCallbackData& Data)
{
	const float PoiseDamage = GetIncomingPoiseDamage();
	SetIncomingPoiseDamage(0.f);
	UDBAbilitySystemComponent* ASC = Cast<UDBAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	if (!ASC || GetHealth() <= 0.f)
	{
		return; // the dead do not stagger
	}

	FGameplayTagContainer SpecTags;
	Data.EffectSpec.GetAllAssetTags(SpecTags);
	// A raised guard prevents knockdowns (unblockable attacks are never blocked in the first place).
	const bool bForceKnockdown = SpecTags.HasTagExact(DBTags::Damage_Knockdown) && !ASC->HasMatchingGameplayTag(DBTags::State_Blocking);

	const R::FPoiseResult Result = R::ApplyPoiseDamage(GetPoise(), GetMaxPoise(), PoiseDamage, bForceKnockdown);
	SetPoise(Result.NewPoise);
	ASC->AddTimedLooseTag(DBTags::State_PoiseRecoverDelay, GetPoiseRecoverDelaySeconds());

	if (R::InterruptsAction(Result.Reaction))
	{
		UE_LOG(LogDBCombat, Log, TEXT("%s: poise broken -> %hs"), *DBCombat::GetCombatName(ASC->GetAvatarActor()), R::ToString(Result.Reaction));
		DBCombat::SendHitReact(ASC, Result.Reaction, Data.EffectSpec.GetEffectContext().GetEffectCauser());
	}
}

void UDBAttributeSet::HandleGuardBreak(const FGameplayEffectModCallbackData& Data)
{
	UDBAbilitySystemComponent* ASC = Cast<UDBAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	// Blocking a hit with no stamina left breaks the guard.
	if (ASC && GetStamina() <= 0.f && Data.EvaluatedData.Magnitude < 0.f && ASC->HasMatchingGameplayTag(DBTags::State_Blocking))
	{
		UE_LOG(LogDBCombat, Log, TEXT("%s: guard broken"), *DBCombat::GetCombatName(ASC->GetAvatarActor()));
		DBCombat::SendHitReact(ASC, R::EHitReaction::Stagger, Data.EffectSpec.GetEffectContext().GetEffectCauser());
	}
}

void UDBAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, Health, OldValue); }
void UDBAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, MaxHealth, OldValue); }
void UDBAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, Stamina, OldValue); }
void UDBAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, MaxStamina, OldValue); }
void UDBAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, Mana, OldValue); }
void UDBAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, MaxMana, OldValue); }
void UDBAttributeSet::OnRep_HealthRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, HealthRegen, OldValue); }
void UDBAttributeSet::OnRep_StaminaRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, StaminaRegen, OldValue); }
void UDBAttributeSet::OnRep_ManaRegen(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, ManaRegen, OldValue); }
void UDBAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, AttackPower, OldValue); }
void UDBAttributeSet::OnRep_SpellPower(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, SpellPower, OldValue); }
void UDBAttributeSet::OnRep_CritChance(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, CritChance, OldValue); }
void UDBAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, Armor, OldValue); }
void UDBAttributeSet::OnRep_Poise(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, Poise, OldValue); }
void UDBAttributeSet::OnRep_MaxPoise(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UDBAttributeSet, MaxPoise, OldValue); }
