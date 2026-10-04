// DEVELOPMENT CONTENT - clearly marked placeholder definitions created in code.
// They exist only so the game is runnable before real data assets are authored in the editor.
// Any asset with the same id (Content/DarkBlood/Data/...) replaces the placeholder automatically.
#pragma once

#include "CoreMinimal.h"

class UDBGameDataSubsystem;

struct DARKBLOOD_API FDBDevelopmentContent
{
	/** Registers placeholder classes, items, regions and quests that are not provided by assets. Returns true if any were added. */
	static bool RegisterMissing(UDBGameDataSubsystem& Data);
};
