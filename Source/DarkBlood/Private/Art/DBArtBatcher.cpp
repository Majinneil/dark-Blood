#include "Art/DBArtBatcher.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

FDBArtBatcher::FDBArtBatcher(AActor& InOwner, USceneComponent& InParent, TArray<TObjectPtr<UInstancedStaticMeshComponent>>& InComponents)
	: Owner(InOwner)
	, Parent(InParent)
	, Components(InComponents)
{
}

UStaticMesh* FDBArtBatcher::GetShapeMesh(EShape Shape)
{
	static TWeakObjectPtr<UStaticMesh> Cached[4];
	// Nanite copies (Tools/UE58/db_create_kit_shapes.py) first: thousands of kit pieces must not take the classic path.
	static const TCHAR* Paths[][2] = {
		{TEXT("/Game/DarkBlood/Art/Kit/Shapes/SM_DB_Cube.SM_DB_Cube"), TEXT("/Engine/BasicShapes/Cube.Cube")},
		{TEXT("/Game/DarkBlood/Art/Kit/Shapes/SM_DB_Cylinder.SM_DB_Cylinder"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder")},
		{TEXT("/Game/DarkBlood/Art/Kit/Shapes/SM_DB_Sphere.SM_DB_Sphere"), TEXT("/Engine/BasicShapes/Sphere.Sphere")},
		{TEXT("/Game/DarkBlood/Art/Kit/Shapes/SM_DB_Cone.SM_DB_Cone"), TEXT("/Engine/BasicShapes/Cone.Cone")},
	};
	const int32 Index = static_cast<int32>(Shape);
	if (!Cached[Index].IsValid())
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Paths[Index][0], nullptr, LOAD_NoWarn | LOAD_Quiet);
		Cached[Index] = Mesh ? Mesh : LoadObject<UStaticMesh>(nullptr, Paths[Index][1]);
	}
	return Cached[Index].Get();
}

UInstancedStaticMeshComponent* FDBArtBatcher::FindOrCreate(UStaticMesh* Mesh, UMaterialInterface* Material, int32 Slot)
{
	const FString Key = FString::Printf(TEXT("%p|%p|%d|%d"), Mesh, Material, Slot, bCollision ? 1 : 0);
	if (UInstancedStaticMeshComponent** Found = ByKey.Find(Key))
	{
		return *Found;
	}
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(&Owner, NAME_None, RF_Transient);
	Component->SetupAttachment(&Parent);
	Component->SetStaticMesh(Mesh);
	if (Material && Slot == INDEX_NONE)
	{
		for (int32 Index = 0; Index < Mesh->GetStaticMaterials().Num(); ++Index)
		{
			Component->SetMaterial(Index, Material);
		}
	}
	else if (Material)
	{
		Component->SetMaterial(Slot, Material);
	}
	Component->SetMobility(EComponentMobility::Static);
	Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Component->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Component->SetCanEverAffectNavigation(bCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->RegisterComponent();
	Components.Add(Component);
	ByKey.Add(Key, Component);
	return Component;
}

void FDBArtBatcher::Shape(EShape Shape, EDBArtMaterial Material, const FTransform& LocalTransform)
{
	if (UStaticMesh* ShapeMesh = GetShapeMesh(Shape))
	{
		Mesh(ShapeMesh, UDBArtMaterialSubsystem::Get(Material), LocalTransform);
	}
}

void FDBArtBatcher::Mesh(UStaticMesh* InMesh, UMaterialInterface* Material, const FTransform& LocalTransform)
{
	if (!InMesh)
	{
		return;
	}
	FindOrCreate(InMesh, Material)->AddInstance(LocalTransform, /*bWorldSpace*/ false);
	++InstanceCount;
}

void FDBArtBatcher::MeshWithSlot(UStaticMesh* InMesh, int32 Slot, UMaterialInterface* Material, const FTransform& LocalTransform)
{
	if (!InMesh)
	{
		return;
	}
	FindOrCreate(InMesh, Material, Slot)->AddInstance(LocalTransform, /*bWorldSpace*/ false);
	++InstanceCount;
}

void FDBArtBatcher::MeshWithMaterials(UStaticMesh* InMesh, const TArray<UMaterialInterface*>& Materials, const FTransform& LocalTransform)
{
	if (!InMesh)
	{
		return;
	}
	// One batch per material combination: a pseudo slot index derived from the combination keys the batch.
	uint32 Hash = GetTypeHash(InMesh);
	for (const UMaterialInterface* Material : Materials)
	{
		Hash = HashCombine(Hash, GetTypeHash(Material));
	}
	UInstancedStaticMeshComponent* Component = FindOrCreate(InMesh, nullptr, -2 - static_cast<int32>(Hash & 0xFFFFFF));
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		if (Materials[Index] && Component->GetMaterial(Index) != Materials[Index])
		{
			Component->SetMaterial(Index, Materials[Index]);
		}
	}
	Component->AddInstance(LocalTransform, /*bWorldSpace*/ false);
	++InstanceCount;
}

void FDBArtBatcher::Box(EDBArtMaterial Material, const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	Shape(EShape::Cube, Material, FTransform(Rotation, Center, Size / 100.f));
}

void FDBArtBatcher::Cylinder(EDBArtMaterial Material, const FVector& Center, float Diameter, float Height, const FRotator& Rotation)
{
	Shape(EShape::Cylinder, Material, FTransform(Rotation, Center, FVector(Diameter, Diameter, Height) / 100.f));
}

void FDBArtBatcher::Sphere(EDBArtMaterial Material, const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	Shape(EShape::Sphere, Material, FTransform(Rotation, Center, Size / 100.f));
}

void FDBArtBatcher::Cone(EDBArtMaterial Material, const FVector& Center, float Diameter, float Height, const FRotator& Rotation)
{
	Shape(EShape::Cone, Material, FTransform(Rotation, Center, FVector(Diameter, Diameter, Height) / 100.f));
}
