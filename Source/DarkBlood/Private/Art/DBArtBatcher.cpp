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
	static const TCHAR* Paths[] = {
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		TEXT("/Engine/BasicShapes/Sphere.Sphere"),
		TEXT("/Engine/BasicShapes/Cone.Cone"),
	};
	const int32 Index = static_cast<int32>(Shape);
	if (!Cached[Index].IsValid())
	{
		Cached[Index] = LoadObject<UStaticMesh>(nullptr, Paths[Index]);
	}
	return Cached[Index].Get();
}

UInstancedStaticMeshComponent* FDBArtBatcher::FindOrCreate(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	const FString Key = FString::Printf(TEXT("%p|%p|%d"), Mesh, Material, bCollision ? 1 : 0);
	if (UInstancedStaticMeshComponent** Found = ByKey.Find(Key))
	{
		return *Found;
	}
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(&Owner, NAME_None, RF_Transient);
	Component->SetupAttachment(&Parent);
	Component->SetStaticMesh(Mesh);
	if (Material)
	{
		Component->SetMaterial(0, Material);
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
