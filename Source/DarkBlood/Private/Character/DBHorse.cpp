#include "Character/DBHorse.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Character/DBCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "World/DBRealmLayout.h"

#define LOCTEXT_NAMESPACE "DarkBloodHorse"

namespace
{
	constexpr float StaminaDrainPerSecond = 12.f;
	constexpr float StaminaRegenPerSecond = 8.f;

	UAbilitySystemComponent* GetRiderAbilitySystem(const APawn* Pawn)
	{
		const IAbilitySystemInterface* Owner = Pawn ? Cast<IAbilitySystemInterface>(Pawn->GetPlayerState()) : nullptr;
		return Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	}
}

ADBHorse::ADBHorse(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDBCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	bUseControllerRotationYaw = false;
	GetCapsuleComponent()->InitCapsuleSize(55.f, 95.f);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = TrotSpeed;
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 220.f, 0.f);
	Movement->bRunPhysicsWithNoController = true;
	Movement->MaxStepHeight = 60.f;
	Movement->BrakingDecelerationWalking = 1400.f;

	// Placeholder horse from basic shapes (forward = +X): body, neck, head and four legs.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	struct FPart
	{
		const TCHAR* Name;
		bool bCube;
		FVector Location;
		FVector Scale;
		FRotator Rotation;
	};
	const FPart Parts[] = {
		{TEXT("Body"), true, FVector(0.f, 0.f, 25.f), FVector(1.9f, 0.62f, 0.75f), FRotator::ZeroRotator},
		{TEXT("Neck"), true, FVector(95.f, 0.f, 75.f), FVector(0.45f, 0.32f, 0.85f), FRotator(-35.f, 0.f, 0.f)},
		{TEXT("Head"), true, FVector(135.f, 0.f, 115.f), FVector(0.65f, 0.28f, 0.3f), FRotator(-20.f, 0.f, 0.f)},
		{TEXT("Tail"), true, FVector(-100.f, 0.f, 35.f), FVector(0.15f, 0.12f, 0.6f), FRotator(25.f, 0.f, 0.f)},
		{TEXT("LegFL"), false, FVector(65.f, 22.f, -50.f), FVector(0.17f, 0.17f, 0.95f), FRotator::ZeroRotator},
		{TEXT("LegFR"), false, FVector(65.f, -22.f, -50.f), FVector(0.17f, 0.17f, 0.95f), FRotator::ZeroRotator},
		{TEXT("LegBL"), false, FVector(-65.f, 22.f, -50.f), FVector(0.17f, 0.17f, 0.95f), FRotator::ZeroRotator},
		{TEXT("LegBR"), false, FVector(-65.f, -22.f, -50.f), FVector(0.17f, 0.17f, 0.95f), FRotator::ZeroRotator},
	};
	for (const FPart& Part : Parts)
	{
		UStaticMeshComponent* PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(Part.Name);
		PartMesh->SetupAttachment(GetCapsuleComponent());
		PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartMesh->SetCanEverAffectNavigation(false);
		PartMesh->SetRelativeLocation(Part.Location);
		PartMesh->SetRelativeRotation(Part.Rotation);
		PartMesh->SetRelativeScale3D(Part.Scale);
		if (UStaticMesh* Shape = Part.bCube ? Cube.Object : Cylinder.Object)
		{
			PartMesh->SetStaticMesh(Shape);
		}
		if (ShapeMaterial.Succeeded())
		{
			PartMesh->SetMaterial(0, ShapeMaterial.Object);
		}
		BodyParts.Add(PartMesh);
	}
}

void ADBHorse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBHorse, Rider);
	DOREPLIFETIME(ADBHorse, OwnerState);
	DOREPLIFETIME(ADBHorse, HorseStamina);
}

void ADBHorse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (BodyParts.Num() > 0 && BodyParts[0]->GetMaterial(0) && !BodyParts[0]->GetMaterial(0)->IsA<UMaterialInstanceDynamic>())
	{
		for (UStaticMeshComponent* Part : BodyParts)
		{
			if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0))
			{
				Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.06f, 0.025f));
			}
		}
	}
	if (!HasAuthority())
	{
		return;
	}
	if (Rider && (!IsValid(Rider) || Rider->GetAttachParentActor() != this))
	{
		Dismount();
	}
	const bool bMoving = GetVelocity().Size2D() > 300.f;
	if (IsGalloping() && bMoving)
	{
		HorseStamina = FMath::Max(0.f, HorseStamina - StaminaDrainPerSecond * DeltaSeconds);
	}
	else
	{
		HorseStamina = FMath::Min(MaxHorseStamina, HorseStamina + StaminaRegenPerSecond * DeltaSeconds);
	}
	GetCharacterMovement()->MaxWalkSpeed = IsGalloping() ? GallopSpeed : TrotSpeed;
	if (Rider && !Steering.IsNearlyZero())
	{
		const FRotator Yaw(0.f, SteeringYaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * Steering.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Steering.X);
	}
}

FText ADBHorse::GetInteractionText() const
{
	return Rider ? LOCTEXT("Dismount", "Absteigen") : LOCTEXT("Mount", "Aufsteigen");
}

bool ADBHorse::CanInteract(const APawn* User) const
{
	if (!User)
	{
		return false;
	}
	if (Rider)
	{
		return Rider == User;
	}
	return !OwnerState || OwnerState == User->GetPlayerState();
}

