// Applies a character visual profile to its owner: body mesh + animation class, modular parts (leader pose),
// material tints from the replicated appearance ids, quality-tier budgets and death/revive presentation.
// Local presentation only - runs identically on server and clients from replicated data; never replicates
// itself and never changes collision or movement. Without a (loadable) profile the greybox body stays.
#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Core/DBTypes.h"

#include "DBCharacterVisualComponent.generated.h"

class ADBCharacterBase;
class UAnimMontage;
class UDBAnimationSetDefinition;
class UDBCharacterVisualDefinition;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup = "DarkBlood", meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBCharacterVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBCharacterVisualComponent();

	/** -DBGreybox (or DBVisuals 0) keeps every character on its greybox body - the reversible fallback. */
	static bool AreVisualsEnabled();
	static void SetVisualsEnabled(UWorld* World, bool bEnabled);

	/** Switches to another profile id (resolved through UDBGameDataSubsystem) and re-applies. */
	void SetProfileId(FName NewProfileId);
	FName GetProfileId() const { return ProfileId; }

	/** Appearance + class of a player character (from the replicated profile); optionally switches the profile. */
	void SetAppearance(const FDBAppearance& Appearance, FName ClassId, FName NewProfileId = NAME_None);

	/** Main-hand weapon shown in the right hand (item id from the replicated equipment; None = empty hand). */
	void SetWeaponItem(FName ItemId);
	FName GetWeaponItem() const { return WeaponItemId; }
	UStaticMeshComponent* GetWeaponComponent() const { return WeaponComponent; }

	/** Re-applies the current profile (after SetVisualsEnabled or a profile change). */
	void RefreshVisuals();

	bool HasVisualBody() const { return bHasVisualBody; }
	const UDBCharacterVisualDefinition* GetActiveProfile() const { return ActiveProfile; }

	/** Montage of the active animation set for an Anim.* key (parent-tag fallback), or null. */
	UAnimMontage* FindMontage(const FGameplayTag& Key, int32 Variant = 0, bool* bOutFitToDuration = nullptr) const;

	void PlayDeathPresentation();
	void PlayRevivePresentation();

	/** Face / head: where to look (dialogue partner, lock-on target) and the current emotion. */
	void SetLookAtTarget(AActor* Target) { LookAtTarget = Target; }
	AActor* GetLookAtTarget() const { return LookAtTarget.Get(); }
	void SetEmotion(FName Emotion) { CurrentEmotion = Emotion; }
	FName GetEmotion() const;

protected:
	virtual void BeginPlay() override;

	/** Profile applied at BeginPlay (e.g. CV_NPC_King). Players pick theirs from the body type. */
	UPROPERTY(EditAnywhere, Category = "Dark Blood|Visuals")
	FName ProfileId;

	/** Direct asset reference; wins over ProfileId (Blueprint subclasses, level instances). */
	UPROPERTY(EditAnywhere, Category = "Dark Blood|Visuals")
	TObjectPtr<UDBCharacterVisualDefinition> ProfileOverride = nullptr;

private:
	ADBCharacterBase* GetCharacter() const;
	const UDBCharacterVisualDefinition* ResolveProfile() const;
	void ApplyProfile(const UDBCharacterVisualDefinition& Profile);
	void ApplyQualityTier(const UDBCharacterVisualDefinition& Profile, USkeletalMeshComponent& Mesh) const;
	void ApplyAppearanceParameters();
	USkeletalMeshComponent* AddPart(const struct FDBVisualPart& Part, USkeletalMeshComponent& Leader);
	void ClearVisuals();
	void ApplyWeapon();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponComponent = nullptr;

	FName WeaponItemId;

	UPROPERTY(Transient)
	TObjectPtr<const UDBCharacterVisualDefinition> ActiveProfile = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> PartComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TintableMaterials;

	TWeakObjectPtr<AActor> LookAtTarget;
	FDBAppearance Appearance;
	FName AppearanceClassId;
	FName CurrentEmotion;
	bool bHasAppearance = false;
	bool bHasVisualBody = false;
};
