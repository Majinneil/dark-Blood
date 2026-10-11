// Moving players across the world (dungeons, the Abyss, the Hall of Echoes, gates, carriages). Several players often
// arrive at the same spot: an engine teleport onto an occupied spot silently fails, so the arrival is spread over a ring
// of free places around the target (Phase 19).
#pragma once

#include "CoreMinimal.h"

class APawn;

namespace DBTeleport
{
	/** Server: moves Pawn to the free place nearest to Location (rings of 1.3 m and 2.6 m), facing Yaw; the controller
	 *  looks the same way. Falls back to the exact spot without the overlap check. Returns false only without a pawn. */
	DARKBLOOD_API bool MovePawn(APawn* Pawn, const FVector& Location, float Yaw);
}
