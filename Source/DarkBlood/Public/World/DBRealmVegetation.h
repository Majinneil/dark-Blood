// Trees and rocks of one 1 km cell of the open world, baked into L_Realm by DBBuildRealmCommandlet (one
// instanced batch per mesh / material combination). Small ground cover (grass, ferns) comes from the landscape grass
// types instead. Static, saved with the map; nothing is generated at runtime.
#pragma once

#include "GameFramework/Actor.h"

#include "DBRealmVegetation.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

UCLASS()
class DARKBLOOD_API ADBRealmVegetation : public AActor
{
	GENERATED_BODY()

public:
	ADBRealmVegetation();

	/** Editor (realm builder): adds an instance; Materials override slots by index (null = authored). */
	void AddInstance(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials, const FTransform& WorldTransform, bool bCollision);

	int32 GetInstanceCount() const;

	/** Runtime: removes the trees and rocks standing inside a circle (boss arenas clear their floor). Returns how many. */
	int32 RemoveInstancesInCircle(const FVector& Center, float Radius);

private:
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood|Realm")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Batches;

	TMap<uint32, TObjectPtr<UInstancedStaticMeshComponent>> BatchByKey;
};
