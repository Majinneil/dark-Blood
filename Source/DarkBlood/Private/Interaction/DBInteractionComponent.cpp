#include "Interaction/DBInteractionComponent.h"

#include "DarkBlood.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/DBInteractable.h"
#include "Player/DBPlayerController.h"

UDBInteractionComponent::UDBInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDBInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	SearchTimer -= DeltaTime;
	if (SearchTimer <= 0.f)
	{
		SearchTimer = 0.1f;
		Focused = FindBestInteractable();
	}
}

AActor* UDBInteractionComponent::FindBestInteractable() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return nullptr;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DBInteraction), false, Pawn);
	World->OverlapMultiByObjectType(Overlaps, Pawn->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams::AllObjects,
		FCollisionShape::MakeSphere(SearchRadius), Params);

	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const IDBInteractable* Interactable = Cast<IDBInteractable>(Candidate);
		if (!Interactable || !Interactable->CanInteract(Pawn))
		{
			continue;
		}
		const FVector ToCandidate = Candidate->GetActorLocation() - Pawn->GetActorLocation();
		const float Distance = ToCandidate.Size2D();
		if (Distance > Interactable->GetInteractionRange())
		{
			continue;
		}
		// Prefer what the player is facing, then what is closest.
		const float Facing = FVector::DotProduct(Pawn->GetActorForwardVector(), ToCandidate.GetSafeNormal2D());
		const float Score = Distance * (1.5f - 0.5f * Facing);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

bool UDBInteractionComponent::TryInteract()
{
	// Refresh now: the prompt may be up to one search interval old.
	Focused = FindBestInteractable();
	AActor* Target = Focused.Get();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	ADBPlayerController* Controller = Pawn ? Pawn->GetController<ADBPlayerController>() : nullptr;
	if (!Target || !Controller)
	{
		return false;
	}
	Controller->ServerInteract(Target);
	return true;
}

bool UDBInteractionComponent::ServerValidateAndInteract(APlayerController* User, AActor* Target)
{
	IDBInteractable* Interactable = Cast<IDBInteractable>(Target);
	const APawn* Pawn = User ? User->GetPawn() : nullptr;
	if (!Interactable || !Pawn || !User->HasAuthority())
	{
		return false;
	}
	// Small tolerance for movement during the request.
	const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target->GetActorLocation());
	if (Distance > Interactable->GetInteractionRange() + 100.f || !Interactable->CanInteract(Pawn))
	{
		UE_LOG(LogDarkBlood, Verbose, TEXT("Interaction with %s refused (distance %.0f)"), *GetNameSafe(Target), Distance);
		return false;
	}
	Interactable->Interact(User);
	return true;
}
