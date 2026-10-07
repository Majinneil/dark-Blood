#include "Visual/DBCharacterVisualComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AnimationRuntime.h"
#include "Art/DBArtBatcher.h"
#include "Art/DBArtMaterials.h"
#include "Components/StaticMeshComponent.h"
#include "Data/DBItemDefinition.h"
#include "Engine/StaticMesh.h"
#include "Character/DBCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/DBGameplayTags.h"
#include "Data/DBGameDataSubsystem.h"
#include "DarkBlood.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Visual/DBAnimationSetDefinition.h"
#include "Visual/DBCharacterVisualDefinition.h"

namespace
{
	bool& VisualsEnabledFlag()
	{
		static bool bEnabled = !FParse::Param(FCommandLine::Get(), TEXT("DBGreybox"));
		return bEnabled;
	}

	const FName HandBone(TEXT("hand_r"));

	/**
	 * Weapon transform relative to hand_r for a hammer grip, derived from the reference pose of the hand:
	 * blade out of the thumb side (pinky -> index knuckles), edge along the fingers, handle in the closed palm.
	 * Works for any skeleton with UE mannequin finger names; others get a plain offset.
	 */
	FTransform ComputeGripTransform(const USkeletalMeshComponent& Mesh)
	{
		const USkinnedAsset* Asset = Mesh.GetSkinnedAsset();
		if (!Asset)
		{
			return FTransform::Identity;
		}
		const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
		const int32 Hand = Skeleton.FindBoneIndex(HandBone);
		const int32 Index = Skeleton.FindBoneIndex(TEXT("index_01_r"));
		const int32 Middle = Skeleton.FindBoneIndex(TEXT("middle_01_r"));
		const int32 Pinky = Skeleton.FindBoneIndex(TEXT("pinky_01_r"));
		const int32 Thumb = Skeleton.FindBoneIndex(TEXT("thumb_03_r"));
		if (Hand == INDEX_NONE || Index == INDEX_NONE || Middle == INDEX_NONE || Pinky == INDEX_NONE || Thumb == INDEX_NONE)
		{
			return FTransform(FVector(10.f, 0.f, 0.f));
		}
		const FTransform HandCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Hand);
		const FVector IndexPos = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Index).GetLocation();
		const FVector MiddlePos = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Middle).GetLocation();
		const FVector PinkyPos = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Pinky).GetLocation();
		const FVector ThumbPos = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Thumb).GetLocation();

		const FVector Blade = (IndexPos - PinkyPos).GetSafeNormal();
		const FVector Fingers = FVector::VectorPlaneProject(MiddlePos - HandCS.GetLocation(), Blade).GetSafeNormal();
		FVector Palm = FVector::CrossProduct(Blade, Fingers).GetSafeNormal();
		if (FVector::DotProduct(ThumbPos - MiddlePos, Palm) < 0.f)
		{
			Palm = -Palm; // the thumb tip rests on the palm side
		}
		// Handle center: just past the knuckles, inside the closed fist; the hand sits near the guard.
		const FVector KnuckleCenter = (IndexPos + PinkyPos) * 0.5f;
		const FVector Grip = KnuckleCenter + Fingers * 1.5f + Palm * 3.5f - Blade * 4.f;
		const FTransform WeaponCS(FRotationMatrix::MakeFromXZ(Fingers, Blade).ToQuat(), Grip);
		return WeaponCS.GetRelativeTransform(HandCS);
	}
}

UDBCharacterVisualComponent::UDBCharacterVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UDBCharacterVisualComponent::AreVisualsEnabled()
{
	return VisualsEnabledFlag();
}

void UDBCharacterVisualComponent::SetVisualsEnabled(UWorld* World, bool bEnabled)
{
	VisualsEnabledFlag() = bEnabled;
	if (!World)
	{
		return;
	}
	for (TActorIterator<ADBCharacterBase> It(World); It; ++It)
	{
		if (UDBCharacterVisualComponent* Visuals = It->GetVisuals())
		{
			Visuals->RefreshVisuals();
		}
	}
}

void UDBCharacterVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisuals();
}

ADBCharacterBase* UDBCharacterVisualComponent::GetCharacter() const
{
	return Cast<ADBCharacterBase>(GetOwner());
}

