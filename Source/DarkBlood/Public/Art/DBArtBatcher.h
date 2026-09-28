// Instanced geometry helper for the procedural art kit: one instanced static mesh component per
// (mesh, material, collision) combination, so a whole house is a handful of draw calls.
// Primitive fallbacks use the engine basic shapes (100 cm, centered) until real kit meshes are assigned.
#pragma once

#include "Art/DBArtMaterials.h"
#include "CoreMinimal.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

class DARKBLOOD_API FDBArtBatcher
{
public:
	enum class EShape : uint8
	{
		Cube,
		Cylinder,
		Sphere,
		Cone,
	};

	FDBArtBatcher(AActor& InOwner, USceneComponent& InParent, TArray<TObjectPtr<UInstancedStaticMeshComponent>>& InComponents);

	/** Structural pieces block and affect navigation; ornaments (lattices, ribs, lanterns) do not. */
	void SetCollision(bool bEnabled) { bCollision = bEnabled; }

	/** Shapes in the parent's local space; Size in cm. */
	void Box(EDBArtMaterial Material, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);
	void Cylinder(EDBArtMaterial Material, const FVector& Center, float Diameter, float Height, const FRotator& Rotation = FRotator::ZeroRotator);
	void Sphere(EDBArtMaterial Material, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator);
	void Cone(EDBArtMaterial Material, const FVector& Center, float Diameter, float Height, const FRotator& Rotation = FRotator::ZeroRotator);
	void Shape(EShape Shape, EDBArtMaterial Material, const FTransform& LocalTransform);

	/** Authored kit mesh (keeps its own materials when Material is null). */
	void Mesh(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& LocalTransform);

	/** Authored mesh with one material slot replaced (e.g. blossom leaves on a tree). */
	void MeshWithSlot(UStaticMesh* Mesh, int32 Slot, UMaterialInterface* Material, const FTransform& LocalTransform);

	/** Authored mesh with per-slot materials (null entries keep the authored material). */
	void MeshWithMaterials(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials, const FTransform& LocalTransform);

	/** Authored mesh with every slot replaced (e.g. a tree turned into corrupted, vein-lit wood). */
	void MeshAllSlots(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& LocalTransform) { MeshWithSlot(Mesh, INDEX_NONE, Material, LocalTransform); }

	int32 GetInstanceCount() const { return InstanceCount; }

	static UStaticMesh* GetShapeMesh(EShape Shape);

private:
	UInstancedStaticMeshComponent* FindOrCreate(UStaticMesh* Mesh, UMaterialInterface* Material, int32 Slot = 0);

	AActor& Owner;
	USceneComponent& Parent;
	TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Components;
	TMap<FString, UInstancedStaticMeshComponent*> ByKey;
	bool bCollision = true;
	int32 InstanceCount = 0;
};
