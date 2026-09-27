#include "Art/DBArtMaterials.h"

#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"

namespace
{
	struct FSlotInfo
	{
		const TCHAR* Asset;
		FLinearColor Fallback;
	};

	const FSlotInfo& GetInfo(EDBArtMaterial Slot)
	{
		static const FSlotInfo Infos[] = {
			{TEXT("Instances/MI_DB_Wood_Weathered_Dark"), FLinearColor(0.085f, 0.055f, 0.035f)},
			{TEXT("Instances/MI_DB_Wood_New_Light"), FLinearColor(0.42f, 0.28f, 0.16f)},
			{TEXT("Instances/MI_DB_Wood_Wet"), FLinearColor(0.07f, 0.045f, 0.03f)},
			{TEXT("Instances/MI_DB_Wood_Burnt"), FLinearColor(0.02f, 0.018f, 0.016f)},
			{TEXT("Instances/MI_DB_Wood_Lacquer_Red"), FLinearColor(0.36f, 0.04f, 0.025f)},
			{TEXT("Instances/MI_DB_Wood_Lacquer_Black"), FLinearColor(0.02f, 0.018f, 0.017f)},
			{TEXT("Instances/MI_DB_Stone_Dry"), FLinearColor(0.3f, 0.29f, 0.27f)},
			{TEXT("Instances/MI_DB_Stone_Wet"), FLinearColor(0.16f, 0.155f, 0.15f)},
			{TEXT("Instances/MI_DB_Stone_Mossy"), FLinearColor(0.2f, 0.22f, 0.15f)},
			{TEXT("Instances/MI_DB_Stone_Temple"), FLinearColor(0.42f, 0.4f, 0.36f)},
			{TEXT("Instances/MI_DB_Stone_Mountain"), FLinearColor(0.22f, 0.22f, 0.23f)},
			{TEXT("Instances/MI_DB_Stone_Ruin"), FLinearColor(0.2f, 0.19f, 0.17f)},
			{TEXT("Instances/MI_DB_Stone_Corrupted"), FLinearColor(0.1f, 0.06f, 0.06f)},
			{TEXT("Instances/MI_DB_Plaster_Lime"), FLinearColor(0.7f, 0.66f, 0.58f)},
			{TEXT("Instances/MI_DB_Plaster_Clay"), FLinearColor(0.36f, 0.27f, 0.18f)},
			{TEXT("Instances/MI_DB_Paper_Shoji"), FLinearColor(0.78f, 0.74f, 0.64f)},
			{TEXT("Instances/MI_DB_Paper_Shoji_Lit"), FLinearColor(0.95f, 0.8f, 0.55f)},
			{TEXT("Instances/MI_DB_Roof_Tile_Dark"), FLinearColor(0.1f, 0.1f, 0.11f)},
			{TEXT("Instances/MI_DB_Roof_Thatch"), FLinearColor(0.23f, 0.18f, 0.1f)},
			{TEXT("Instances/MI_DB_Roof_Copper"), FLinearColor(0.12f, 0.26f, 0.2f)},
			{TEXT("Instances/MI_DB_Metal_Iron_Dark"), FLinearColor(0.12f, 0.115f, 0.11f)},
			{TEXT("Instances/MI_DB_Metal_Bronze"), FLinearColor(0.45f, 0.3f, 0.14f)},
			{TEXT("Instances/MI_DB_Fabric_Linen"), FLinearColor(0.55f, 0.5f, 0.42f)},
			{TEXT("Instances/MI_DB_Fabric_Crimson"), FLinearColor(0.3f, 0.025f, 0.025f)},
			{TEXT("Instances/MI_DB_Fabric_Indigo"), FLinearColor(0.03f, 0.04f, 0.12f)},
			{TEXT("Instances/MI_DB_Lantern_Paper"), FLinearColor(1.f, 0.6f, 0.3f)},
			{TEXT("Instances/MI_DB_Lantern_Fire"), FLinearColor(1.f, 0.5f, 0.2f)},
			{TEXT("Instances/MI_DB_Water_Stream"), FLinearColor(0.02f, 0.035f, 0.035f)},
			{TEXT("Instances/MI_DB_Ground_PackedEarth"), FLinearColor(0.15f, 0.115f, 0.08f)},
			{TEXT("Instances/MI_DB_Ground_ForestFloor"), FLinearColor(0.075f, 0.06f, 0.035f)},
			{TEXT("Instances/MI_DB_Ground_Courtyard"), FLinearColor(0.33f, 0.315f, 0.29f)},
			{TEXT("Instances/MI_DB_Foliage_Leaves"), FLinearColor(0.06f, 0.12f, 0.03f)},
			{TEXT("Instances/MI_DB_Foliage_Sakura"), FLinearColor(0.85f, 0.42f, 0.52f)},
			{TEXT("Instances/MI_DB_Foliage_Dead"), FLinearColor(0.08f, 0.06f, 0.04f)},
			{TEXT("DarkBlood/MI_DB_DarkBlood_Veins"), FLinearColor(0.06f, 0.02f, 0.02f)},
			{TEXT("DarkBlood/MI_DB_DarkBlood_Soil"), FLinearColor(0.05f, 0.03f, 0.025f)},
			{TEXT("DarkBlood/MI_DB_DarkBlood_Stone"), FLinearColor(0.1f, 0.085f, 0.085f)},
			{TEXT("Instances/MI_DB_Bark_Cedar"), FLinearColor(0.12f, 0.08f, 0.06f)},
			{TEXT("Instances/MI_DB_Bark_Sakura"), FLinearColor(0.14f, 0.09f, 0.08f)},
			{nullptr, FLinearColor(0.004f, 0.004f, 0.004f)},
		};
		static_assert(UE_ARRAY_COUNT(Infos) == static_cast<int32>(EDBArtMaterial::Count), "EDBArtMaterial and slot table out of sync");
		return Infos[static_cast<int32>(Slot)];
	}

