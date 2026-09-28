// Authored models from free sources (Sketchfab CC BY / CC0, credited in docs/CREDITS.md), imported by
// Tools/UE58/db_import_sketchfab.py to /Game/DarkBlood/Art/Environment/Sketchfab/<Key>/. Every static mesh in a model's
// folder is one of its parts (found through the asset registry, so a re-import needs no code change). The library knows
// how tall each model stands in the world and how to turn it so its front faces +X; the art kit places them like its
// own pieces and falls back to kit geometry when a model is not imported.
#pragma once

#include "CoreMinimal.h"

class FDBArtBatcher;

namespace DBModels
{
	struct FModelInfo
	{
		const TCHAR* Key;
		/** World height (cm) the model is scaled to. */
		float Height;
		/** Turn (deg) that makes the model's front face +X. */
		float Yaw;
		/** Share of the height sunk into the ground (a scanned hill block under a castle). */
		float Sink = 0.f;
		/** Parts whose name contains this are left out (a stray piece far off the model). */
		const TCHAR* ExcludePart = nullptr;
	};

	/** Library entry of a model key, or null. */
	DARKBLOOD_API const FModelInfo* Find(const FString& Key);

	/** Package paths of the model's static meshes (empty when it is not imported). */
	DARKBLOOD_API const TArray<FString>& GetParts(const FString& Key);

	/** All library entries (tests, showroom). */
	DARKBLOOD_API TArrayView<const FModelInfo> GetAll();

	/** Puts a model's parts into Batcher, turned to face +X, scaled so it is Length long (X) and standing with its bottom
	 *  at Placement's origin. Returns false when the model is not imported. */
	DARKBLOOD_API bool BuildScaledToLength(FDBArtBatcher& Batcher, const FString& Key, float Length, const FTransform& Placement);
}
