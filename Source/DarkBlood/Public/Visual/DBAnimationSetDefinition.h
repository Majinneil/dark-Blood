// Animation set (DBAnimationSet): montages keyed by Anim.* gameplay tags.
// Abilities ask for a key ("Anim.Attack.Light") instead of holding montages, so a character can switch from the
// DEV mannequin set to a Game Animation Sample / motion-capture set without touching gameplay code.
// Montages are presentation only: damage windows, i-frames and movement stay server-side in the abilities.
#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "DBAnimationSetDefinition.generated.h"

class UAnimInstance;
class UAnimMontage;

USTRUCT(BlueprintType)
struct FDBAnimationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (Categories = "Anim"))
	FGameplayTag Key;

	/** Variants; combo steps pick by index, everything else picks the first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TArray<TSoftObjectPtr<UAnimMontage>> Montages;

	/** Scales the play rate so the montage fits the gameplay duration of the action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	bool bFitToActionDuration = true;
};

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBAnimationSetDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/**
	 * Montage for Key (variant Index). Falls back to the parent tag (Anim.Attack.Sprint -> Anim.Attack), so a
	 * small set still covers every action. Loads synchronously on first use.
	 */
	UAnimMontage* FindMontage(const FGameplayTag& Key, int32 Index, bool* bOutFitToDuration = nullptr) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FName AnimationSetId;

	/** Locomotion Animation Blueprint used with this set (motion matching / blend spaces). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftClassPtr<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation", meta = (TitleProperty = "Key"))
	TArray<FDBAnimationEntry> Entries;
};