void UDBCharacterVisualComponent::SetProfileId(FName NewProfileId)
{
	if (ProfileId != NewProfileId)
	{
		ProfileId = NewProfileId;
		if (HasBegunPlay())
		{
			RefreshVisuals();
		}
	}
}

void UDBCharacterVisualComponent::SetAppearance(const FDBAppearance& InAppearance, FName ClassId, FName NewProfileId)
{
	if (!NewProfileId.IsNone())
	{
		ProfileId = NewProfileId;
	}
	Appearance = InAppearance;
	AppearanceClassId = ClassId;
	bHasAppearance = true;
	if (HasBegunPlay())
	{
		RefreshVisuals();
	}
}

const UDBCharacterVisualDefinition* UDBCharacterVisualComponent::ResolveProfile() const
{
	if (ProfileOverride)
	{
		return ProfileOverride;
	}
	const UDBGameDataSubsystem* Data = ProfileId.IsNone() ? nullptr : UDBGameDataSubsystem::Get(this);
	return Data ? Data->FindCharacterVisual(ProfileId) : nullptr;
}

void UDBCharacterVisualComponent::RefreshVisuals()
{
	ClearVisuals();
	// A dedicated server renders nothing: it keeps the light greybox (collision/movement never depend on the mesh).
	if (!AreVisualsEnabled() || IsRunningDedicatedServer())
	{
		return;
	}
	if (const UDBCharacterVisualDefinition* Profile = ResolveProfile())
	{
		ApplyProfile(*Profile);
	}
}

void UDBCharacterVisualComponent::ApplyProfile(const UDBCharacterVisualDefinition& Profile)
{
	ADBCharacterBase* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	USkeletalMesh* Body = Profile.BodyMesh.LoadSynchronous();
	if (!Mesh || !Body)
	{
		// Asset not imported yet (e.g. Tools/UE58/Setup-DevMannequin.ps1 not run): stay on the greybox body.
		UE_LOG(LogDarkBlood, Verbose, TEXT("Visual profile %s: body mesh %s not available, keeping greybox"), *Profile.ProfileId.ToString(),
			*Profile.BodyMesh.ToString());
		return;
	}

	TSubclassOf<UAnimInstance> AnimClass = Profile.AnimClassOverride.LoadSynchronous();
	if (!AnimClass && Profile.AnimationSet)
	{
		AnimClass = Profile.AnimationSet->AnimClass.LoadSynchronous();
	}

	// Presentation only: gameplay traces use the capsule, so the mesh never collides.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetRelativeTransform(Profile.MeshTransform);
	// A previous profile's material overrides (the placeholder demon's veins) must not stick to the new body.
	Mesh->EmptyOverrideMaterials();
	Mesh->SetSkeletalMesh(Body);
	for (int32 Index = 0; Index < Profile.BodyMaterials.Num(); ++Index)
	{
		if (UMaterialInterface* Material = Profile.BodyMaterials[Index].LoadSynchronous())
		{
			Mesh->SetMaterial(Index, Material);
		}
	}
	if (AnimClass)
	{
		Mesh->SetAnimInstanceClass(AnimClass);
	}
	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		// Movement is code-driven and server-authoritative; animations never move the capsule.
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}
	Mesh->SetHiddenInGame(false);
	Mesh->SetVisibility(true, true);
	ApplyQualityTier(Profile, *Mesh);

	for (const FDBVisualPart& Part : Profile.DefaultParts)
	{
		AddPart(Part, *Mesh);
	}
	if (bHasAppearance)
	{
		if (Profile.HairOptions.IsValidIndex(Appearance.HairStyle))
		{
			AddPart(Profile.HairOptions[Appearance.HairStyle], *Mesh);
		}
		if (Profile.FaceOptions.IsValidIndex(Appearance.FacePreset))
		{
			AddPart(Profile.FaceOptions[Appearance.FacePreset], *Mesh);
		}
	}

	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(Index))
		{
			TintableMaterials.Add(Material);
		}
	}
	ApplyVisualActor(Profile, *Mesh);
	ActiveProfile = &Profile;
	bHasVisualBody = true;
	Character->SetPlaceholderVisible(false);
	ApplyAppearanceParameters();
	ApplyWeapon();
	ApplyDemonAccent();
	if (Character->IsDead())
	{
		PlayDeathPresentation();
	}
}

