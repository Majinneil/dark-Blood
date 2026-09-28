#include "World/DBShip.h"

#include "Art/DBArtMaterials.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DarkBlood.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Landscape.h"
#include "Net/UnrealNetwork.h"
#include "World/DBRealmLayout.h"

namespace
{
	/** Deck height above the water line and the helm position at the stern (ship frame, cm). */
	constexpr float DeckHeight = 380.f;
	/** Relative to the actor (the deck box center, 60 cm below the deck surface): a standing character at the stern. */
	const FVector HelmOffset(-1250.f, 0.f, 160.f);
}

ADBShip::ADBShip()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(30.f);
	ShipName = FText::FromString(TEXT("Pinasse"));

	// Walkable deck: a flat box, 33 m long, level with the model's deck.
	Hull = CreateDefaultSubobject<UBoxComponent>(TEXT("Hull"));
	Hull->SetBoxExtent(FVector(1500.f, 380.f, 60.f));
	Hull->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Hull->SetMobility(EComponentMobility::Movable);
	Hull->SetCanEverAffectNavigation(false);
	RootComponent = Hull;

	Model = CreateDefaultSubobject<USceneComponent>(TEXT("Model"));
	Model->SetupAttachment(Hull);
	Model->SetRelativeLocation(FVector(0.f, 0.f, -(DeckHeight - 60.f)));
}

void ADBShip::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// The CC0 pinnace (Poly Haven, modeled along Y and grounded on its keel) or a plain hull.
	static const TCHAR* Parts[] = {TEXT("aft"), TEXT("deck"), TEXT("details"), TEXT("hull"), TEXT("interior"), TEXT("rigging"), TEXT("sails")};
	bool bModel = false;
	for (const TCHAR* Part : Parts)
	{
		const FString Path = FString::Printf(TEXT("/Game/DarkBlood/Art/Environment/PolyHaven/ship_pinnace/ship_pinnace_1k/StaticMeshes/ship_pinnace_%s.ship_pinnace_%s"),
			Part, Part);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh)
		{
			continue;
		}
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Piece->SetStaticMesh(Mesh);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetupAttachment(Model);
		// Keel 2.2 m below the water line; the model's Y axis becomes the ship's forward axis.
		Piece->SetRelativeTransform(FTransform(FRotator(0.f, 90.f, 0.f), FVector(0.f, 0.f, -220.f)));
		Piece->RegisterComponent();
		bModel = true;
	}
	if (!bModel)
	{
		UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Box->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Box->SetMaterial(0, UDBArtMaterialSubsystem::Get(EDBArtMaterial::WoodDark));
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetupAttachment(Model);
		Box->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, DeckHeight * 0.5f - 150.f), FVector(30.f, 7.5f, (DeckHeight + 150.f) / 100.f)));
		Box->RegisterComponent();
	}
}

void ADBShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBShip, Helmsman);
}

ADBShip* ADBShip::FindSteeredBy(const APawn* Pawn)
{
	if (!Pawn || !Pawn->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ADBShip> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->Helmsman == Pawn)
		{
			return *It;
		}
	}
	return nullptr;
}

ADBShip* ADBShip::SpawnAt(UWorld* World, const FVector2D& Location, float Yaw, const FText& Name)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// The actor sits at the deck box center; the water line is DeckHeight - 60 below it.
	ADBShip* Ship = World->SpawnActor<ADBShip>(ADBShip::StaticClass(), FVector(Location.X, Location.Y, DeckHeight - 60.f), FRotator(0.f, Yaw, 0.f), Params);
	if (Ship)
	{
		Ship->ShipName = Name;
	}
	return Ship;
}

FText ADBShip::GetInteractionText() const
{
	return FText::Format(FText::FromString(Helmsman ? TEXT("Steuer loslassen: {0}") : TEXT("Steuer uebernehmen: {0}")), ShipName);
}

bool ADBShip::CanInteract(const APawn* User) const
{
	return User && (!Helmsman || Helmsman == User);
}

void ADBShip::Interact(APlayerController* User)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	if (Helmsman == Pawn)
	{
		ReleaseHelm();
	}
	else if (!Helmsman)
	{
		TakeHelm(Pawn);
	}
}

void ADBShip::TakeHelm(APawn* Pawn)
{
	Helmsman = Pawn;
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->GetCharacterMovement()->SetMovementMode(MOVE_None);
	}
	Pawn->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Pawn->SetActorRelativeLocation(HelmOffset);
	Pawn->SetActorRelativeRotation(FRotator::ZeroRotator);
	UE_LOG(LogDarkBlood, Log, TEXT("%s takes the helm of %s"), *GetNameSafe(Pawn), *ShipName.ToString());
}

void ADBShip::ReleaseHelm()
{
	APawn* Pawn = Helmsman;
	Helmsman = nullptr;
	Steering = FVector2D::ZeroVector;
	if (!Pawn)
	{
		return;
	}
	Pawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	UE_LOG(LogDarkBlood, Log, TEXT("%s leaves the helm of %s"), *GetNameSafe(Pawn), *ShipName.ToString());
}

void ADBShip::SetSteering(const FVector2D& Input)
{
	Steering = FVector2D(FMath::Clamp(Input.X, -1.f, 1.f), FMath::Clamp(Input.Y, -1.f, 1.f));
}

bool ADBShip::IsWaterAhead(const FVector& Location, const FVector& Forward) const
{
	// Outside the open world (test maps) there is no layout: the sea is everywhere.
	bool bRealm = false;
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		bRealm = true;
		break;
	}
	if (!bRealm)
	{
		return true;
	}
	// The bow and both bow quarters, so a turn along a cliff does not scrape the hull into the rock.
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward) * 450.f;
	for (const FVector& Probe : {Location + Forward * 1800.f, Location + Forward * 1300.f + Side, Location + Forward * 1300.f - Side})
	{
		if (DBRealm::SampleHeight(Probe.X / 100.0, Probe.Y / 100.0) >= -1.5)
		{
			return false;
		}
	}
	return true;
}

void ADBShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	WaveTime += DeltaSeconds;
	// Waves: the model rolls and pitches, the walkable deck stays level (no one slides off).
	Model->SetRelativeRotation(FRotator(FMath::Sin(WaveTime * 0.7f) * 1.2f, 0.f, FMath::Sin(WaveTime * 0.9f + 1.3f) * 2.2f));
	if (!HasAuthority())
	{
		return;
	}
	if (Helmsman && (!IsValid(Helmsman) || Helmsman->GetAttachParentActor() != this))
	{
		ReleaseHelm();
	}
	// Sails: hold forward for full sail, back to reef them; the ship coasts to a stop.
	const float Target = FMath::Max(Steering.Y, 0.f) * MaxSpeed;
	const float Rate = Steering.Y < 0.f ? 400.f : 180.f;
	Speed = FMath::FInterpConstantTo(Speed, Target, DeltaSeconds, Rate);
	if (Speed < 1.f && FMath::IsNearlyZero(Steering.X))
	{
		return;
	}
	FRotator Rotation = GetActorRotation();
	Rotation.Yaw += Steering.X * TurnRate * FMath::Clamp(Speed / (MaxSpeed * 0.3f), 0.25f, 1.f) * DeltaSeconds;
	const FVector Forward = Rotation.Vector();
	FVector Location = GetActorLocation();
	if (!IsWaterAhead(Location, Forward))
	{
		Speed = 0.f; // ran aground: stop at the shore
	}
	Location += Forward * Speed * DeltaSeconds;
	SetActorLocationAndRotation(Location, Rotation);
}
