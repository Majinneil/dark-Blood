// Infinite periodic effect that regenerates health, stamina and mana from the *Regen attributes.
// Configured in C++ so it works without authored assets; a Blueprint child can override it.
#pragma once

#include "GameplayEffect.h"

#include "DBRegenerationEffect.generated.h"

UCLASS()
class DARKBLOOD_API UDBRegenerationEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	static constexpr float TickSeconds = 0.25f;

	UDBRegenerationEffect();
};
