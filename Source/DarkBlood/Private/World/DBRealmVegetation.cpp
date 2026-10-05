#include "World/DBRealmVegetation.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

ADBRealmVegetation::ADBRealmVegetation()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void ADBRealmVegetation::AddInstance(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials, const FTransform& WorldTransform, bool bCollision)
{
	if (!Mesh)
	{
		return;
	}
	uint32 Key = HashCombine(GetTypeHash(Mesh), GetTypeHash(bCollision));
	for (const UMaterialInterface* Material : Materials)
	{
		Key = HashCombine(Key, GetTypeHash(Material));
	}
	TObjectPtr<UInstancedStaticMeshComponent>* Found = BatchByKey.Find(Key);
	UInstancedStaticMeshComponent* Batch = Found ? Found->Get() : nullptr;
	if (!Batch)
	{
		Batch = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);
		Batch->SetupAttachment(Root);
		Batch->SetMobility(EComponentMobility::Static);
		Batch->SetStaticMesh(Mesh);
		for (int32 Index = 0; Index < Materials.Num(); ++Index)
		{
			if (Materials[Index])
			{
				Batch->SetMaterial(Index, Materials[Index]);
			}
		}
		Batch->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Batch->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Batch->SetCanEverAffectNavigation(false);
		Batch->SetGenerateOverlapEvents(false);
		AddInstanceComponent(Batch);
		Batch->RegisterComponent();
		Batches.Add(Batch);
		BatchByKey.Add(Key, Batch);
	}
	Batch->AddInstance(WorldTransform, /*bWorldSpace*/ true);
}

int32 ADBRealmVegetation::GetInstanceCount() const
{
	int32 Count = 0;
	for (const UInstancedStaticMeshComponent* Batch : Batches)
	{
		Count += Batch ? Batch->GetInstanceCount() : 0;
	}
	return Count;
}

int32 ADBRealmVegetation::RemoveInstancesInCircle(const FVector& Center, float Radius)
{
	int32 Removed = 0;
	for (UInstancedStaticMeshComponent* Batch : Batches)
	{
		const FBox Bounds = Batch ? Batch->Bounds.GetBox() : FBox(ForceInit);
		if (!Batch || !Bounds.IsValid || Bounds.ComputeSquaredDistanceToPoint(FVector(Center.X, Center.Y, Bounds.GetCenter().Z)) > FMath::Square(Radius))
		{
			continue;
		}
		TArray<int32> Doomed;
		for (int32 Index = 0; Index < Batch->GetInstanceCount(); ++Index)
		{
			FTransform Instance;
			if (Batch->GetInstanceTransform(Index, Instance, true) && FVector::Dist2D(Instance.GetLocation(), Center) < Radius)
			{
				Doomed.Add(Index);
			}
		}
		if (Doomed.Num() > 0)
		{
			Batch->RemoveInstances(Doomed);
			Removed += Doomed.Num();
		}
	}
	return Removed;
}
