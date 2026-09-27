// Anything the player can use with the interact button (NPCs, doors, shrines, chests ...).
#pragma once

#include "UObject/Interface.h"

#include "DBInteractable.generated.h"

class APawn;
class APlayerController;

UINTERFACE(MinimalAPI, BlueprintType)
class UDBInteractable : public UInterface
{
	GENERATED_BODY()
};

class DARKBLOOD_API IDBInteractable
{
	GENERATED_BODY()

public:
	/** Prompt shown to the player, e.g. "Sprechen: Koenig Aoki". */
	virtual FText GetInteractionText() const = 0;

	/** Checked locally for the prompt and again on the server before Interact. */
	virtual bool CanInteract(const APawn* User) const = 0;

	/** Server only: performs the interaction for this player. */
	virtual void Interact(APlayerController* User) = 0;

	/** Maximum distance (cm) between the user and the interactable. */
	virtual float GetInteractionRange() const { return 300.f; }
};
