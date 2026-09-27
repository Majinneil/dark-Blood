// Data asset bundling abilities and effects granted together (class kit, weapon moveset, buffs).
#pragma once

#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"

#include "DBAbilitySet.generated.h"

class UAbilitySystemComponent;
class UDBGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FDBAbilitySetAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TSubclassOf<UDBGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	int32 AbilityLevel = 1;

	/** Input that activates the ability (Input.*). Leave empty for passive or AI-driven abilities. */
	UPROPERTY(EditDefaultsOnly, Category = "Ability", meta = (Categories = "Input"))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FDBAbilitySetEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float EffectLevel = 1.f;
};

/** Handles of everything granted by an ability set, so it can be removed again (e.g. weapon swap). */
USTRUCT(BlueprintType)
struct FDBAbilitySetHandles
{
	GENERATED_BODY()

	void TakeFromAbilitySystem(UAbilitySystemComponent* ASC);

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> Abilities;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> Effects;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Server only. */
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, FDBAbilitySetHandles* OutHandles, UObject* SourceObject = nullptr) const;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities", meta = (TitleProperty = Ability))
	TArray<FDBAbilitySetAbility> GrantedAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Effects", meta = (TitleProperty = Effect))
	TArray<FDBAbilitySetEffect> GrantedEffects;
};
