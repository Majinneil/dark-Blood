#include "World/DBBuildRealmCommandlet.h"

#include "DarkBlood.h"
#include "World/DBRealmLayout.h"

#if WITH_EDITOR
#include "Async/ParallelFor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "LandscapeEditTypes.h"
#include "LandscapeSubsystem.h"
#include "AssetCompilingManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "World/DBRealmDirector.h"
#include "World/DBRealmVegetation.h"
#include "Math/RandomStream.h"
#endif

UDBBuildRealmCommandlet::UDBBuildRealmCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace
{
	constexpr int32 SectionQuads = 127;
	constexpr int32 SectionsPerComponent = 2;
	constexpr int32 ComponentsPerSide = 16;
	constexpr int32 Quads = SectionQuads * SectionsPerComponent * ComponentsPerSide; // 4064
	constexpr int32 Verts = Quads + 1;
	constexpr double QuadMeters = 4.0;
	/** Landscape Z scale: +-1024 m around 0 in 16 bit. */
	constexpr double ScaleZ = 400.0;
	constexpr int32 LayerCount = static_cast<int32>(EDBRealmLayer::Count);

	double VertexX(int32 Column) { return -DBRealm::HalfSize + Column * QuadMeters; }
	double VertexY(int32 Row) { return -DBRealm::HalfSize + Row * QuadMeters; }

	uint16 ToLandscapeValue(double Meters)
	{
		// Height[cm] = (Value - 32768) * ScaleZ / 128
		return static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(32768.0 + Meters * 100.0 * 128.0 / ScaleZ), 0, 65535));
	}

	/** Map-style preview (dominant paint, hillshade, water) as a 24-bit BMP. */
	void WritePreview(const TArray<float>& Heights, const TArray<uint8>& Dominant, const FString& Path)
	{
		constexpr int32 Step = 4;
		constexpr int32 Size = Verts / Step;
		static const FColor Colors[] = {FColor(98, 140, 58), FColor(60, 82, 38), FColor(118, 114, 108), FColor(235, 238, 245), FColor(214, 186, 128),
			FColor(130, 104, 74), FColor(96, 30, 32), FColor(170, 48, 18)};
		TArray<uint8> Pixels;
		Pixels.SetNumZeroed(Size * Size * 3);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 Index = (Y * Step) * Verts + X * Step;
				const float H = Heights[Index];
				const float Hx = Heights[FMath::Min(Index + Step, Heights.Num() - 1)];
				const float Hy = Heights[FMath::Min(Index + Step * Verts, Heights.Num() - 1)];
				const float Shade = FMath::Clamp(1.f + ((H - Hx) + (H - Hy)) * 0.02f, 0.45f, 1.35f);
				FColor Color = H < 0.f ? FColor(30, 70 + static_cast<uint8>(FMath::Clamp(H + 40.f, 0.f, 40.f)), 120) : Colors[Dominant[Index]];
				if (H >= 0.f)
				{
					Color = FColor(static_cast<uint8>(FMath::Clamp(Color.R * Shade, 0.f, 255.f)), static_cast<uint8>(FMath::Clamp(Color.G * Shade, 0.f, 255.f)),
						static_cast<uint8>(FMath::Clamp(Color.B * Shade, 0.f, 255.f)));
				}
				// BMP rows run bottom-up; north (map top, -Y) must end up on top.
				const int32 Out = ((Size - 1 - Y) * Size + X) * 3;
				Pixels[Out + 0] = Color.B;
				Pixels[Out + 1] = Color.G;
				Pixels[Out + 2] = Color.R;
			}
		}
		const int32 RowBytes = Size * 3;
		const int32 Padding = (4 - RowBytes % 4) % 4;
		TArray<uint8> File;
		auto Put32 = [&File](uint32 Value) { File.Append(reinterpret_cast<uint8*>(&Value), 4); };
		auto Put16 = [&File](uint16 Value) { File.Append(reinterpret_cast<uint8*>(&Value), 2); };
		const uint32 DataSize = (RowBytes + Padding) * Size;
		File.Add('B');
		File.Add('M');
		Put32(54 + DataSize);
		Put32(0);
		Put32(54);
		Put32(40);
		Put32(Size);
		Put32(Size);
		Put16(1);
		Put16(24);
		Put32(0);
		Put32(DataSize);
		Put32(2835);
		Put32(2835);
		Put32(0);
		Put32(0);
		for (int32 Row = 0; Row < Size; ++Row)
		{
			File.Append(&Pixels[Row * RowBytes], RowBytes);
			for (int32 Pad = 0; Pad < Padding; ++Pad)
			{
				File.Add(0);
			}
		}
		FFileHelper::SaveArrayToFile(File, *Path);
	}

	enum class ESpecies : uint8
	{
		TreeSmall,
		IslandTree,
		Sakura,
		Fir,
		DeadLog,
		DemonTree,
		Boulder,
		RockMoss,
		RockFace,
		Count
	};

	struct FPlacement
	{
		ESpecies Species;
		FTransform Transform;
	};

	/** Trees and rocks by region character, on a jittered 10 m grid; nothing in water, on cliffs or in settlements. */
	TArray<FPlacement> PlaceVegetation(const TArray<float>& Heights, const TArray<TArray<uint8>>& Layers)
	{
		using B = EDBRealmBiome;
		TArray<FPlacement> Result;
		FRandomStream Random(1709);
		const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
		const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
		constexpr double Spacing = 10.0;
		const int32 Steps = FMath::FloorToInt(2.0 * DBRealm::HalfSize / Spacing);
		auto Sample = [&](double X, double Y, int32& OutIndex, float& OutNormalZ)
		{
			const int32 Column = FMath::Clamp(FMath::RoundToInt((X + DBRealm::HalfSize) / QuadMeters), 1, Verts - 2);
			const int32 Row = FMath::Clamp(FMath::RoundToInt((Y + DBRealm::HalfSize) / QuadMeters), 1, Verts - 2);
			OutIndex = Row * Verts + Column;
			const FVector Normal(Heights[OutIndex - 1] - Heights[OutIndex + 1], Heights[OutIndex - Verts] - Heights[OutIndex + Verts], 2.0 * QuadMeters);
			OutNormalZ = static_cast<float>(Normal.GetSafeNormal().Z);
			return Heights[OutIndex];
		};
		auto Weight = [&](EDBRealmLayer Layer, int32 Index) { return Layers[static_cast<int32>(Layer)][Index] / 255.f; };
		for (int32 StepY = 0; StepY < Steps; ++StepY)
		{
			for (int32 StepX = 0; StepX < Steps; ++StepX)
			{
				const double X = -DBRealm::HalfSize + (StepX + Random.FRand()) * Spacing;
				const double Y = -DBRealm::HalfSize + (StepY + Random.FRand()) * Spacing;
				const float Roll = Random.FRand();
				const float Pick = Random.FRand();
				int32 Index;
				float NormalZ;
				const float Height = Sample(X, Y, Index, NormalZ);
				if (Height < 1.5f)
				{
					continue;
				}
				bool bInSettlement = FVector2D::Distance(FVector2D(X, Y), DBRealm::GetCapital().Center) < 700.0;
				for (const FDBRealmSettlement& Site : Settlements)
				{
					bInSettlement |= FVector2D::Distance(FVector2D(X, Y), Site.Center) < Site.Radius * 1.15;
				}
				if (bInSettlement)
				{
					continue;
				}
				const float Forest = Weight(EDBRealmLayer::Forest, Index);
				const float Meadow = Weight(EDBRealmLayer::Meadow, Index);
				const float Rock = Weight(EDBRealmLayer::Rock, Index);
				const float Snow = Weight(EDBRealmLayer::Snow, Index);
				const float Sand = Weight(EDBRealmLayer::Sand, Index);
				const float Corrupt = Weight(EDBRealmLayer::Corrupt, Index);
				// Rocks: on bare rock, sparse in deserts and corrupted land.
				const float RockChance = Rock * 0.018f + Sand * 0.0015f + Corrupt * 0.008f;
				ESpecies Species = ESpecies::Count;
				float Scale = 1.f;
				if (Roll < RockChance)
				{
					Species = Pick < 0.3f ? ESpecies::Boulder : (Pick < 0.8f ? ESpecies::RockMoss : ESpecies::RockFace);
					Scale = Species == ESpecies::RockMoss ? Random.FRandRange(1.f, 3.2f) : Random.FRandRange(0.6f, 2.f);
				}
				else if (NormalZ > 0.8f)
				{
					const B Biome = Regions[DBRealm::FindRegionIndex(X, Y)].Biome;
					float TreeChance = Forest * 0.55f + Meadow * 0.02f;
					switch (Biome)
					{
					case B::CherryValley: TreeChance += Meadow * 0.05f; break;
					case B::BambooForest: TreeChance = 0.3f + Forest * 0.2f; break;
					case B::MistMountains: TreeChance = (1.f - Snow) * (0.08f + Forest * 0.45f + Meadow * 0.06f); break;
					case B::IceWaste: TreeChance = (1.f - Snow) * 0.03f + 0.004f; break;
					case B::SpiritForest: TreeChance = 0.22f + Forest * 0.3f; break;
					case B::FireMountains: TreeChance = 0.003f; break;
					case B::DemonWaste:
					case B::TheEnd: TreeChance = 0.012f; break;
					case B::Desert: TreeChance = 0.f; break;
					default: break;
					}
					if (Roll >= RockChance + TreeChance)
					{
						continue;
					}
					switch (Biome)
					{
					case B::Capital:
						Species = Pick < 0.4f ? ESpecies::Sakura : ESpecies::TreeSmall;
						break;
					case B::CherryValley:
						Species = Pick < 0.7f ? ESpecies::Sakura : ESpecies::TreeSmall;
						break;
					case B::MistMountains:
					case B::IceWaste:
						Species = ESpecies::Fir;
						break;
					case B::SpiritForest:
						Species = Pick < 0.6f ? ESpecies::IslandTree : (Pick < 0.8f ? ESpecies::DeadLog : ESpecies::TreeSmall);
						break;
					case B::FireMountains:
					case B::DemonWaste:
					case B::TheEnd:
						Species = Pick < 0.5f ? ESpecies::DemonTree : ESpecies::DeadLog;
						break;
					default:
						Species = Pick < 0.65f ? ESpecies::TreeSmall : ESpecies::IslandTree;
						break;
					}
					switch (Species)
					{
					case ESpecies::TreeSmall: Scale = Random.FRandRange(1.1f, 2.0f); break;
					case ESpecies::IslandTree: Scale = Random.FRandRange(1.4f, 2.6f); break;
					case ESpecies::Sakura: Scale = Random.FRandRange(1.4f, 2.3f); break;
					case ESpecies::Fir: Scale = Random.FRandRange(2.2f, 3.8f); break;
					case ESpecies::DemonTree: Scale = Random.FRandRange(1.8f, 3.5f); break;
					default: Scale = Random.FRandRange(1.f, 1.8f); break;
					}
				}
				if (Species == ESpecies::Count)
				{
					continue;
				}
				const FVector Location(X * 100.0, Y * 100.0, Height * 100.0 - 20.0);
				Result.Add({Species, FTransform(FRotator(0.0, Random.FRandRange(0.f, 360.f), 0.0), Location, FVector(Scale))});
			}
		}
		return Result;
	}

	template <typename T>
	T* Spawn(UWorld& World, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World.SpawnActor<T>(T::StaticClass(), Location, Rotation, Params);
	}

	bool SaveAsset(UPackage* Package, UObject* Asset, const FString& Extension)
	{
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), Extension);
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}
}
#endif

