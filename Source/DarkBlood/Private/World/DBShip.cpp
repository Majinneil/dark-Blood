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
	/** The actor is the walkable deck box; its center lies this far below the deck surface. */
	constexpr float DeckHalfHeight = 60.f;

	/** Relative to the actor: standing on the raised stern deck. */
	FVector HelmOffset(const FDBShipSpec& Spec)
	{
		return FVector(-Spec.Length * 0.43f, 0.f, Spec.SternDeckZ - (Spec.DeckZ - DeckHalfHeight) + 95.f);
	}
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
	Hull->SetBoxExtent(FVector(1500.f, 380.f, DeckHalfHeight));
	Hull->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Hull->SetMobility(EComponentMobility::Movable);
	Hull->SetCanEverAffectNavigation(false);
	RootComponent = Hull;

	Model = CreateDefaultSubobject<USceneComponent>(TEXT("Model"));
	Model->SetupAttachment(Hull);
}

void ADBShip::BeginPlay()
{
	Super::BeginPlay();
	ApplyStyle();
}

void ADBShip::OnRep_Style()
{
	if (HasActorBegunPlay())
	{
		ApplyStyle();
	}
}

void ADBShip::ApplyStyle()
{
	const FDBShipSpec& Spec = DBShipArt::GetSpec(Style);
	Hull->SetBoxExtent(FVector(Spec.Length * 0.4f, Spec.Beam * 0.4f, DeckHalfHeight));
	Model->SetRelativeLocation(FVector(0.f, 0.f, -(Spec.DeckZ - DeckHalfHeight)));
	MaxSpeed = Spec.MaxSpeed;
	TurnRate = Spec.TurnRate;
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (UInstancedStaticMeshComponent* Piece : ModelPieces)
	{
		if (Piece)
		{
			Piece->DestroyComponent();
		}
	}
	ModelPieces.Reset();
	// The model rolls with the waves, so its pieces are movable and do not collide (the level deck box does).
	FDBArtBatcher Batcher(*this, *Model, ModelPieces);
	Batcher.SetMovable(true);
	DBShipArt::Build(Batcher, Style, 7, false); // same seed on every machine
}

void ADBShip::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBShip, Helmsman);
	DOREPLIFETIME_CONDITION(ADBShip, Style, COND_InitialOnly);
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

ADBShip* ADBShip::SpawnAt(UWorld* World, const FVector2D& Location, float Yaw, const FText& Name, EDBShipStyle Style)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	// The actor sits at the deck box center; the water line lies DeckZ - DeckHalfHeight below it.
	const FDBShipSpec& Spec = DBShipArt::GetSpec(Style);
	const FTransform At(FRotator(0.f, Yaw, 0.f), FVector(Location.X, Location.Y, Spec.DeckZ - DeckHalfHeight));
	ADBShip* Ship = World->SpawnActorDeferred<ADBShip>(ADBShip::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Ship)
	{
		Ship->Style = Style;
		Ship->ShipName = Name;
		Ship->FinishSpawning(At);
	}
	return Ship;
}

float ADBShip::GetInteractionRange() const
{
	return DBShipArt::GetSpec(Style).Length * 0.5f + 400.f;
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
	Pawn->SetActorRelativeLocation(HelmOffset(DBShipArt::GetSpec(Style)));
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
	const FDBShipSpec& Spec = DBShipArt::GetSpec(Style);
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward) * Spec.Beam * 0.6f;
	const FVector Bow = Forward * Spec.Length * 0.6f;
	const FVector Quarter = Forward * Spec.Length * 0.43f;
	for (const FVector& Probe : {Location + Bow, Location + Quarter + Side, Location + Quarter - Side})
	{
		if (DBRealm::SampleHeight(Probe.X / 100.0, Probe.Y / 100.0) >= -Spec.Draft)
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
	// Big ships ride the waves more calmly than boats.
	const float Swell = FMath::Clamp(3000.f / DBShipArt::GetSpec(Style).Length, 0.6f, 2.5f);
	Model->SetRelativeRotation(FRotator(FMath::Sin(WaveTime * 0.7f) * 1.2f * Swell, 0.f, FMath::Sin(WaveTime * 0.9f + 1.3f) * 2.2f * Swell));
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
