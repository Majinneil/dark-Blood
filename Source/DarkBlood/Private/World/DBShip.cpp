#include "World/DBShip.h"

#include "Art/DBArtBatcher.h"
#include "Art/DBShipArt.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DarkBlood.h"
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
	/** Relative to the actor (the deck box center, 60 cm below the deck surface): standing on the raised stern deck. */
	const FVector HelmOffset(-1150.f, 0.f, 300.f);
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
	// A bezaisen from the art kit; the model rolls with the waves, so its pieces are movable and do not collide (the
	// level deck box does).
	FDBArtBatcher Batcher(*this, *Model, ModelPieces);
	Batcher.SetMovable(true);
	DBShipArt::Build(Batcher, EDBShipStyle::Bezaisen, 7, false); // same seed on every machine
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
