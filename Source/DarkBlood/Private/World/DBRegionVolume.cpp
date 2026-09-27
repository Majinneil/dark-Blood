#include "World/DBRegionVolume.h"

#include "Components/BrushComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/DBPlayerState.h"

ADBRegionVolume::ADBRegionVolume()
{
	GetBrushComponent()->SetCollisionProfileName(TEXT("Trigger"));
	GetBrushComponent()->SetGenerateOverlapEvents(true);
	bReplicates = false;
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif
}

void ADBRegionVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (HasAuthority() && Pawn)
	{
		if (ADBPlayerState* PlayerState = Pawn->GetPlayerState<ADBPlayerState>())
		{
			PlayerState->EnterRegionVolume(this);
		}
	}
}

void ADBRegionVolume::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (HasAuthority() && Pawn)
	{
		if (ADBPlayerState* PlayerState = Pawn->GetPlayerState<ADBPlayerState>())
		{
			PlayerState->ExitRegionVolume(this);
		}
	}
}
