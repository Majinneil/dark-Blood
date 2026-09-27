#include "Art/DBArtMaterials.h"

#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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
			{nullptr, FLinearColor(0.004f, 0.004f, 0.004f)},
		};
		static_assert(UE_ARRAY_COUNT(Infos) == static_cast<int32>(EDBArtMaterial::Count), "EDBArtMaterial and slot table out of sync");
		return Infos[static_cast<int32>(Slot)];
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