	/** Poly Haven imports (Tools/UE58/db_import_polyhaven.py), relative to /Game/DarkBlood/Art/Environment/PolyHaven. */
	const TArray<const TCHAR*>& GetMeshNames(EDBArtMeshSet Set)
	{
		static const TArray<const TCHAR*> Sets[] = {
			{TEXT("tree_small_02/tree_small_02_1k/StaticMeshes/tree_small_02_1k")},
			{TEXT("dead_tree_trunk_02/dead_tree_trunk_02_1k/StaticMeshes/dead_tree_trunk_02_1k")},
			{TEXT("shrub_02/shrub_02_1k/StaticMeshes/shrub_02_a"), TEXT("shrub_02/shrub_02_1k/StaticMeshes/shrub_02_b"),
				TEXT("shrub_02/shrub_02_1k/StaticMeshes/shrub_02_c"), TEXT("shrub_02/shrub_02_1k/StaticMeshes/shrub_02_d"),
				TEXT("shrub_04/shrub_04_1k/StaticMeshes/shrub_04_1k")},
			{TEXT("fern_02/fern_02_1k/StaticMeshes/fern_02_a"), TEXT("fern_02/fern_02_1k/StaticMeshes/fern_02_b"),
				TEXT("fern_02/fern_02_1k/StaticMeshes/fern_02_c"), TEXT("fern_02/fern_02_1k/StaticMeshes/fern_02_d")},
			{TEXT("moss_01/moss_01_1k/StaticMeshes/moss_01_a_LOD0"), TEXT("moss_01/moss_01_1k/StaticMeshes/moss_01_c_LOD0"),
				TEXT("moss_01/moss_01_1k/StaticMeshes/moss_01_e_LOD0"), TEXT("moss_01/moss_01_1k/StaticMeshes/moss_01_g_LOD0"),
				TEXT("moss_01/moss_01_1k/StaticMeshes/moss_01_tall_a_LOD0"), TEXT("moss_01/moss_01_1k/StaticMeshes/moss_01_tall_b_LOD0")},
			{TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock01"),
				TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock02"),
				TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock03"),
				TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock04"),
				TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock05"),
				TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock06"),
				TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock07"),
				TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock08"),
				TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock09"),
				TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock10"),
				TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock11"),
				TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock12"),
				TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock13")},
			{TEXT("boulder_01/boulder_01_1k/StaticMeshes/boulder_01_1k")},
			{TEXT("tree_stump_01/tree_stump_01_1k/StaticMeshes/tree_stump_01_1k")},
		};
		static_assert(UE_ARRAY_COUNT(Sets) == static_cast<int32>(EDBArtMeshSet::Count), "EDBArtMeshSet and mesh table out of sync");
		return Sets[static_cast<int32>(Set)];
	}

	UDBArtMaterialSubsystem* GetSubsystem()
	{
		return GEngine ? GEngine->GetEngineSubsystem<UDBArtMaterialSubsystem>() : nullptr;
	}
}

FString UDBArtMaterialSubsystem::GetAssetPath(EDBArtMaterial Slot)
{
	const FSlotInfo& Info = GetInfo(Slot);
	if (!Info.Asset)
	{
		return FString();
	}
	const FString Name = FPaths::GetCleanFilename(Info.Asset);
	return FString::Printf(TEXT("/Game/DarkBlood/Art/Materials/%s.%s"), Info.Asset, *Name);
}

UMaterialInterface* UDBArtMaterialSubsystem::Get(EDBArtMaterial Slot)
{
	UDBArtMaterialSubsystem* Subsystem = GetSubsystem();
	return Subsystem ? Subsystem->Resolve(Slot) : nullptr;
}

bool UDBArtMaterialSubsystem::IsAuthored(EDBArtMaterial Slot)
{
	UDBArtMaterialSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->Resolve(Slot))
	{
		return false;
	}
	return Subsystem->Authored[static_cast<int32>(Slot)];
}