void UDBCharacterVisualComponent::ApplyQualityTier(const UDBCharacterVisualDefinition& Profile, USkeletalMeshComponent& Mesh) const
{
	switch (Profile.QualityTier)
	{
	case EDBVisualQualityTier::Player:
	case EDBVisualQualityTier::Hero:
		Mesh.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
		Mesh.bEnableUpdateRateOptimizations = false;
		break;
	case EDBVisualQualityTier::ImportantNpc:
		Mesh.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		Mesh.bEnableUpdateRateOptimizations = true;
		break;
	case EDBVisualQualityTier::Crowd:
		Mesh.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		Mesh.bEnableUpdateRateOptimizations = true;
		Mesh.SetCastContactShadow(false);
		break;
	}
}

USkeletalMeshComponent* UDBCharacterVisualComponent::AddPart(const FDBVisualPart& Part, USkeletalMeshComponent& Leader)
{
	USkeletalMesh* PartMesh = Part.Mesh.LoadSynchronous();
	if (!PartMesh)
	{
		return nullptr;
	}
	USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(GetOwner(), NAME_None, RF_Transient);
	Component->SetupAttachment(&Leader);
	Component->SetSkeletalMesh(PartMesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetLeaderPoseComponent(&Leader);
	if (UMaterialInterface* Material = Part.Material.LoadSynchronous())
	{
		for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
		{
			Component->SetMaterial(Index, Material);
		}
	}
	Component->RegisterComponent();
	for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(Index))
		{
			TintableMaterials.Add(Material);
		}
	}
	PartComponents.Add(Component);
	return Component;
}

void UDBCharacterVisualComponent::ApplyAppearanceParameters()
{
	if (!ActiveProfile)
	{
		return;
	}
	const UDBCharacterVisualDefinition& Profile = *ActiveProfile;
	FLinearColor Outfit = Profile.OutfitTint;
	if (const FLinearColor* ClassTint = Profile.ClassOutfitTints.Find(AppearanceClassId))
	{
		Outfit = *ClassTint;
	}
	for (UMaterialInstanceDynamic* Material : TintableMaterials)
	{
		if (!Profile.OutfitTintParameter.IsNone())
		{
			Material->SetVectorParameterValue(Profile.OutfitTintParameter, Outfit);
		}
		if (!bHasAppearance)
		{
			continue;
		}
		if (!Profile.SkinToneParameter.IsNone() && Profile.SkinTones.IsValidIndex(Appearance.SkinTone))
		{
			Material->SetVectorParameterValue(Profile.SkinToneParameter, Profile.SkinTones[Appearance.SkinTone]);
		}
		if (!Profile.HairColorParameter.IsNone())
		{
			Material->SetVectorParameterValue(Profile.HairColorParameter, FLinearColor(Appearance.HairColor));
		}
		if (!Profile.EyeColorParameter.IsNone())
		{
			Material->SetVectorParameterValue(Profile.EyeColorParameter, FLinearColor(Appearance.EyeColor));
		}
	}
}

void UDBCharacterVisualComponent::SetWeaponItem(FName ItemId)
{
	if (WeaponItemId != ItemId)
	{
		WeaponItemId = ItemId;
		ApplyWeapon();
	}
}

void UDBCharacterVisualComponent::ApplyWeapon()
{
	if (WeaponComponent)
	{
		WeaponComponent->DestroyComponent();
		WeaponComponent = nullptr;
	}
	ADBCharacterBase* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const UDBGameDataSubsystem* Data = WeaponItemId.IsNone() ? nullptr : UDBGameDataSubsystem::Get(this);
	const UDBItemDefinition* Item = Data ? Data->FindItem(WeaponItemId) : nullptr;
	if (!bHasVisualBody || !Mesh || !Item || Item->Category != EDBItemCategory::Weapon || Mesh->GetBoneIndex(HandBone) == INDEX_NONE)
	{
		return;
	}
	// Authored weapon model, or a plain steel blade until the model is imported.
	UStaticMesh* WeaponMesh = Item->WorldMesh.LoadSynchronous();
	UMaterialInterface* Material = Item->WorldMaterial.LoadSynchronous();
	FTransform ModelTransform = FTransform::Identity;
	if (!WeaponMesh)
	{
		WeaponMesh = FDBArtBatcher::GetShapeMesh(FDBArtBatcher::EShape::Cube);
		Material = UDBArtMaterialSubsystem::Get(EDBArtMaterial::MetalIron);
		ModelTransform = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 38.f), FVector(0.035f, 0.008f, 0.95f));
	}
	if (!WeaponMesh)
	{
		return;
	}
	WeaponComponent = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
	WeaponComponent->SetStaticMesh(WeaponMesh);
	if (Material)
	{
		WeaponComponent->SetMaterial(0, Material);
	}
	WeaponComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponComponent->SetGenerateOverlapEvents(false);
	WeaponComponent->SetCanEverAffectNavigation(false);
	WeaponComponent->SetupAttachment(Mesh, HandBone);
	WeaponComponent->SetRelativeTransform(ModelTransform * ComputeGripTransform(*Mesh));
	WeaponComponent->RegisterComponent();
	WeaponComponent->SetVisibility(Mesh->IsVisible());
	UE_LOG(LogDarkBlood, Log, TEXT("%s: weapon %s (%s)"), *GetNameSafe(GetOwner()), *WeaponItemId.ToString(), *GetNameSafe(WeaponMesh));
}