void ADBHorse::Interact(APlayerController* User)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	if (!Pawn || !HasAuthority() || !CanInteract(Pawn))
	{
		return;
	}
	if (Rider == Pawn)
	{
		Dismount();
	}
	else
	{
		Mount(Pawn);
	}
}

void ADBHorse::Mount(APawn* Pawn)
{
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character || FindRiddenBy(Pawn))
	{
		return;
	}
	Rider = Pawn;
	Steering = FVector2D::ZeroVector;
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->GetCharacterMovement()->SetMovementMode(MOVE_None);
	Character->GetCapsuleComponent()->IgnoreActorWhenMoving(this, true);
	GetCapsuleComponent()->IgnoreActorWhenMoving(Character, true);
	Character->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Character->SetActorRelativeLocation(FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 30.f));
	Character->SetActorRelativeRotation(FRotator::ZeroRotator);
	if (UAbilitySystemComponent* ASC = GetRiderAbilitySystem(Pawn))
	{
		ASC->AddLooseGameplayTag(DBTags::State_Mounted);
	}
	UE_LOG(LogDarkBlood, Log, TEXT("%s mounts %s"), *GetNameSafe(Pawn), *GetName());
}

void ADBHorse::Dismount()
{
	APawn* Pawn = Rider;
	Rider = nullptr;
	Steering = FVector2D::ZeroVector;
	bGallop = false;
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!IsValid(Character))
	{
		return;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorLocation(GetActorLocation() + GetActorRightVector() * 160.f + FVector(0.f, 0.f, 40.f), false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Character->GetCapsuleComponent()->IgnoreActorWhenMoving(this, false);
	GetCapsuleComponent()->IgnoreActorWhenMoving(Character, false);
	if (UAbilitySystemComponent* ASC = GetRiderAbilitySystem(Character))
	{
		ASC->RemoveLooseGameplayTag(DBTags::State_Mounted);
	}
	UE_LOG(LogDarkBlood, Log, TEXT("%s dismounts %s"), *GetNameSafe(Character), *GetName());
}

void ADBHorse::SetSteering(const FVector2D& Input, float CameraYaw)
{
	Steering = FVector2D(FMath::Clamp(Input.X, -1.f, 1.f), FMath::Clamp(Input.Y, -1.f, 1.f));
	SteeringYaw = CameraYaw;
}

void ADBHorse::SetGallop(bool bInGallop)
{
	bGallop = bInGallop;
}

ADBHorse* ADBHorse::FindRiddenBy(const APawn* Pawn)
{
	ADBHorse* Horse = Pawn ? Cast<ADBHorse>(Pawn->GetAttachParentActor()) : nullptr;
	return Horse && Horse->Rider == Pawn ? Horse : nullptr;
}

ADBHorse* ADBHorse::FindOwnedBy(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return nullptr;
	}
	for (TActorIterator<ADBHorse> It(PlayerState->GetWorld()); It; ++It)
	{
		if (It->OwnerState == PlayerState)
		{
			return *It;
		}
	}
	return nullptr;
}

ADBHorse* ADBHorse::CallHorse(APawn* Player)
{
	APlayerState* PlayerState = Player ? Player->GetPlayerState() : nullptr;
	if (!PlayerState || !Player->HasAuthority() || FindRiddenBy(Player))
	{
		return nullptr;
	}
	UWorld* World = Player->GetWorld();
	// A free spot on the ground near the player (behind first): not on a roof, not inside a house or a tree.
	const FVector PlayerLocation = Player->GetActorLocation();
	const FVector Forward = Player->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DBCallHorse), false, Player);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(60.f, 95.f);
	FVector Location = PlayerLocation + Right * 200.f + FVector(0.f, 0.f, 20.f);
	const FVector Candidates[] = {-Forward * 600.f, -Forward * 900.f, Right * 600.f, -Right * 600.f, Forward * 800.f, -Forward * 1200.f, Right * 1000.f,
		-Right * 1000.f};
	for (const FVector& Offset : Candidates)
	{
		const FVector Spot = PlayerLocation + Offset;
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 600.f), Spot - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params) ||
			FMath::Abs(Hit.ImpactPoint.Z - PlayerLocation.Z) > 400.f)
		{
			continue;
		}
		const FVector Candidate = Hit.ImpactPoint + FVector(0.f, 0.f, 110.f);
		if (!World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Capsule, Params))
		{
			Location = Candidate;
			break;
		}
	}
	const FRotator Facing(0.f, Player->GetActorRotation().Yaw, 0.f);

	ADBHorse* Horse = FindOwnedBy(PlayerState);
	if (!Horse)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Horse = World->SpawnActor<ADBHorse>(ADBHorse::StaticClass(), Location, Facing, SpawnParams);
		if (Horse)
		{
			Horse->OwnerState = PlayerState;
		}
	}
	else if (!Horse->Rider && FVector::Dist(Horse->GetActorLocation(), Player->GetActorLocation()) > 1500.f)
	{
		Horse->TeleportTo(Location, Facing);
		Horse->GetCharacterMovement()->StopMovementImmediately();
	}
	return Horse;
}

#undef LOCTEXT_NAMESPACE