int32 UDBBuildRealmCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bPreviewOnly = Params.Contains(TEXT("-Preview"));
	const double StartSeconds = FPlatformTime::Seconds();

	// ---- 1. Height and paint from the layout --------------------------------------------------------------------
	TArray<float> HeightsMeters;
	HeightsMeters.SetNumUninitialized(Verts * Verts);
	ParallelFor(Verts, [&](int32 Row)
	{
		for (int32 Column = 0; Column < Verts; ++Column)
		{
			HeightsMeters[Row * Verts + Column] = static_cast<float>(DBRealm::SampleHeight(VertexX(Column), VertexY(Row)));
		}
	});
	TArray<TArray<uint8>> Layers;
	Layers.SetNum(LayerCount);
	for (TArray<uint8>& Layer : Layers)
	{
		Layer.SetNumZeroed(Verts * Verts);
	}
	TArray<uint8> Dominant;
	Dominant.SetNumZeroed(Verts * Verts);
	ParallelFor(Verts, [&](int32 Row)
	{
		for (int32 Column = 0; Column < Verts; ++Column)
		{
			const int32 Index = Row * Verts + Column;
			const float Left = HeightsMeters[Row * Verts + FMath::Max(Column - 1, 0)];
			const float Right = HeightsMeters[Row * Verts + FMath::Min(Column + 1, Verts - 1)];
			const float Up = HeightsMeters[FMath::Max(Row - 1, 0) * Verts + Column];
			const float Down = HeightsMeters[FMath::Min(Row + 1, Verts - 1) * Verts + Column];
			const FVector Normal = FVector(Left - Right, Up - Down, 2.0 * QuadMeters).GetSafeNormal();
			uint8 Weights[LayerCount];
			DBRealm::SampleLayers(VertexX(Column), VertexY(Row), HeightsMeters[Index], Normal.Z, Weights);
			int32 Best = 0;
			for (int32 Layer = 0; Layer < LayerCount; ++Layer)
			{
				Layers[Layer][Index] = Weights[Layer];
				Best = Weights[Layer] > Weights[Best] ? Layer : Best;
			}
			Dominant[Index] = static_cast<uint8>(Best);
		}
	});
	const FString PreviewPath = FPaths::ProjectSavedDir() / TEXT("Realm/RealmPreview.bmp");
	WritePreview(HeightsMeters, Dominant, PreviewPath);
	float MinHeight = TNumericLimits<float>::Max();
	float MaxHeight = TNumericLimits<float>::Lowest();
	int32 LandVerts = 0;
	for (const float Height : HeightsMeters)
	{
		MinHeight = FMath::Min(MinHeight, Height);
		MaxHeight = FMath::Max(MaxHeight, Height);
		LandVerts += Height > 0.f ? 1 : 0;
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBREALM terrain %dx%d (%.0f m quads): height %.0f .. %.0f m, land %.0f %%, preview %s (%.1f s)"), Verts, Verts, QuadMeters,
		MinHeight, MaxHeight, 100.0 * LandVerts / HeightsMeters.Num(), *PreviewPath, FPlatformTime::Seconds() - StartSeconds);
	const TArray<FPlacement> Vegetation = PlaceVegetation(HeightsMeters, Layers);
	UE_LOG(LogDarkBlood, Display, TEXT("DBREALM vegetation: %d trees and rocks"), Vegetation.Num());
	if (bPreviewOnly)
	{
		return 0;
	}

	// ---- 2. Map, paint layer assets ----------------------------------------------------------------------------
	const FString MapName = TEXT("/Game/DarkBlood/Maps/L_Realm");
	// Rebuild from scratch: the previous map file goes away first (it would otherwise be loaded into the new package).
	IFileManager::Get().Delete(*FPackageName::LongPackageNameToFilename(MapName, FPackageName::GetMapPackageExtension()), false, true, true);
	UPackage* MapPackage = CreatePackage(*MapName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("L_Realm"), MapPackage);
	World->SetFlags(RF_Public | RF_Standalone);

	TArray<ULandscapeLayerInfoObject*> LayerInfos;
	for (int32 Layer = 0; Layer < LayerCount; ++Layer)
	{
		const FName LayerName(DBRealm::GetLayerName(static_cast<EDBRealmLayer>(Layer)));
		const FString AssetName = FString::Printf(TEXT("LI_Realm_%s"), *LayerName.ToString());
		const FString LayerPath = TEXT("/Game/DarkBlood/Maps/Realm/") + AssetName;
		ULandscapeLayerInfoObject* Info = LoadObject<ULandscapeLayerInfoObject>(nullptr, *(LayerPath + TEXT(".") + AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Info)
		{
			UPackage* LayerPackage = CreatePackage(*LayerPath);
			Info = NewObject<ULandscapeLayerInfoObject>(LayerPackage, *AssetName, RF_Public | RF_Standalone);
			Info->SetLayerName(LayerName, false);
			SaveAsset(LayerPackage, Info, FPackageName::GetAssetPackageExtension());
		}
		LayerInfos.Add(Info);
	}

	// ---- 3. Landscape: 4 x 4 tiles of 4 x 4 components (~4 km each) ---------------------------------------------------
	// Separate landscapes sharing their edge vertices: the Nanite build then works tile by tile (one 16 km landscape needs
	// more memory to build than a 32 GB machine has) and culling / streaming works per tile.
	constexpr int32 TilesPerSide = 4;
	constexpr int32 TileQuads = Quads / TilesPerSide; // 1016
	const double HalfCm = DBRealm::HalfSize * 100.0;
	UMaterialInterface* LandscapeMaterial =
		LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Landscape/M_DB_Realm_Landscape.M_DB_Realm_Landscape"));
	// (bEnableNanite is protected and has no setter; the editor sets it through reflection as well.)
	FBoolProperty* NaniteProperty = Params.Contains(TEXT("-Nanite")) ? FindFProperty<FBoolProperty>(ALandscapeProxy::StaticClass(), TEXT("bEnableNanite")) : nullptr;
	int32 Components = 0;
	int32 NaniteTiles = 0;
	for (int32 TileY = 0; TileY < TilesPerSide; ++TileY)
	{
		for (int32 TileX = 0; TileX < TilesPerSide; ++TileX)
		{
			const int32 TileVerts = TileQuads + 1;
			TArray<uint16> Heights;
			Heights.SetNumUninitialized(TileVerts * TileVerts);
			TArray<FLandscapeImportLayerInfo> ImportLayers;
			for (int32 Layer = 0; Layer < LayerCount; ++Layer)
			{
				FLandscapeImportLayerInfo& Import = ImportLayers.Emplace_GetRef(LayerInfos[Layer]->GetLayerName());
				Import.LayerInfo = LayerInfos[Layer];
				Import.LayerData.SetNumUninitialized(TileVerts * TileVerts);
			}
			for (int32 Row = 0; Row < TileVerts; ++Row)
			{
				for (int32 Column = 0; Column < TileVerts; ++Column)
				{
					const int32 Source = (TileY * TileQuads + Row) * Verts + TileX * TileQuads + Column;
					const int32 Target = Row * TileVerts + Column;
					Heights[Target] = ToLandscapeValue(HeightsMeters[Source]);
					for (int32 Layer = 0; Layer < LayerCount; ++Layer)
					{
						ImportLayers[Layer].LayerData[Target] = Layers[Layer][Source];
					}
				}
			}
			TMap<FGuid, TArray<uint16>> HeightData;
			HeightData.Add(FGuid(), MoveTemp(Heights));
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerData;
			LayerData.Add(FGuid(), MoveTemp(ImportLayers));

			const FVector Corner(-HalfCm + TileX * TileQuads * QuadMeters * 100.0, -HalfCm + TileY * TileQuads * QuadMeters * 100.0, 0.0);
			ALandscape* Tile = Spawn<ALandscape>(*World, Corner);
			Tile->SetActorRelativeScale3D(FVector(QuadMeters * 100.0, QuadMeters * 100.0, ScaleZ));
			Tile->SetActorLabel(FString::Printf(TEXT("Landscape_%d_%d"), TileX, TileY));
			Tile->LandscapeMaterial = LandscapeMaterial;
			Tile->StaticLightingLOD = 1;
			if (NaniteProperty)
			{
				NaniteProperty->SetPropertyValue_InContainer(Tile, true);
			}
			Tile->Import(FGuid::NewGuid(), 0, 0, TileQuads, TileQuads, SectionsPerComponent, SectionQuads, HeightData, nullptr, LayerData,
				ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
			ULandscapeInfo* TileInfo = Tile->GetLandscapeInfo();
			TileInfo->UpdateLayerInfoMap(Tile);
			for (ULandscapeLayerInfoObject* Info : LayerInfos)
			{
				Tile->AddTargetLayer(Info->GetLayerName(), FLandscapeTargetLayerSettings(Info));
				const int32 LayerIndex = TileInfo->GetLayerInfoIndex(Info->GetLayerName());
				if (LayerIndex != INDEX_NONE)
				{
					TileInfo->Layers[LayerIndex].LayerInfoObj = Info;
				}
			}
			Components += TileInfo->XYtoComponentMap.Num();
			if (NaniteProperty)
			{
				// One tile at a time: the build runs on task threads, pump the game thread until it is done.
				Tile->UpdateNaniteRepresentation(nullptr);
				const double Deadline = FPlatformTime::Seconds() + 600.0;
				while (!Tile->IsNaniteMeshUpToDate() && FPlatformTime::Seconds() < Deadline)
				{
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
					FAssetCompilingManager::Get().ProcessAsyncTasks();
					FPlatformProcess::Sleep(0.05f);
				}
				NaniteTiles += Tile->IsNaniteMeshUpToDate() ? 1 : 0;
				// Release the intermediate build data before the next tile (it adds up to more than 32 GB otherwise).
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			}
		}
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBREALM landscape: %d tiles, %d components, Nanite %s"), TilesPerSide * TilesPerSide, Components,
		NaniteProperty ? *FString::Printf(TEXT("built for %d tiles"), NaniteTiles) : TEXT("off"));

	// ---- 4. Vegetation, one actor per 1 km cell ----------------------------------------------------------------------
	{
		const FString Root = TEXT("/Game/DarkBlood/Art/Environment/PolyHaven/");
		auto Mesh = [&Root](const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, *(Root + Path)); };
		auto Material = [](const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); };
		UStaticMesh* Island = Mesh(TEXT("island_tree_02/island_tree_02_1k/StaticMeshes/island_tree_02_1k.island_tree_02_1k"));
		UMaterialInterface* SakuraLeaves = Material(TEXT("/Game/DarkBlood/Art/Materials/Instances/MI_DB_Foliage_Sakura_Leaves.MI_DB_Foliage_Sakura_Leaves"));
		UMaterialInterface* Veins = Material(TEXT("/Game/DarkBlood/Art/Materials/DarkBlood/MI_DB_DarkBlood_Veins.MI_DB_DarkBlood_Veins"));
		UMaterialInterface* DemonLeaves = Material(TEXT("/Game/DarkBlood/Art/Materials/DarkBlood/MI_DB_Foliage_Demon_Leaves.MI_DB_Foliage_Demon_Leaves"));
		TArray<UMaterialInterface*> SakuraMaterials;
		TArray<UMaterialInterface*> DemonMaterials;
		if (Island)
		{
			for (const FStaticMaterial& Slot : Island->GetStaticMaterials())
			{
				const bool bLeaves = Slot.MaterialSlotName.ToString().Contains(TEXT("leaves"));
				SakuraMaterials.Add(bLeaves ? SakuraLeaves : nullptr);
				DemonMaterials.Add(bLeaves ? DemonLeaves : Veins);
			}
		}
		struct FSpeciesAsset
		{
			TArray<UStaticMesh*> Meshes;
			const TArray<UMaterialInterface*>* Materials;
		};
		const TArray<UMaterialInterface*> None;
		const FSpeciesAsset Assets[] = {
			{{Mesh(TEXT("tree_small_02/tree_small_02_1k/StaticMeshes/tree_small_02_1k.tree_small_02_1k"))}, &None},
			{{Island}, &None},
			{{Island}, &SakuraMaterials},
			{{Mesh(TEXT("fir_sapling_medium/fir_sapling_medium_1k/StaticMeshes/fir_sapling_medium_a_LOD0.fir_sapling_medium_a_LOD0")),
				 Mesh(TEXT("fir_sapling_medium/fir_sapling_medium_1k/StaticMeshes/fir_sapling_medium_b_LOD0.fir_sapling_medium_b_LOD0")),
				 Mesh(TEXT("fir_sapling_medium/fir_sapling_medium_1k/StaticMeshes/fir_sapling_medium_c_LOD0.fir_sapling_medium_c_LOD0"))},
				&None},
			{{Mesh(TEXT("dead_tree_trunk_02/dead_tree_trunk_02_1k/StaticMeshes/dead_tree_trunk_02_1k.dead_tree_trunk_02_1k"))}, &None},
			{{Island}, &DemonMaterials},
			{{Mesh(TEXT("boulder_01/boulder_01_1k/StaticMeshes/boulder_01_1k.boulder_01_1k"))}, &None},
			{{Mesh(TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock01.rock_moss_set_01_rock01")),
				 Mesh(TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock02.rock_moss_set_01_rock02")),
				 Mesh(TEXT("rock_moss_set_01/rock_moss_set_01_1k/StaticMeshes/rock_moss_set_01_rock03.rock_moss_set_01_rock03")),
				 Mesh(TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock07.rock_moss_set_02_rock07")),
				 Mesh(TEXT("rock_moss_set_02/rock_moss_set_02_1k/StaticMeshes/rock_moss_set_02_rock09.rock_moss_set_02_rock09"))},
				&None},
			{{Mesh(TEXT("rock_face_01/rock_face_01_2k/StaticMeshes/rock_face_01_2k.rock_face_01_2k")),
				 Mesh(TEXT("rock_face_02/rock_face_02_2k/StaticMeshes/rock_face_02_2k.rock_face_02_2k"))},
				&None},
		};
		static_assert(UE_ARRAY_COUNT(Assets) == static_cast<int32>(ESpecies::Count), "one asset entry per species");
		TMap<FIntPoint, ADBRealmVegetation*> Cells;
		FRandomStream Pick(4242);
		int32 Placed = 0;
		for (const FPlacement& Placement : Vegetation)
		{
			const FSpeciesAsset& Asset = Assets[static_cast<int32>(Placement.Species)];
			UStaticMesh* Chosen = Asset.Meshes.Num() > 0 ? Asset.Meshes[Pick.RandRange(0, Asset.Meshes.Num() - 1)] : nullptr;
			if (!Chosen)
			{
				continue;
			}
			const FVector Location = Placement.Transform.GetLocation();
			const FIntPoint Cell(FMath::FloorToInt((Location.X / 100.0 + DBRealm::HalfSize) / 1000.0), FMath::FloorToInt((Location.Y / 100.0 + DBRealm::HalfSize) / 1000.0));
			ADBRealmVegetation*& Actor = Cells.FindOrAdd(Cell);
			if (!Actor)
			{
				const FVector CellCenter((Cell.X + 0.5) * 100000.0 - DBRealm::HalfSize * 100.0, (Cell.Y + 0.5) * 100000.0 - DBRealm::HalfSize * 100.0, 0.0);
				Actor = Spawn<ADBRealmVegetation>(*World, CellCenter);
				Actor->SetActorLabel(FString::Printf(TEXT("Vegetation_%02d_%02d"), Cell.X, Cell.Y));
			}
			Actor->AddInstance(Chosen, *Asset.Materials, Placement.Transform, true);
			++Placed;
		}
		UE_LOG(LogDarkBlood, Display, TEXT("DBREALM vegetation baked: %d instances in %d cells"), Placed, Cells.Num());
	}

	// ---- 5. Sea, sky, light, start, director ----------------------------------------------------------------------
	if (AStaticMeshActor* Sea = Spawn<AStaticMeshActor>(*World, FVector(0.0, 0.0, -30.0)))
	{
		Sea->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
		Sea->GetStaticMeshComponent()->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Instances/MI_DB_Water_Sea.MI_DB_Water_Sea")));
		Sea->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Sea->GetStaticMeshComponent()->SetCastShadow(false);
		Sea->SetActorScale3D(FVector(40000.0, 40000.0, 1.0)); // the 1 m engine plane, 40 km across
		Sea->SetActorLabel(TEXT("Sea"));
	}
	if (ADirectionalLight* Sun = Spawn<ADirectionalLight>(*World, FVector(0.0, 0.0, 50000.0), FRotator(-35.0, 140.0, 0.0)))
	{
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sun->GetLightComponent()->SetIntensity(8.f);
		Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->SetAtmosphereSunLight(true);
	}
	if (ASkyLight* Sky = Spawn<ASkyLight>(*World, FVector(0.0, 0.0, 50000.0)))
	{
		Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->SetRealTimeCapture(true);
	}
	Spawn<ASkyAtmosphere>(*World, FVector::ZeroVector);
	Spawn<AVolumetricCloud>(*World, FVector::ZeroVector);
	if (AExponentialHeightFog* Fog = Spawn<AExponentialHeightFog>(*World, FVector::ZeroVector))
	{
		Fog->GetComponent()->SetFogDensity(0.01f);
		Fog->GetComponent()->SetVolumetricFog(true);
	}
	const FDBRealmRegion& Capital = DBRealm::GetCapital();
	const FVector CapitalCenter(Capital.Center.X * 100.0, Capital.Center.Y * 100.0, DBRealm::SampleHeight(Capital.Center.X, Capital.Center.Y) * 100.0);
	Spawn<APlayerStart>(*World, CapitalCenter + FVector(0.0, 0.0, 120.0));
	Spawn<ADBRealmDirector>(*World, CapitalCenter);

	// ---- 6. Save ------------------------------------------------------------------------------------------------
	World->UpdateWorldComponents(true, false);
	const bool bSaved = SaveAsset(MapPackage, World, FPackageName::GetMapPackageExtension());
	UE_LOG(LogDarkBlood, Display, TEXT("DBREALM %s %s (%.1f s total)"), *MapName, bSaved ? TEXT("saved") : TEXT("SAVE FAILED"), FPlatformTime::Seconds() - StartSeconds);
	World->DestroyWorld(false);
	return bSaved ? 0 : 1;
#else
	return 1;
#endif
}
