#include "Visual/DBCharacterVisualComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/DBCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/DBGameplayTags.h"
#include "Data/DBGameDataSubsystem.h"
#include "DarkBlood.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
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
	ActiveProfile = &Profile;
	bHasVisualBody = true;
	Character->SetPlaceholderVisible(false);
	ApplyAppearanceParameters();
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

void UDBCharacterVisualComponent::ClearVisuals()
{
	for (USkeletalMeshComponent* Part : PartComponents)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	PartComponents.Reset();
	TintableMaterials.Reset();
	ActiveProfile = nullptr;
	if (bHasVisualBody)
	{
		if (ADBCharacterBase* Character = GetCharacter())
		{
			Character->GetMesh()->SetAnimInstanceClass(nullptr);
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
