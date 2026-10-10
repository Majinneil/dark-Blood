#include "Character/DBCharacterBase.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Character/DBCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DBGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Visual/DBCharacterVisualComponent.h"
#include "Visual/DBCombatFeedback.h"

ADBCharacterBase::ADBCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDBCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderBody->SetGenerateOverlapEvents(false);
	PlaceholderBody->SetCanEverAffectNavigation(false);

	Visuals = CreateDefaultSubobject<UDBCharacterVisualComponent>(TEXT("Visuals"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		PlaceholderBody->SetStaticMesh(Cylinder.Object);
		// Engine cylinder is 100x100x100 and centered: scale it to the default capsule.
		PlaceholderBody->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.8f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		PlaceholderBody->SetMaterial(0, ShapeMaterial.Object);
	}
}

void ADBCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBCharacterBase, bIsDead);
}

void ADBCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	const bool bHasRealMesh = GetMesh() && GetMesh()->GetSkeletalMeshAsset() != nullptr;
	PlaceholderBody->SetHiddenInGame(bHasRealMesh);
	if (!bHasRealMesh)
	{
		if (UMaterialInstanceDynamic* Material = PlaceholderBody->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), PlaceholderColor);
		}
	}
}

void ADBCharacterBase::SetPlaceholderVisible(bool bVisible)
{
	const bool bHasRealBody = (Visuals && Visuals->HasVisualBody()) || (GetMesh() && GetMesh()->GetSkeletalMeshAsset() != nullptr);
	PlaceholderBody->SetHiddenInGame(false);
	PlaceholderBody->SetVisibility(bVisible && !bHasRealBody);
}

UAbilitySystemComponent* ADBCharacterBase::GetAbilitySystemComponent() const
{
	return CachedAbilitySystem.Get();
}

void ADBCharacterBase::HandleGameplayCue(AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
	if (EventType == EGameplayCueEvent::Executed)
	{
		DBCombatFeedback::HandleCue(this, GameplayCueTag, Parameters);
	}
}

UDBAbilitySystemComponent* ADBCharacterBase::GetDBAbilitySystemComponent() const
{
	return Cast<UDBAbilitySystemComponent>(GetAbilitySystemComponent());
}

void ADBCharacterBase::BindToAttributeSet(UAbilitySystemComponent* ASC)
{
	if (!ASC || !HasAuthority())
	{
		return;
	}
	if (const UDBAttributeSet* Attributes = ASC->GetSet<UDBAttributeSet>())
	{
		Attributes->OnOutOfHealth.Remove(OutOfHealthHandle);
		OutOfHealthHandle = Attributes->OnOutOfHealth.AddUObject(this, &ADBCharacterBase::HandleOutOfHealth);
	}
}

void ADBCharacterBase::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageMagnitude)
{
	if (bIsDead || !HasAuthority())
	{
		return;
	}
	bIsDead = true;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->CancelAllAbilities();
		// Each machine sets its own local count (server here, clients in OnRep_IsDead); cleared on respawn.
		ASC->SetLooseGameplayTagCount(DBTags::State_Dead, 1);
		// Weapon damage over time ends with the victim (GAS defers the removal while an effect executes).
		ASC->RemoveActiveEffectsWithGrantedTags(
			FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{DBTags::State_Burning, DBTags::State_Poisoned, DBTags::State_Bleeding, DBTags::State_Afflicted}));
	}
	PlayDeathPresentation();
	OnDied.Broadcast(this);
}

void ADBCharacterBase::OnRep_IsDead()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(DBTags::State_Dead, bIsDead ? 1 : 0);
	}
	if (bIsDead)
	{
		PlayDeathPresentation();
	}
	else
	{
		PlayRevivePresentation();
	}
}

void ADBCharacterBase::PlayDeathPresentation()
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visuals->PlayDeathPresentation();
}

void ADBCharacterBase::Revive()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !bIsDead || !ASC)
	{
		return;
	}
	bIsDead = false;
	ASC->SetLooseGameplayTagCount(DBTags::State_Dead, 0);
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetHealthAttribute(), ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()));
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetStaminaAttribute(), ASC->GetNumericAttribute(UDBAttributeSet::GetMaxStaminaAttribute()));
	ASC->SetNumericAttributeBase(UDBAttributeSet::GetPoiseAttribute(), ASC->GetNumericAttribute(UDBAttributeSet::GetMaxPoiseAttribute()));
	PlayRevivePresentation();
}

void ADBCharacterBase::PlayRevivePresentation()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Visuals->PlayRevivePresentation();
}

FString ADBCharacterBase::GetCombatDisplayName() const
{
	return GetName();
}

bool ADBCharacterBase::IsMovementInputBlocked() const
{
	static const FGameplayTagContainer BlockingTags = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
		DBTags::State_Attacking, DBTags::State_Dodging, DBTags::State_Staggered, DBTags::State_KnockedDown, DBTags::State_Dead});
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return bIsDead || (ASC && ASC->HasAnyMatchingGameplayTags(BlockingTags));
}
