// Maps Enhanced Input actions to gameplay tags. Native actions (move/look/jump) are bound directly,
// ability actions are forwarded to the ability system by tag.
#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "DBInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FDBInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputAction* FindNativeInputAction(const FGameplayTag& InputTag) const;

	/**
	 * DEVELOPMENT FALLBACK: builds input actions and a keyboard/mouse + gamepad mapping context in code,
	 * so the project is playable before input assets are authored. Replace with assets for shipping.
	 */
	static UDBInputConfig* CreateDevelopmentDefaults(UObject* Outer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputAction"))
	TArray<FDBInputAction> NativeInputActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputAction"))
	TArray<FDBInputAction> AbilityInputActions;
};
