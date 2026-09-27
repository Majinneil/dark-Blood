#include "Character/DBCharacterBase.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DBGameplayTags.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADBCharacterBase::ADBCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderBody->SetGenerateOverlapEvents(false);
	PlaceholderBody->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		PlaceholderBody->SetStaticMesh(Cylinder.Object);
		// Engine cylinder is 100x100x100 and centered: scale it to the default capsule.
		PlaceholderBody->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.8f));
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
}

UAbilitySystemComponent* ADBCharacterBase::GetAbilitySystemComponent() const
{
	return CachedAbilitySystem.Get();
}

UDBAbilitySystemComponent* ADBCharacterBase::GetDBAbilitySystemComponent() const
{
	return CachedAbilitySystem.Get();
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
	}
	PlayDeathPresentation();
	OnDied.Broadcast(this);
}

void ADBCharacterBase::OnRep_IsDead()
{
	if (bIsDead)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ASC->SetLooseGameplayTagCount(DBTags::State_Dead, 1);
		}
		PlayDeathPresentation();
	}
}

void ADBCharacterBase::PlayDeathPresentation()
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Phase 2: death montage / ragdoll, VFX and audio are played here.
}