void UDBCharacterVisualComponent::ClearVisuals()
{
	if (WeaponComponent)
	{
		WeaponComponent->DestroyComponent();
		WeaponComponent = nullptr;
	}
	for (USkeletalMeshComponent* Part : PartComponents)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	PartComponents.Reset();
	TintableMaterials.Reset();
	if (VisualActor)
	{
		VisualActor->Destroy();
		VisualActor = nullptr;
		if (ADBCharacterBase* Character = GetCharacter())
		{
			USkeletalMeshComponent* Mesh = Character->GetMesh();
			Mesh->SetRenderInMainPass(true);
			Mesh->SetRenderInDepthPass(true);
			Mesh->SetCastShadow(true);
		}
	}
	ActiveProfile = nullptr;
	if (bHasVisualBody)
	{
		if (ADBCharacterBase* Character = GetCharacter())
		{
			Character->GetMesh()->SetAnimInstanceClass(nullptr);
			Character->GetMesh()->SetOverlayMaterial(nullptr);
			Character->GetMesh()->SetSkeletalMesh(nullptr);
			Character->SetPlaceholderVisible(!Character->IsDead());
		}
	}
	bHasVisualBody = false;
}

UAnimMontage* UDBCharacterVisualComponent::FindMontage(const FGameplayTag& Key, int32 Variant, bool* bOutFitToDuration) const
{
	const UDBAnimationSetDefinition* Set = ActiveProfile ? ActiveProfile->AnimationSet.Get() : nullptr;
	return Set && bHasVisualBody ? Set->FindMontage(Key, Variant, bOutFitToDuration) : nullptr;
}

void UDBCharacterVisualComponent::PlayDeathPresentation()
{
	ADBCharacterBase* Character = GetCharacter();
	if (!bHasVisualBody || !Character)
	{
		return;
	}
	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	// Death montages do not blend out: the body holds the last pose until the character is revived/removed.
	UAnimMontage* Death = AnimInstance ? FindMontage(DBTags::Anim_Death, FMath::RandRange(0, 1)) : nullptr;
	if (Death)
	{
		AnimInstance->Montage_Play(Death);
	}
	else
	{
		Character->GetMesh()->SetVisibility(false, true);
	}
}

void UDBCharacterVisualComponent::PlayRevivePresentation()
{
	ADBCharacterBase* Character = GetCharacter();
	if (!bHasVisualBody || !Character)
	{
		return;
	}
	if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.1f);
	}
	Character->GetMesh()->SetVisibility(true, true);
}

FName UDBCharacterVisualComponent::GetEmotion() const
{
	if (!CurrentEmotion.IsNone())
	{
		return CurrentEmotion;
	}
	return ActiveProfile ? ActiveProfile->DefaultEmotion : FName(TEXT("Neutral"));
}

void UDBCharacterVisualComponent::SetDemonAccent(const FLinearColor& Color, float Strength, float DrawDistance)
{
	AccentColor = Color;
	AccentStrength = FMath::Max(0.f, Strength);
	AccentDrawDistance = DrawDistance;
	ApplyDemonAccent();
}

