// DEVELOPMENT story slice: spawns the first story beat (king, training dummies, captain, demon encounter)
// around a transform, so it can be played in any map until L_Realm exists. Enabled with -DBDevSlice or the
// DBSetupSlice command. Server only.
#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace DBDevelopmentSlice
{
	/** Spawns the slice relative to Origin (X = forward). Does nothing if it already exists. */
	DARKBLOOD_API void Spawn(UWorld* World, const FTransform& Origin);
}
