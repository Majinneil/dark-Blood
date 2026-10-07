// Character visual profile (DBCharacterVisual) - follows Schemas/character_visual_profile.schema.json of the
// visual foundation pack. Holds only presentation: body mesh, parts, materials, animation set and the
// mapping from the replicated FDBAppearance ids to material parameters / parts.
// MetaHuman integration: point BodyMesh / Parts at the assembled MetaHuman meshes (Joints Only rig for Crowd)
// and set the parameter names of its materials. No runtime dependency on MetaHuman editor tools.
#pragma once

#include "Engine/DataAsset.h"
#include "Visual/DBVisualTypes.h"

#include "DBCharacterVisualDefinition.generated.h"

class AActor;
class UAnimInstance;
class UDBAnimationSetDefinition;
class UMaterialInterface;
class USkeletalMesh;

UCLASS(BlueprintType, Const)
class DARKBLOOD_API UDBCharacterVisualDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	FName ProfileId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	EDBVisualQualityTier QualityTier = EDBVisualQualityTier::ImportantNpc;

	/** Preset ids of the art pipeline (MetaHuman body/face/skin presets). Informational until presets exist. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	FName BodyPreset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	FName FacePreset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	FName SkinPreset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	FName OutfitSetId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	FName ArmorSetId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	FName MaterialVariantId;

	/** True for DEV profiles (template mannequin). The visual audit lists them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Profile")
	bool bDevelopmentPlaceholder = false;

	// ---- Body --------------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	/** Overrides the animation set's AnimClass (e.g. a character-specific post-process or face graph). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftClassPtr<UAnimInstance> AnimClassOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TObjectPtr<UDBAnimationSetDefinition> AnimationSet = nullptr;

	/** Mesh placement inside the capsule (feet at the capsule bottom, facing +X). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	FTransform MeshTransform = FTransform(FRotator(0.f, -90.f, 0.f), FVector(0.f, 0.f, -88.f));

	/** Per-section material overrides of the body (empty entries keep the mesh material). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TArray<TSoftObjectPtr<UMaterialInterface>> BodyMaterials;

	/** Parts always attached (outfit layers, armor, accessories). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body", meta = (TitleProperty = "PartId"))
	TArray<FDBVisualPart> DefaultParts;

	/** Authored character shown instead of BodyMesh (a MetaHuman build, BP_<Name>): spawned and attached to the character
	 * mesh; its body follows the hidden BodyMesh by bone name, so every animation of the profile drives it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftClassPtr<AActor> VisualActorClass;

	// ---- Appearance mapping (FDBAppearance -> visuals) ------------------------------------------------

	/** FDBAppearance::HairStyle indexes this list. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance", meta = (TitleProperty = "PartId"))
	TArray<FDBVisualPart> HairOptions;

	/** FDBAppearance::FacePreset indexes this list (beards / face parts). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance", meta = (TitleProperty = "PartId"))
	TArray<FDBVisualPart> FaceOptions;

	/** FDBAppearance::SkinTone indexes this palette. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	TArray<FLinearColor> SkinTones;

	/** Material parameter names on the body / parts. None = not supported by this mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FName SkinToneParameter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FName HairColorParameter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FName EyeColorParameter;

	/** Outfit tint parameter and color; ClassOutfitTints overrides the color per class id (players). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FName OutfitTintParameter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FLinearColor OutfitTint = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	TMap<FName, FLinearColor> ClassOutfitTints;

	// ---- Face ------------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Face")
	bool bProceduralBlink = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Face")
	bool bLookAtTargets = true;

	/** Emotion used when nothing else is requested (Neutral, Stern, Warm, Afraid, Angry, Pain ...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Face")
	FName DefaultEmotion = TEXT("Neutral");
};