UMaterialInterface* UDBArtMaterialSubsystem::Resolve(EDBArtMaterial Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (Index < 0 || Index >= static_cast<int32>(EDBArtMaterial::Count))
	{
		return nullptr;
	}
	if (Cache.Num() != static_cast<int32>(EDBArtMaterial::Count))
	{
		Cache.SetNum(static_cast<int32>(EDBArtMaterial::Count));
		Authored.Init(false, static_cast<int32>(EDBArtMaterial::Count));
	}
	if (Cache[Index])
	{
		return Cache[Index];
	}

	const FString Path = GetAssetPath(Slot);
	UMaterialInterface* Material = Path.IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Authored[Index] = Material != nullptr;
	if (!Material)
	{
		// Flat fallback so the procedural kit still reads by color.
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* Flat = UMaterialInstanceDynamic::Create(Base, this);
			Flat->SetVectorParameterValue(TEXT("Color"), GetInfo(Slot).Fallback);
			Material = Flat;
		}
	}
	Cache[Index] = Material;
	return Material;
}

TArray<FString> UDBArtMaterialSubsystem::GetMeshPaths(EDBArtMeshSet Set)
{
	TArray<FString> Paths;
	for (const TCHAR* Name : GetMeshNames(Set))
	{
		Paths.Add(FString::Printf(TEXT("/Game/DarkBlood/Art/Environment/PolyHaven/%s.%s"), Name, *FPaths::GetCleanFilename(Name)));
	}
	return Paths;
}

const TArray<TObjectPtr<UStaticMesh>>& UDBArtMaterialSubsystem::GetMeshes(EDBArtMeshSet Set)
{
	static const TArray<TObjectPtr<UStaticMesh>> Empty;
	UDBArtMaterialSubsystem* Subsystem = GetSubsystem();
	const int32 Index = static_cast<int32>(Set);
	if (!Subsystem || Index < 0 || Index >= static_cast<int32>(EDBArtMeshSet::Count))
	{
		return Empty;
	}
	if (Subsystem->MeshSets.Num() != static_cast<int32>(EDBArtMeshSet::Count))
	{
		Subsystem->MeshSets.SetNum(static_cast<int32>(EDBArtMeshSet::Count));
		for (int32 SetIndex = 0; SetIndex < static_cast<int32>(EDBArtMeshSet::Count); ++SetIndex)
		{
			for (const FString& Path : GetMeshPaths(static_cast<EDBArtMeshSet>(SetIndex)))
			{
				if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					Subsystem->MeshSets[SetIndex].Meshes.Add(Mesh);
				}
			}
		}
	}
	return Subsystem->MeshSets[Index].Meshes;
}

UStaticMesh* UDBArtMaterialSubsystem::PickMesh(EDBArtMeshSet Set, FRandomStream& Random)
{
	const TArray<TObjectPtr<UStaticMesh>>& Meshes = GetMeshes(Set);
	return Meshes.Num() > 0 ? Meshes[Random.RandRange(0, Meshes.Num() - 1)].Get() : nullptr;
}
