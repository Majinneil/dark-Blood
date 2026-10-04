// Shared types of the visual layer (characters, animation, art kit). Purely cosmetic: gameplay never reads them.
// See docs/VISUAL_FOUNDATION.md.
#pragma once

#include "CoreMinimal.h"

#include "DBVisualTypes.generated.h"

class UMaterialInterface;
class USkeletalMesh;

/** Render budget of a character (LOD, animation update rate, face cost). */
UENUM(BlueprintType)
enum class EDBVisualQualityTier : uint8
{
	/** Player characters: full rig, always animated. */
	Player,
	/** Story heroes / vassals / the king: highest quality, full face. */
	Hero,
	/** Quest NPCs, merchants, guards. */
	ImportantNpc,
	/** Civilians and common enemies: reduced LODs, animation only when visible. */
	Crowd,
};

/** Modular slots of a character. Every part follows the body's pose (leader pose). */
UENUM(BlueprintType)
enum class EDBVisualSlot : uint8
{
	Face,
	Hair,
	Beard,
	Outfit,
	Armor,
	Accessory,
};

/** One swappable skeletal part (hair, outfit layer, armor piece ...). */
USTRUCT(BlueprintType)
struct FDBVisualPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FName PartId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	EDBVisualSlot Slot = EDBVisualSlot::Outfit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Optional material for every section of the part. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UMaterialInterface> Material;
};
