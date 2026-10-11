#include "Audio/DBFootstepComponent.h"

#include "Audio/DBAudioSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/DBCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LandscapeProxy.h"
#include "World/DBRealmLayout.h"
#include "World/DBShip.h"

namespace
{
	/** Beyond this the steps are not worth a sound (cm). */
	constexpr float HearingDistance = 2500.f;
}

UDBFootstepComponent::UDBFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
	SetIsReplicatedByDefault(false);
}

void UDBFootstepComponent::BeginPlay()
{
	Super::BeginPlay();
	// No ears on a dedicated server or without an audio device (headless tests).
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !World->GetAudioDeviceRaw())
	{
		SetComponentTickEnabled(false);
		return;
	}
	LastLocation = GetOwner()->GetActorLocation();
}

FName UDBFootstepComponent::GetLandscapeStep(const FVector& Location, float& OutPitch)
{
	OutPitch = 1.f;
	const double X = Location.X / 100.0;
	const double Y = Location.Y / 100.0;
	const double Height = DBRealm::SampleHeight(X, Y);
	const double Dx = DBRealm::SampleHeight(X + 2.0, Y) - DBRealm::SampleHeight(X - 2.0, Y);
	const double Dy = DBRealm::SampleHeight(X, Y + 2.0) - DBRealm::SampleHeight(X, Y - 2.0);
	const double NormalZ = 1.0 / FMath::Sqrt(1.0 + FMath::Square(Dx / 4.0) + FMath::Square(Dy / 4.0));
	uint8 Weights[static_cast<int32>(EDBRealmLayer::Count)] = {};
	DBRealm::SampleLayers(X, Y, Height, NormalZ, Weights);
	int32 Strongest = 0;
	for (int32 Layer = 1; Layer < static_cast<int32>(EDBRealmLayer::Count); ++Layer)
	{
		Strongest = Weights[Layer] > Weights[Strongest] ? Layer : Strongest;
	}
	switch (static_cast<EDBRealmLayer>(Strongest))
	{
	case EDBRealmLayer::Meadow:
	case EDBRealmLayer::Forest: return TEXT("Footsteps/Grass");
	case EDBRealmLayer::Rock: return TEXT("Footsteps/Stone");
	case EDBRealmLayer::Snow: return TEXT("Footsteps/Snow");
	case EDBRealmLayer::Sand: OutPitch = 0.9f; return TEXT("Footsteps/Dirt");
	case EDBRealmLayer::Corrupt: OutPitch = 0.85f; return TEXT("Footsteps/Dirt");
	case EDBRealmLayer::Lava: OutPitch = 0.85f; return TEXT("Footsteps/Stone");
	default: return TEXT("Footsteps/Dirt");
	}
}

void UDBFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ADBCharacterBase* Character = Cast<ADBCharacterBase>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || Character->IsDead())
	{
		return;
	}
	const FVector Location = Character->GetActorLocation();
	const float Moved = FVector::Dist2D(Location, LastLocation);
	LastLocation = Location;
	const bool bOnGround = Movement->IsMovingOnGround();
	const APlayerController* Local = GetWorld()->GetFirstPlayerController();
	const FVector Ear = Local && Local->PlayerCameraManager ? Local->PlayerCameraManager->GetCameraLocation() : Location;
	const bool bHeard = FVector::DistSquared(Ear, Location) < FMath::Square(HearingDistance);
	const bool bDemon = Character->GetTeam() == EDBTeam::Demons;
	const FVector Feet = Location - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	auto StepSound = [&](float& OutPitch) -> FName
	{
		const AActor* Floor = Movement->CurrentFloor.HitResult.GetActor();
		OutPitch = 1.f;
		if (Floor && Floor->IsA<ADBShip>())
		{
			return TEXT("Footsteps/Wood");
		}
		if (!Floor || Floor->IsA<ALandscapeProxy>())
		{
			return GetLandscapeStep(Location, OutPitch);
		}
		return TEXT("Footsteps/Stone");
	};

	// Landing after a jump or a fall: one firm step.
	if (bOnGround && !bLastOnGround && bHeard)
	{
		float Pitch = 1.f;
		const FName Sound = StepSound(Pitch);
		UDBAudioSubsystem::PlayAt(this, Sound, Feet, EDBSoundReach::Near, 1.f, Pitch * 0.85f);
		Travelled = 0.f;
	}
	bLastOnGround = bOnGround;
	if (!bOnGround || Moved < 1.f || Moved > 400.f)
	{
		return;
	}
	// A stride is longer when running; demons and big bosses take longer, heavier strides.
	const float Speed = Moved / FMath::Max(DeltaTime, 1e-3f);
	const float Scale = Character->GetActorScale3D().Z;
	const float Stride = (Speed > 380.f ? 185.f : 135.f) * FMath::Max(1.f, Scale);
	Travelled += Moved;
	if (Travelled < Stride)
	{
		return;
	}
	Travelled = FMath::Fmod(Travelled, Stride);
	++Steps;
	if (!bHeard)
	{
		return;
	}
	float Pitch = 1.f;
	const FName Sound = StepSound(Pitch);
	const float Volume = (Speed > 380.f ? 0.75f : 0.5f) * (bDemon ? 0.85f : 1.f) * FMath::Min(Scale, 2.f);
	UDBAudioSubsystem::PlayAt(this, Sound, Feet, Scale > 1.3f ? EDBSoundReach::Combat : EDBSoundReach::Near, Volume, Pitch * (bDemon ? 0.82f : 1.f) / FMath::Sqrt(Scale));
}
