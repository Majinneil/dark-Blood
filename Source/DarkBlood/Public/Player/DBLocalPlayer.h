// The local player sends a lasting player key with every login (?DBKey=...). Without an online platform (the NULL
// subsystem of LAN / direct-IP play) the net id changes with every start, and a dedicated server would no longer find
// the player's characters; the key keeps them (Phase 19). With Steam / EOS the platform id is used instead.
#pragma once

#include "Engine/LocalPlayer.h"

#include "DBLocalPlayer.generated.h"

UCLASS()
class DARKBLOOD_API UDBLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	virtual FString GetGameLoginOptions() const override;

	/** This machine's player key (created once under Saved/SaveGames; -DBPlayerKey=<32 hex> overrides it for tests). */
	static FString GetPlayerKey();
	/** A key is exactly 32 hexadecimal characters. */
	static bool IsValidPlayerKey(const FString& Key);
};
