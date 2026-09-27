// Player-side interaction: finds the best interactable in front of the local player (for the prompt) and
// asks the server to use it. The server re-validates range and conditions.
#pragma once

#include "Components/ActorComponent.h"

#include "DBInteractionComponent.generated.h"

UCLASS(ClassGroup = (DarkBlood), meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Local: current candidate (nullptr if none). */
	AActor* GetFocusedInteractable() const { return Focused.Get(); }

	/** Local: uses the focused interactable. Returns false if there is none. */
	bool TryInteract();

	/** Server: validates and performs the interaction for the owning player. */
	static bool ServerValidateAndInteract(APlayerController* User, AActor* Target);

protected:
	AActor* FindBestInteractable() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dark Blood|Interaction", meta = (Units = "cm"))
	float SearchRadius = 350.f;

private:
	TWeakObjectPtr<AActor> Focused;
	float SearchTimer = 0.f;
};
