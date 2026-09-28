#include "Art/DBModelLibrary.h"

#include "Art/DBArtBatcher.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h"

namespace
{
	const TCHAR* Root = TEXT("/Game/DarkBlood/Art/Environment/Sketchfab/");

	const DBModels::FModelInfo Models[] = {
		// Houses and shrines
		{TEXT("minka_houses"), 600.f, 0.f},
		{TEXT("japanese_house"), 800.f, 0.f},
		{TEXT("shirakawago_house"), 1500.f, 0.f},
		{TEXT("asian_shrine"), 700.f, 0.f},
		// Temples, pagodas, castles
		{TEXT("japanese_temple"), 3000.f, 0.f},
		{TEXT("temple_pagoda_lanterns"), 2600.f, 0.f},
		{TEXT("pagoda"), 2800.f, 0.f},
		{TEXT("japanese_castle"), 8000.f, 0.f},
		{TEXT("kokura_castle"), 9000.f, 0.f, 0.28f},
		// Ships
		{TEXT("junk_red_large"), 3600.f, -90.f},
		{TEXT("junk_red_small"), 3000.f, 0.f},
		{TEXT("junk_merchant"), 2600.f, -90.f, 0.f, TEXT("Material_006")},
		{TEXT("wooden_boat"), 120.f, -90.f},
		// Details
		{TEXT("lantern_hanging"), 260.f, 0.f},
		{TEXT("lantern_stone"), 180.f, 0.f},
		{TEXT("bridge_red"), 450.f, 0.f},
		{TEXT("torii_game"), 900.f, 90.f},
		{TEXT("torii_large"), 1100.f, 0.f},
	};
}

const DBModels::FModelInfo* DBModels::Find(const FString& Key)
{
	for (const FModelInfo& Model : Models)
	{
		if (Key.Equals(Model.Key, ESearchCase::IgnoreCase))
		{
			return &Model;
		}
	}
	return nullptr;
}

TArrayView<const DBModels::FModelInfo> DBModels::GetAll()
{
	return MakeArrayView(Models);
}

const TArray<FString>& DBModels::GetParts(const FString& Key)
{
	static TMap<FString, TArray<FString>> Cache;
	if (const TArray<FString>* Found = Cache.Find(Key))
	{
		return *Found;
	}
	TArray<FString>& Parts = Cache.Add(Key);
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FString Path = FString(Root) + Key;
#if WITH_EDITOR
	// An uncooked -game session may still be gathering assets: scan the model's folder now.
	Registry.ScanPathsSynchronous({Path}, false);
#endif
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	const FModelInfo* Info = Find(Key);
	for (const FAssetData& Asset : Assets)
	{
		if (Info && Info->ExcludePart && Asset.AssetName.ToString().Contains(Info->ExcludePart))
		{
			continue;
		}
		Parts.Add(Asset.PackageName.ToString());
	}
	Parts.Sort();
	return Parts;
}

bool DBModels::BuildScaledToLength(FDBArtBatcher& Batcher, const FString& Key, float Length, const FTransform& Placement)
{
	const FModelInfo* Info = Find(Key);
	if (!Info)
	{
		return false;
	}
	const FTransform Turn(FRotator(0.f, Info->Yaw, 0.f));
	TArray<UStaticMesh*> Meshes;
	FBox Bounds(ForceInit);
	for (const FString& Part : GetParts(Key))
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(Part + TEXT(".") + FPaths::GetBaseFilename(Part))))
		{
			Meshes.Add(Mesh);
			Bounds += Mesh->GetBoundingBox().TransformBy(Turn);
		}
	}
	if (Meshes.IsEmpty() || !Bounds.IsValid || Bounds.GetSize().X < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const float Scale = Length / Bounds.GetSize().X;
	const FVector Center = Bounds.GetCenter();
	const FTransform Local = Turn * FTransform(FRotator::ZeroRotator, FVector(-Center.X, -Center.Y, -Bounds.Min.Z) * Scale, FVector(Scale));
	for (UStaticMesh* Mesh : Meshes)
	{
		Batcher.Mesh(Mesh, nullptr, Local * Placement);
	}
	return true;
}
