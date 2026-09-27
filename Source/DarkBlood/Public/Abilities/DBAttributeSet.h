// Core character attributes (health, stamina, mana and combat values).
// Base values are driven by the rules core (class growth + level + gear), see UDBProgressionComponent.
#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "DBAttributeSet.generated.h"

#define DB_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)           \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)   \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                 \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                 \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_MULTICAST_DELEGATE_ThreeParams(FDBOutOfHealthEvent, AActor* /*Instigator*/, AActor* /*Causer*/, float /*DamageMagnitude*/);

UCLASS()
class DARKBLOOD_API UDBAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UDBAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/** Broadcast on the server when health reaches zero. */
	mutable FDBOutOfHealthEvent OnOutOfHealth;

	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, Health);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, MaxHealth);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, Stamina);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, MaxStamina);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, Mana);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, MaxMana);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, HealthRegen);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, StaminaRegen);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, ManaRegen);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, AttackPower);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, SpellPower);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, CritChance);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, Armor);
	DB_ATTRIBUTE_ACCESSORS(UDBAttributeSet, IncomingDamage);

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealthRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_StaminaRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ManaRegen(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpellPower(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CritChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);

private:
	void ClampToMax(const FGameplayAttribute& Attribute, float& NewValue) const;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vitals", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Vitals", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Vitals", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Stamina;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Vitals", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxStamina;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "Vitals", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Mana;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "Vitals", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxMana;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegen, Category = "Regeneration", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData HealthRegen;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegen, Category = "Regeneration", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData StaminaRegen;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ManaRegen, Category = "Regeneration", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ManaRegen;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData AttackPower;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpellPower, Category = "Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData SpellPower;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChance, Category = "Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CritChance;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Combat", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Armor;

	/** Meta attribute written by the damage execution; converted into health loss on the server. Not replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "Meta", meta = (AllowPrivateAccess = true))
	FGameplayAttributeData IncomingDamage;

	bool bOutOfHealth = false;
};