void UDBCharacterVisualComponent::ApplyDemonAccent()
{
	ADBCharacterBase* Character = GetCharacter();
	if (!Character || !bHasVisualBody)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (AccentStrength <= 0.f)
	{
		Mesh->SetOverlayMaterial(nullptr);
		return;
	}
	// Glow parameters the Paragon masters share (absent ones are simply ignored by the material).
	struct FDBAccentParameter
	{
		const TCHAR* Name;
		float Scale;
	};
	static const FDBAccentParameter AccentParameters[] = {
		{TEXT("EyeGlowColor"), 4.f}, {TEXT("TeamColor"), 3.f}, {TEXT("EmissiveColor"), 3.f},
		{TEXT("BodyGlowColorLow"), 1.f}, {TEXT("BodyGlowColorHigh"), 3.f}, {TEXT("HairEmissiveColor"), 2.f},
		{TEXT("FlameTint"), 1.f}, {TEXT("EmissiveColor_SwordTip"), 4.f}, {TEXT("EmissiveColor_SwordBase"), 4.f}};
	for (UMaterialInstanceDynamic* Material : TintableMaterials)
	{
		for (const FDBAccentParameter& Parameter : AccentParameters)
		{
			FLinearColor Current;
			if (Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter.Name), Current))
			{
				Material->SetVectorParameterValue(Parameter.Name, AccentColor * (Parameter.Scale * AccentStrength));
			}
		}
	}

	static TSoftObjectPtr<UMaterialInterface> OverlayAsset(FSoftObjectPath(TEXT("/Game/DarkBlood/Art/Materials/Master/M_DB_DemonOverlay.M_DB_DemonOverlay")));
	UMaterialInterface* OverlayMaterial = OverlayAsset.LoadSynchronous();
	if (!OverlayMaterial)
	{
		return;
	}
	if (!AccentOverlay || AccentOverlay->Parent != OverlayMaterial)
	{
		AccentOverlay = UMaterialInstanceDynamic::Create(OverlayMaterial, this);
	}
	AccentOverlay->SetVectorParameterValue(TEXT("AccentColor"), AccentColor);
	// A dark cast in the accent colour and a thin glowing edge - the body keeps its own shading underneath.
	AccentOverlay->SetScalarParameterValue(TEXT("RimIntensity"), 2.5f * AccentStrength);
	AccentOverlay->SetScalarParameterValue(TEXT("BodyGlow"), 0.06f);
	AccentOverlay->SetScalarParameterValue(TEXT("BodyOpacity"), FMath::Min(0.3f * AccentStrength, 0.45f));
	Mesh->SetOverlayMaterial(AccentOverlay);
	Mesh->SetOverlayMaterialMaxDrawDistance(AccentDrawDistance);
}

void UDBCharacterVisualComponent::ApplyVisualActor(const UDBCharacterVisualDefinition& Profile, USkeletalMeshComponent& Leader)
{
	UClass* ActorClass = Profile.VisualActorClass.LoadSynchronous();
	UWorld* World = GetWorld();
	if (!ActorClass || !World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AActor* Actor = World->SpawnActor<AActor>(ActorClass, Leader.GetComponentTransform(), Params);
	if (!Actor)
	{
		return;
	}
	// Local presentation like the rest of this component: never replicated, never colliding.
	Actor->SetReplicates(false);
	Actor->SetActorEnableCollision(false);
	Actor->AttachToComponent(&Leader, FAttachmentTransformRules::SnapToTargetIncludingScale);
	// Skeletal meshes carrying the mannequin body bones follow the leader; face and grooms keep their own setup
	// (a MetaHuman face copies the body pose itself).
	TArray<USkeletalMeshComponent*> Meshes;
	Actor->GetComponents(Meshes);
	int32 Followers = 0;
	for (USkeletalMeshComponent* Part : Meshes)
	{
		if (Part->GetSkeletalMeshAsset() && Part->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE && Part->GetBoneIndex(TEXT("thigh_l")) != INDEX_NONE)
		{
			Part->SetRelativeTransform(FTransform::Identity);
			Part->SetLeaderPoseComponent(&Leader);
			++Followers;
		}
	}
	// The leader keeps animating (and receives montages) but is not drawn.
	Leader.SetRenderInMainPass(false);
	Leader.SetRenderInDepthPass(false);
	Leader.SetCastShadow(false);
	Leader.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	VisualActor = Actor;
	UE_LOG(LogDarkBlood, Log, TEXT("Visual profile %s: authored actor %s, %d body meshes follow the animation"), *Profile.ProfileId.ToString(),
		*ActorClass->GetName(), Followers);
}
