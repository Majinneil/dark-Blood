#include "Character/DBCharacterMovementComponent.h"

#include "DarkBlood.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PhysicsVolume.h"
#include "World/DBRealmDirector.h"

namespace
{
	const FName SeaTag(TEXT("DBSea"));
	/** The realm's sea level (DBRealm frame: 0 m). */
	constexpr float SeaLevel = 0.f;
}

UDBCharacterMovementComponent::UDBCharacterMovementComponent()
{
	// Buoyancy balances gravity at ~75 % immersion: the head stays above the surface.
	Buoyancy = 1.3f;
	MaxSwimSpeed = 350.f;
	NavAgentProps.bCanSwim = true;
}

bool UDBCharacterMovementComponent::IsInRealm()
{
	const UWorld* World = GetWorld();
	if (!bRealm && World && World->GetTimeSeconds() >= NextRealmCheck)
	{
		// The realm director is always loaded in the open world and absent elsewhere.
		bRealm = ADBRealmDirector::Get(World) != nullptr;
		NextRealmCheck = World->GetTimeSeconds() + 2.0;
	}
	return bRealm;
}

APhysicsVolume* UDBCharacterMovementComponent::GetSeaVolume()
{
	if (SeaVolume.IsValid())
	{
		return SeaVolume.Get();
	}
	UWorld* World = GetWorld();
	for (TActorIterator<APhysicsVolume> It(World); It; ++It)
	{
		if (It->ActorHasTag(SeaTag))
		{
			SeaVolume = *It;
			return *It;
		}
	}
	// One local volume per world, never replicated: it has no brush and is only ever assigned by this component.
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APhysicsVolume* Volume = World->SpawnActor<APhysicsVolume>(APhysicsVolume::StaticClass(), FTransform::Identity, Params);
	if (Volume)
	{
		Volume->SetReplicates(false);
		Volume->bWaterVolume = true;
		Volume->FluidFriction = 0.3f;
		Volume->Tags.Add(SeaTag);
		SeaVolume = Volume;
	}
	return Volume;
}

bool UDBCharacterMovementComponent::IsInSeaVolume() const
{
	return SeaVolume.IsValid() && GetPhysicsVolume() == SeaVolume.Get();
}

void UDBCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	if (!UpdatedComponent || !CharacterOwner || !IsInRealm())
	{
		return;
	}
	APhysicsVolume* Sea = GetSeaVolume();
	if (!Sea)
	{
		return;
	}
	// The sea volume has no shape: overlap updates would always fall back to the world's default volume.
	UpdatedComponent->SetShouldUpdatePhysicsVolume(false);

	const float HalfHeight = CharacterOwner->GetSimpleCollisionHalfHeight();
	const float CenterZ = UpdatedComponent->GetComponentLocation().Z;
	const bool bInSea = IsInSeaVolume();
	// Enter when the water reaches the chest, leave when the hips are out; the gap keeps the surface stable.
	const bool bWantSea = bInSea ? CenterZ < SeaLevel + HalfHeight * 0.3f : CenterZ < SeaLevel - HalfHeight * 0.2f;
	if (bWantSea != bInSea)
	{
		// Triggers PhysicsVolumeChanged: swimming starts, or the character climbs / jumps out of the water.
		UpdatedComponent->SetPhysicsVolume(bWantSea ? Sea : GetWorld()->GetDefaultPhysicsVolume(), true);
	}
}

float UDBCharacterMovementComponent::ImmersionDepth() const
{
	if (CharacterOwner && UpdatedComponent && IsInSeaVolume())
	{
		const float HalfHeight = CharacterOwner->GetSimpleCollisionHalfHeight();
		const float Bottom = UpdatedComponent->GetComponentLocation().Z - HalfHeight;
		return HalfHeight > 0.f ? FMath::Clamp((SeaLevel - Bottom) / (2.f * HalfHeight), 0.f, 1.f) : 1.f;
	}
	return Super::ImmersionDepth();
}

void UDBCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (CharacterOwner && (MovementMode == MOVE_Swimming) != (PreviousMovementMode == MOVE_Swimming))
	{
		UE_LOG(LogDarkBlood, Log, TEXT("%s %s"), *CharacterOwner->GetName(), MovementMode == MOVE_Swimming ? TEXT("swims") : TEXT("leaves the water"));
	}
}
