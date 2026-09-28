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
