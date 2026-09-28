#include "Art/DBModularBuilding.h"

#include "Art/DBArtBatcher.h"
#include "Art/DBBuildingKitDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"

using EShape = FDBArtBatcher::EShape;
using M = EDBArtMaterial;

namespace
{
	constexpr float PostSize = 22.f;
	constexpr float WallThickness = 12.f;
	constexpr float FloorHeight = 300.f;
	constexpr float WallHeight = 270.f; // floor to underside of the top beam
	constexpr float DoorHeight = 205.f;

	enum class EBay : uint8
	{
		Plaster,
		Shoji,
		Plank,
		Window,
		Door,
		Open,
		ShopFront,
	};

	/** Side frames: 0 front (+X), 1 back (-X), 2 right (+Y), 3 left (-Y). Local X points outwards. */
	FTransform SideFrame(int32 Side)
	{
		static const float Yaw[] = {0.f, 180.f, 90.f, -90.f};
		return FTransform(FRotator(0.f, Yaw[Side], 0.f));
	}

	void PutShape(FDBArtBatcher& Batcher, EShape Shape, M Material, const FTransform& Parent, const FVector& Center, const FVector& Size,
		const FRotator& Rotation = FRotator::ZeroRotator)
	{
		Batcher.Shape(Shape, Material, FTransform(Rotation, Center, Size / 100.f) * Parent);
	}

	void PutBox(FDBArtBatcher& Batcher, M Material, const FTransform& Parent, const FVector& Center, const FVector& Size,
		const FRotator& Rotation = FRotator::ZeroRotator)
	{
		PutShape(Batcher, EShape::Cube, Material, Parent, Center, Size, Rotation);
	}
}

struct ADBModularBuilding::FPalette
{
	M Post = M::WoodDark;
	M Beam = M::WoodDark;
	M WallUpper = M::PlasterClay;
	M WallLower = M::WoodDark;
	M Paper = M::PaperShoji;
	M Plank = M::WoodDark;
	M Roof = M::RoofTile;
	M Ridge = M::RoofTile;
	M Plinth = M::StoneDry;
	M Floor = M::WoodDark;
	M Trim = M::WoodLacquerBlack;
	M Noren = M::FabricIndigo;
	float PlinthHeight = 40.f;
	float RoofPitch = 26.f;
	float Overhang = 90.f;
	float RoofThickness = 16.f;
	bool bTileRibs = true;
	bool bRoundPosts = false;
};

bool ADBModularBuilding::bDeferRebuild = false;
FDBArtBatcher* ADBModularBuilding::SharedBatcher = nullptr;

ADBModularBuilding::ADBModularBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void ADBModularBuilding::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!bDeferRebuild)
	{
		Rebuild();
	}
}

void ADBModularBuilding::Configure(EDBBuildingType InType, int32 InSeed, FName InRegion, float InWealth, int32 InModulesX, int32 InModulesY,
	int32 InFloors, float InDamage)
{
	BuildingType = InType;
	Seed = InSeed;
	Region = InRegion;
	Wealth = InWealth;
	ModulesX = InModulesX;
	ModulesY = InModulesY;
	Floors = InFloors;
	Damage = InDamage;
	switch (BuildingType)
	{
	case EDBBuildingType::TempleHall:
		RoofFamily = EDBRoofFamily::Tiered;
		bVeranda = true;
		LanternDensity = 0.5f;
		break;
	case EDBBuildingType::LargeHouse:
	case EDBBuildingType::VillageHall:
		RoofFamily = EDBRoofFamily::Hip;
		bVeranda = BuildingType == EDBBuildingType::VillageHall || Wealth > 0.6f;
		LanternDensity = 0.4f;
		break;
	case EDBBuildingType::Guardhouse:
		RoofFamily = EDBRoofFamily::Hip;
		LanternDensity = 0.5f;
		break;
	case EDBBuildingType::Tavern:
		RoofFamily = EDBRoofFamily::Gable;
		LanternDensity = 1.f;
		break;
	case EDBBuildingType::MerchantHouse:
		RoofFamily = EDBRoofFamily::Gable;
		LanternDensity = 0.6f;
		break;
	case EDBBuildingType::Shrine:
		RoofFamily = EDBRoofFamily::Gable;
		LanternDensity = 0.f;
		break;
	default:
		RoofFamily = EDBRoofFamily::Gable;
		LanternDensity = 0.2f;
		break;
	}
}

ADBModularBuilding::FPalette ADBModularBuilding::MakePalette(FRandomStream& Random) const
{
	FPalette P;
	const bool bTempleStyle = BuildingType == EDBBuildingType::TempleHall || BuildingType == EDBBuildingType::Shrine || Region == TEXT("Temple");
	if (Region == TEXT("Capital"))
	{
		P.WallUpper = Wealth > 0.35f || Random.FRand() < 0.5f ? M::PlasterLime : M::PlasterClay;
		P.Plinth = M::StoneTemple;
		P.Overhang = 100.f;
	}
	else if (Region == TEXT("DarkBlood"))
	{
		P.Post = M::WoodBurnt;
		P.Beam = M::WoodBurnt;
		P.Plank = M::WoodBurnt;
		P.WallLower = M::WoodBurnt;
		P.Floor = M::WoodBurnt;
		P.Plinth = M::StoneCorrupted;
	}
	else
	{
		P.WallUpper = Random.FRand() < 0.75f ? M::PlasterClay : M::PlasterLime;
		P.Plinth = M::StoneMossy;
		if (Wealth < 0.45f && !bTempleStyle)
		{
			P.Roof = M::RoofThatch;
			P.Ridge = M::WoodDark;
			P.bTileRibs = false;
			P.RoofPitch = 38.f;
			P.RoofThickness = 45.f;
			P.Overhang = 80.f;
		}
	}
	if (bTempleStyle)
	{
		P.Post = M::WoodLacquerRed;
		P.Beam = M::WoodLacquerRed;
		P.WallUpper = M::PlasterLime;
		P.WallLower = M::WoodLacquerRed;
		P.Plinth = M::StoneTemple;
		P.Roof = Random.FRand() < 0.4f ? M::RoofCopper : M::RoofTile;
		P.Ridge = P.Roof;
		P.bTileRibs = P.Roof == M::RoofTile;
		P.RoofThickness = P.Roof == M::RoofCopper ? 10.f : 16.f;
		P.bRoundPosts = true;
		P.Overhang = 130.f;
		P.RoofPitch = 28.f;
	}
	switch (BuildingType)
	{
	case EDBBuildingType::TempleHall:
		P.PlinthHeight = 100.f;
		break;
	case EDBBuildingType::Shrine:
		P.PlinthHeight = 80.f;
		P.Overhang = 90.f;
		break;
	case EDBBuildingType::Warehouse:
		P.WallLower = M::WoodLacquerBlack;
		P.WallUpper = M::PlasterLime;
		P.PlinthHeight = 60.f;
		break;
	case EDBBuildingType::Tavern:
		P.Noren = M::FabricCrimson;
		P.PlinthHeight = 30.f;
		break;
	case EDBBuildingType::MerchantHouse:
		P.PlinthHeight = 30.f;
		break;
	default:
		P.PlinthHeight = 35.f + 25.f * Wealth;
		break;
	}
	if (bVeranda)
	{
		P.Overhang += 40.f;
	}
	return P;
}

void ADBModularBuilding::Clear()
{
	for (UInstancedStaticMeshComponent* Piece : Pieces)
	{
		if (Piece)
		{
			Piece->DestroyComponent();
		}
	}
	Pieces.Reset();
	for (UPointLightComponent* Light : Lights)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	Lights.Reset();
	InstanceCount = 0;
}

void ADBModularBuilding::AddLight(const FVector& LocalPosition, float Lumens, float Radius, const FLinearColor& Color)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
	Light->SetupAttachment(Root);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetRelativeLocation(LocalPosition);
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(Lumens);
	Light->SetAttenuationRadius(Radius);
	Light->SetLightColor(Color);
	Light->SetSourceRadius(15.f);
	Light->SetCastShadows(bLanternsCastShadows);
	// Small warm lights matter only up close: beyond 50 m they fade out and cost nothing (a city has hundreds of them).
	Light->MaxDrawDistance = 5000.f;
	Light->MaxDistanceFadeRange = 1500.f;
	Light->RegisterComponent();
	Lights.Add(Light);
}

void ADBModularBuilding::Rebuild()
{
	Clear();
	if (!Root)
	{
		return;
	}
	FRandomStream Random(Seed);
	const FPalette Palette = MakePalette(Random);
	FDBArtBatcher OwnBatcher(*this, *Root, Pieces);
	FDBArtBatcher& Batcher = SharedBatcher ? *SharedBatcher : OwnBatcher;
	const int32 InstancesBefore = Batcher.GetInstanceCount();
	Batcher.SetFrame(SharedBatcher ? GetActorTransform() : FTransform::Identity);
	const FTransform Identity = FTransform::Identity;

	const float W = ModulesX * ModuleSize;
	const float D = ModulesY * ModuleSize;
	const float P = Palette.PlinthHeight;
	const float FloorZ = P + 8.f;

	// Foundation: stone plinth and floor boards.
	Batcher.SetCollision(true);
	PutBox(Batcher, Palette.Plinth, Identity, FVector(0.f, 0.f, P * 0.5f), FVector(W + 70.f, D + 70.f, P));
	PutBox(Batcher, Palette.Floor, Identity, FVector(0.f, 0.f, P + 4.f), FVector(W + 16.f, D + 16.f, 8.f));

	for (int32 Floor = 0; Floor < Floors; ++Floor)
	{
		const float BaseZ = FloorZ + Floor * FloorHeight;
		BuildFloor(Batcher, Random, Palette, Floor, BaseZ);
		if (Floor > 0)
		{
			Batcher.SetCollision(true);
			PutBox(Batcher, Palette.Floor, Identity, FVector(0.f, 0.f, BaseZ - 5.f), FVector(W + 10.f, D + 10.f, 10.f));
		}
		if (Floor < Floors - 1)
		{
			BuildSkirtRoof(Batcher, Palette, BaseZ + FloorHeight - 25.f, 75.f, 22.f);
		}
	}
	const float WallTopZ = FloorZ + (Floors - 1) * FloorHeight + FloorHeight;
	if (RoofFamily == EDBRoofFamily::Tiered)
	{
		BuildSkirtRoof(Batcher, Palette, FloorZ + 190.f, Palette.Overhang - 20.f, 24.f);
	}
	BuildRoof(Batcher, Random, Palette, WallTopZ);

	// Veranda (engawa) along the front, stairs up to the entrance.
	float StairX = W * 0.5f + 35.f;
	float StairTopZ = P;
	if (bVeranda)
	{
		Batcher.SetCollision(true);
		const float DeckZ = P - 4.f;
		PutBox(Batcher, M::WoodLight, Identity, FVector(W * 0.5f + 70.f, 0.f, DeckZ), FVector(140.f, D + 40.f, 10.f));
		Batcher.SetCollision(false);
		for (int32 Index = 0; Index <= ModulesY; ++Index)
		{
			const float Y = -D * 0.5f - 20.f + Index * (D + 40.f) / ModulesY;
			PutBox(Batcher, Palette.Post, Identity, FVector(W * 0.5f + 132.f, Y, (DeckZ - 5.f) * 0.5f), FVector(12.f, 12.f, DeckZ - 5.f));
		}
		StairX = W * 0.5f + 140.f + 15.f;
		StairTopZ = DeckZ + 5.f;
	}
	Batcher.SetCollision(true);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(StairTopZ / 18.f));
	const float StairWidth = FMath::Min(ModuleSize * 1.3f, D * 0.6f);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const float Height = StairTopZ * (Step + 1) / Steps;
		const float X = StairX + (Steps - 1 - Step) * 30.f + 15.f;
		PutBox(Batcher, Palette.Plinth == M::StoneCorrupted ? M::StoneRuin : M::StoneTemple, Identity, FVector(X, 0.f, Height * 0.5f),
			FVector(30.f, StairWidth, Height));
	}

	BuildDressing(Batcher, Random, Palette, FloorZ);
	InstanceCount = Batcher.GetInstanceCount() - InstancesBefore;
	Batcher.SetFrame(FTransform::Identity);
}

void ADBModularBuilding::BuildFloor(FDBArtBatcher& Batcher, FRandomStream& Random, const FPalette& Palette, int32 Floor, float BaseZ)
{
	const float W = ModulesX * ModuleSize;
	const float D = ModulesY * ModuleSize;
	const FTransform Identity = FTransform::Identity;

	// Posts at every perimeter grid node.
	Batcher.SetCollision(true);
	for (int32 I = 0; I <= ModulesX; ++I)
	{
		for (int32 J = 0; J <= ModulesY; ++J)
		{
			if (I != 0 && I != ModulesX && J != 0 && J != ModulesY)
			{
				continue;
			}
			const FVector Center(-W * 0.5f + I * ModuleSize, -D * 0.5f + J * ModuleSize, BaseZ + FloorHeight * 0.5f);
			if (const FDBBuildingModule* Module = Kit ? Kit->PickModule(EDBBuildingModuleCategory::Pillar, Region, Wealth, Seed + I * 13 + J) : nullptr)
			{
				Batcher.Mesh(Module->Mesh.LoadSynchronous(), Module->Material.LoadSynchronous(), FTransform(FVector(Center.X, Center.Y, BaseZ)));
				continue;
			}
			if (Palette.bRoundPosts)
			{
				PutShape(Batcher, EShape::Cylinder, Palette.Post, Identity, Center, FVector(30.f, 30.f, FloorHeight));
			}
			else
			{
				PutBox(Batcher, Palette.Post, Identity, Center, FVector(PostSize, PostSize, FloorHeight));
			}
		}
	}

	// Beam rings: sill, rail and the top beam carrying the roof / upper floor.
	const float Rings[][2] = {{BaseZ + 7.f, 14.f}, {BaseZ + 92.f, 14.f}, {BaseZ + WallHeight + 15.f, 30.f}};
	for (const float* Ring : Rings)
	{
		PutBox(Batcher, Palette.Beam, Identity, FVector(W * 0.5f, 0.f, Ring[0]), FVector(24.f, D + 24.f, Ring[1]));
		PutBox(Batcher, Palette.Beam, Identity, FVector(-W * 0.5f, 0.f, Ring[0]), FVector(24.f, D + 24.f, Ring[1]));
		PutBox(Batcher, Palette.Beam, Identity, FVector(0.f, D * 0.5f, Ring[0]), FVector(W + 24.f, 24.f, Ring[1]));
		PutBox(Batcher, Palette.Beam, Identity, FVector(0.f, -D * 0.5f, Ring[0]), FVector(W + 24.f, 24.f, Ring[1]));
	}

	for (int32 Side = 0; Side < 4; ++Side)
	{
		const FTransform Frame = SideFrame(Side);
		const float A = Side < 2 ? W * 0.5f : D * 0.5f;
		const float Length = Side < 2 ? D : W;
		const int32 Bays = Side < 2 ? ModulesY : ModulesX;
		for (int32 Bay = 0; Bay < Bays; ++Bay)
		{
			const float Y = -Length * 0.5f + (Bay + 0.5f) * ModuleSize;
			const float L = ModuleSize - PostSize;
			const bool bFront = Side == 0;
			const bool bCenter = Bay == Bays / 2;

			// Choose the bay type from building type, floor, side and seed.
			EBay Type = Region == TEXT("Village") || Region == TEXT("DarkBlood") ? (Random.FRand() < 0.45f ? EBay::Plank : EBay::Plaster)
																				   : EBay::Plaster;
			if (!bFront && Random.FRand() < 0.3f)
			{
				Type = EBay::Window;
			}
			if (bFront)
			{
				Type = Wealth > 0.45f ? EBay::Shoji : (Random.FRand() < 0.5f ? EBay::Plank : EBay::Window);
			}
			switch (BuildingType)
			{
			case EDBBuildingType::Tavern:
			case EDBBuildingType::MerchantHouse:
				if (bFront && Floor == 0)
				{
					Type = (Bay == 0 || Bay == Bays - 1) && Bays > 2 ? EBay::Plank : EBay::ShopFront;
				}
				else if (bFront)
				{
					Type = EBay::Window;
				}
				break;
			case EDBBuildingType::Smithy:
				Type = bFront && Floor == 0 ? EBay::Open : EBay::Plank;
				break;
			case EDBBuildingType::Shrine:
				Type = bFront ? EBay::Open : EBay::Plank;
				break;
			case EDBBuildingType::TempleHall:
				Type = bFront ? EBay::Shoji : (Random.FRand() < 0.4f ? EBay::Window : EBay::Plaster);
				break;
			case EDBBuildingType::Warehouse:
				Type = !bFront && Bay == 0 && Floor == 0 ? EBay::Window : EBay::Plaster;
				break;
			case EDBBuildingType::Guardhouse:
				Type = bFront ? EBay::Plank : (Random.FRand() < 0.5f ? EBay::Window : EBay::Plank);
				break;
			default:
				break;
			}
			if (bFront && Floor == 0 && bCenter && Type != EBay::Open && Type != EBay::ShopFront)
			{
				Type = EBay::Door;
			}
			if (Damage > 0.f && Type != EBay::Door && Random.FRand() < Damage * 0.3f)
			{
				Type = EBay::Open; // broken wall panel
			}

			const float Top = WallHeight;
			const float LowerHeight = 90.f;
			Batcher.SetCollision(true);
			const EDBBuildingModuleCategory Category = Type == EBay::Door || Type == EBay::ShopFront ? EDBBuildingModuleCategory::Door
				: Type == EBay::Window ? EDBBuildingModuleCategory::Window : EDBBuildingModuleCategory::Wall;
			if (const FDBBuildingModule* Module = Kit && Type != EBay::Open ? Kit->PickModule(Category, Region, Wealth, Seed + Side * 101 + Bay * 7 + Floor) : nullptr)
			{
				// Authored module: pivot at the bottom center of the bay on the wall line, facing outwards.
				Batcher.Mesh(Module->Mesh.LoadSynchronous(), Module->Material.LoadSynchronous(), FTransform(FVector(A, Y, BaseZ)) * Frame);
				continue;
			}
			switch (Type)
			{
			case EBay::Plaster:
				PutBox(Batcher, Palette.WallLower, Frame, FVector(A, Y, BaseZ + LowerHeight * 0.5f), FVector(WallThickness, L, LowerHeight));
				PutBox(Batcher, Palette.WallUpper, Frame, FVector(A, Y, BaseZ + LowerHeight + (Top - LowerHeight) * 0.5f),
					FVector(WallThickness - 2.f, L, Top - LowerHeight));
				break;
			case EBay::Window:
			{
				PutBox(Batcher, Palette.WallLower, Frame, FVector(A, Y, BaseZ + LowerHeight * 0.5f), FVector(WallThickness, L, LowerHeight));
				PutBox(Batcher, Palette.WallUpper, Frame, FVector(A, Y, BaseZ + LowerHeight + (Top - LowerHeight) * 0.5f),
					FVector(WallThickness - 2.f, L, Top - LowerHeight));
				Batcher.SetCollision(false);
				const float WindowZ = BaseZ + 165.f;
				const float WindowWidth = L * 0.5f;
				PutBox(Batcher, M::Void, Frame, FVector(A + WallThickness * 0.5f, Y, WindowZ), FVector(1.f, WindowWidth, 70.f));
				for (int32 Bar = 0; Bar < 7; ++Bar)
				{
					const float BarY = Y - WindowWidth * 0.5f + (Bar + 0.5f) * WindowWidth / 7.f;
					PutBox(Batcher, Palette.Beam, Frame, FVector(A + WallThickness * 0.5f + 3.f, BarY, WindowZ), FVector(4.f, 3.f, 72.f));
				}
				PutBox(Batcher, Palette.Beam, Frame, FVector(A + WallThickness * 0.5f + 3.f, Y, WindowZ + 38.f), FVector(6.f, WindowWidth + 8.f, 6.f));
				PutBox(Batcher, Palette.Beam, Frame, FVector(A + WallThickness * 0.5f + 3.f, Y, WindowZ - 38.f), FVector(6.f, WindowWidth + 8.f, 6.f));
				break;
			}
			case EBay::Plank:
				PutBox(Batcher, Palette.Plank, Frame, FVector(A, Y, BaseZ + Top * 0.5f), FVector(WallThickness, L, Top));
				Batcher.SetCollision(false);
				for (float Batten = -L * 0.5f + 18.f; Batten < L * 0.5f - 10.f; Batten += 32.f)
				{
					PutBox(Batcher, Palette.Beam, Frame, FVector(A + WallThickness * 0.5f + 1.5f, Y + Batten, BaseZ + Top * 0.5f), FVector(3.f, 5.f, Top));
				}
				break;
			case EBay::Shoji:
			{
				PutBox(Batcher, Palette.WallLower, Frame, FVector(A, Y, BaseZ + 30.f), FVector(WallThickness, L, 60.f));
				PutBox(Batcher, Palette.Paper, Frame, FVector(A, Y, BaseZ + 60.f + (Top - 60.f) * 0.5f), FVector(4.f, L, Top - 60.f));
				Batcher.SetCollision(false);
				for (int32 Bar = 1; Bar < 4; ++Bar)
				{
					PutBox(Batcher, Palette.Beam, Frame, FVector(A + 3.5f, Y - L * 0.5f + Bar * L / 4.f, BaseZ + 60.f + (Top - 60.f) * 0.5f),
						FVector(3.f, 3.f, Top - 60.f));
				}
				for (int32 Bar = 1; Bar < 5; ++Bar)
				{
					PutBox(Batcher, Palette.Beam, Frame, FVector(A + 3.5f, Y, BaseZ + 60.f + Bar * (Top - 60.f) / 5.f), FVector(3.f, L, 3.f));
				}
				break;
			}
			case EBay::Door:
				// Opening with a half-open sliding door parked on the right: >1 m clear passage.
				PutBox(Batcher, Palette.WallUpper, Frame, FVector(A, Y, BaseZ + DoorHeight + (Top - DoorHeight) * 0.5f),
					FVector(WallThickness, L, Top - DoorHeight));
				PutBox(Batcher, Palette.Beam, Frame, FVector(A + 2.f, Y, BaseZ + DoorHeight), FVector(WallThickness + 6.f, L, 12.f));
				PutBox(Batcher, Wealth > 0.45f ? Palette.Paper : Palette.Plank, Frame, FVector(A - 10.f, Y + L * 0.25f, BaseZ + DoorHeight * 0.5f),
					FVector(5.f, L * 0.5f, DoorHeight));
				break;
			case EBay::ShopFront:
			{
				PutBox(Batcher, Palette.WallUpper, Frame, FVector(A, Y, BaseZ + DoorHeight + 20.f + (Top - DoorHeight - 20.f) * 0.5f),
					FVector(WallThickness, L, Top - DoorHeight - 20.f));
				Batcher.SetCollision(false);
				// Noren curtain in three strips.
				const float Strip = (L - 16.f) / 3.f;
				for (int32 Index = 0; Index < 3; ++Index)
				{
					PutBox(Batcher, Palette.Noren, Frame, FVector(A + 10.f, Y - L * 0.5f + 8.f + (Index + 0.5f) * Strip, BaseZ + DoorHeight - 15.f),
						FVector(2.f, Strip - 4.f, 70.f));
				}
				break;
			}
			case EBay::Open:
				break;
			}
		}
	}
}

void ADBModularBuilding::BuildSkirtRoof(FDBArtBatcher& Batcher, const FPalette& Palette, float Z, float Overhang, float PitchDegrees)
{
	const float W = ModulesX * ModuleSize;
	const float D = ModulesY * ModuleSize;
	const float Tan = FMath::Tan(FMath::DegreesToRadians(PitchDegrees));
	const float Cos = FMath::Cos(FMath::DegreesToRadians(PitchDegrees));
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const FTransform Frame = SideFrame(Side);
		const float A = Side < 2 ? W * 0.5f : D * 0.5f;
		const float Length = (Side < 2 ? D : W) + 2.f * Overhang;
		const float Run = Overhang + 10.f;
		const FTransform Slab = FTransform(FRotator(-PitchDegrees, 0.f, 0.f), FVector(A - 10.f + Run * 0.5f, 0.f, Z - Run * 0.5f * Tan)) * Frame;
		Batcher.SetCollision(true);
		PutBox(Batcher, Palette.Roof, Slab, FVector::ZeroVector, FVector(Run / Cos, Length, 10.f));
		if (Palette.bTileRibs)
		{
			Batcher.SetCollision(false);
			for (float V = -Length * 0.5f + 15.f; V < Length * 0.5f; V += 30.f)
			{
				PutShape(Batcher, EShape::Cylinder, Palette.Roof, Slab, FVector(0.f, V, 7.f), FVector(11.f, 11.f, Run / Cos), FRotator(90.f, 0.f, 0.f));
			}
		}
	}
}

void ADBModularBuilding::BuildRoof(FDBArtBatcher& Batcher, FRandomStream& Random, const FPalette& Palette, float WallTopZ)
{
	const float W = ModulesX * ModuleSize;
	const float D = ModulesY * ModuleSize;
	const bool bRidgeAlongY = BuildingType == EDBBuildingType::Shrine ? false : D >= W;
	const int32 SlopeSides[2] = {bRidgeAlongY ? 0 : 2, bRidgeAlongY ? 1 : 3};
	const int32 EndSides[2] = {bRidgeAlongY ? 2 : 0, bRidgeAlongY ? 3 : 1};
	const float SlopeHalf = bRidgeAlongY ? W * 0.5f : D * 0.5f; // wall line to ridge (horizontal)
	const float EndHalf = bRidgeAlongY ? D * 0.5f : W * 0.5f;
	const float Pitch = Palette.RoofPitch;
	const float Tan = FMath::Tan(FMath::DegreesToRadians(Pitch));
	const float Cos = FMath::Cos(FMath::DegreesToRadians(Pitch));
	const float O = Palette.Overhang;
	const float OEnd = O * 0.75f;
	const float T = Palette.RoofThickness;
	const float WallZ = WallTopZ + 5.f;
	const float RidgeZ = WallZ + SlopeHalf * Tan;
	const bool bHip = RoofFamily != EDBRoofFamily::Gable;
	const float SlabWidth = 2.f * (EndHalf + OEnd);

	auto Slab = [&](int32 Side, float InnerX, float OuterX, float Width, bool bRibs)
	{
		const float Run = OuterX - InnerX;
		const float CenterZ = RidgeZ - Run * 0.5f * Tan; // slabs start at the ridge height
		const float Tilt = Damage > 0.f ? Random.FRandRange(-4.f, 4.f) * Damage : 0.f;
		const FTransform SlabFrame = FTransform(FRotator(-Pitch + Tilt, 0.f, 0.f), FVector(InnerX + Run * 0.5f, 0.f, CenterZ)) * SideFrame(Side);
		Batcher.SetCollision(true);
		PutBox(Batcher, Palette.Roof, SlabFrame, FVector::ZeroVector, FVector(Run / Cos, Width, T));
		Batcher.SetCollision(false);
		// Eave fascia board.
		PutBox(Batcher, Palette.Beam, SlabFrame, FVector(Run / Cos * 0.5f - 4.f, 0.f, -T * 0.5f - 6.f), FVector(8.f, Width, 16.f));
		if (bRibs)
		{
			for (float V = -Width * 0.5f + 15.f; V < Width * 0.5f; V += 30.f)
			{
				PutShape(Batcher, EShape::Cylinder, Palette.Roof, SlabFrame, FVector(0.f, V, T * 0.5f + 3.f), FVector(12.f, 12.f, Run / Cos),
					FRotator(90.f, 0.f, 0.f));
			}
		}
	};

	// Main slopes: ridge (x = 0 in the side frame) down past the wall line to the eave.
	for (const int32 Side : SlopeSides)
	{
		Slab(Side, 0.f, SlopeHalf + O, SlabWidth, Palette.bTileRibs);
	}

	float RidgeLength = SlabWidth;
	if (bHip)
	{
		// Hip ends rise at the same pitch from the end walls to the ends of a shorter ridge.
		const float RidgeHalf = FMath::Max(EndHalf - SlopeHalf, 25.f);
		RidgeLength = 2.f * RidgeHalf;
		for (const int32 Side : EndSides)
		{
			const float Inner = RidgeHalf;
			const float Run = EndHalf + OEnd - Inner;
			const float CenterZ = RidgeZ - Run * 0.5f * Tan;
			const FTransform SlabFrame = FTransform(FRotator(-Pitch, 0.f, 0.f), FVector(Inner + Run * 0.5f, 0.f, CenterZ)) * SideFrame(Side);
			Batcher.SetCollision(true);
			PutBox(Batcher, Palette.Roof, SlabFrame, FVector::ZeroVector, FVector(Run / Cos, SlopeHalf + O, T));
			Batcher.SetCollision(false);
			PutBox(Batcher, Palette.Beam, SlabFrame, FVector(Run / Cos * 0.5f - 4.f, 0.f, -T * 0.5f - 6.f), FVector(8.f, SlopeHalf + O, 16.f));
			// Hip ridges running down to the corners.
			for (const float Sign : {-1.f, 1.f})
			{
				const FVector Start(Inner, 0.f, RidgeZ + T * 0.5f);
				const FVector End(EndHalf + OEnd, Sign * (SlopeHalf + O), WallZ - O * Tan + T * 0.5f);
				const FVector Mid = (Start + End) * 0.5f;
				const FVector Dir = End - Start;
				PutShape(Batcher, EShape::Cube, Palette.Ridge, SideFrame(Side), Mid, FVector(Dir.Size(), 22.f, 20.f), Dir.Rotation());
			}
		}
	}
	else
	{
		// Gable ends: stepped infill under the roof and barge boards.
		const float Rise = RidgeZ - WallZ;
		for (const int32 Side : EndSides)
		{
			const FTransform Frame = SideFrame(Side);
			Batcher.SetCollision(true);
			for (int32 Step = 0; Step < 4; ++Step)
			{
				const float Width = 2.f * SlopeHalf * (1.f - (Step + 0.5f) / 4.f);
				PutBox(Batcher, Palette.WallUpper, Frame, FVector(EndHalf, 0.f, WallZ + (Step + 0.5f) * Rise / 4.f), FVector(WallThickness, Width, Rise / 4.f));
			}
			Batcher.SetCollision(false);
			PutBox(Batcher, Palette.Beam, Frame, FVector(EndHalf + 4.f, 0.f, WallZ + Rise * 0.5f), FVector(10.f, 16.f, Rise));
			for (const float Sign : {-1.f, 1.f})
			{
				const FVector Start(EndHalf + OEnd, 0.f, RidgeZ + T * 0.5f);
				const FVector End(EndHalf + OEnd, Sign * (SlopeHalf + O), WallZ - O * Tan);
				const FVector Dir = End - Start;
				PutShape(Batcher, EShape::Cube, Palette.Trim, Frame, (Start + End) * 0.5f, FVector(Dir.Size(), 8.f, 26.f), Dir.Rotation());
			}
		}
	}

	// Ridge with end ornaments.
	Batcher.SetCollision(false);
	const FVector RidgeSize = bRidgeAlongY ? FVector(34.f, RidgeLength, 30.f) : FVector(RidgeLength, 34.f, 30.f);
	PutBox(Batcher, Palette.Ridge, FTransform::Identity, FVector(0.f, 0.f, RidgeZ + T * 0.5f + 10.f), RidgeSize);
	for (const float Sign : {-1.f, 1.f})
	{
		if (Palette.Roof != M::RoofTile)
		{
			break;
		}
		const FVector End = bRidgeAlongY ? FVector(0.f, Sign * RidgeLength * 0.5f, RidgeZ + T * 0.5f + 26.f)
										 : FVector(Sign * RidgeLength * 0.5f, 0.f, RidgeZ + T * 0.5f + 26.f);
		PutBox(Batcher, M::RoofTile, FTransform::Identity, End, bRidgeAlongY ? FVector(36.f, 20.f, 34.f) : FVector(20.f, 36.f, 34.f));
	}
}

void ADBModularBuilding::BuildDressing(FDBArtBatcher& Batcher, FRandomStream& Random, const FPalette& Palette, float FloorZ)
{
	const float W = ModulesX * ModuleSize;
	const float D = ModulesY * ModuleSize;
	const FTransform Identity = FTransform::Identity;
	const float EaveZ = FloorZ + WallHeight - 40.f;
	const FLinearColor Warm(1.f, 0.6f, 0.32f);

	// Hanging paper lanterns under the front eave.
	Batcher.SetCollision(false);
	TArray<float> LanternY;
	if (LanternDensity >= 0.75f)
	{
		for (int32 Bay = 0; Bay <= ModulesY; ++Bay)
		{
			LanternY.Add(-D * 0.5f + Bay * ModuleSize);
		}
	}
	else if (LanternDensity > 0.f)
	{
		const float Half = ModuleSize * 0.5f + 20.f;
		LanternY = {-Half, Half};
	}
	const float LanternX = W * 0.5f + (bVeranda ? 110.f : 45.f);
	for (const float Y : LanternY)
	{
		PutShape(Batcher, EShape::Sphere, M::LanternPaper, Identity, FVector(LanternX, Y, EaveZ), FVector(36.f, 36.f, 50.f));
		PutShape(Batcher, EShape::Cylinder, M::WoodLacquerBlack, Identity, FVector(LanternX, Y, EaveZ + 27.f), FVector(24.f, 24.f, 6.f));
		PutShape(Batcher, EShape::Cylinder, M::WoodLacquerBlack, Identity, FVector(LanternX, Y, EaveZ - 27.f), FVector(24.f, 24.f, 6.f));
		PutBox(Batcher, M::WoodLacquerBlack, Identity, FVector(LanternX, Y, EaveZ + 45.f), FVector(1.5f, 1.5f, 30.f));
	}
	if (LanternY.Num() > 0)
	{
		AddLight(FVector(LanternX + 30.f, 0.f, EaveZ - 20.f), 1600.f + 400.f * LanternY.Num(), 1100.f, Warm);
	}

	switch (BuildingType)
	{
	case EDBBuildingType::Tavern:
	{
		// Interior: tables with benches, counter, sake barrels, warm light.
		Batcher.SetCollision(true);
		for (int32 Index = 0; Index < FMath::Max(1, ModulesY - 1); ++Index)
		{
			const float Y = -D * 0.5f + ModuleSize * (Index + 1);
			const FVector Table(W * 0.15f, Y, FloorZ);
			PutBox(Batcher, M::WoodLight, Identity, Table + FVector(0.f, 0.f, 70.f), FVector(90.f, 150.f, 8.f));
			PutBox(Batcher, M::WoodDark, Identity, Table + FVector(0.f, 0.f, 33.f), FVector(60.f, 110.f, 66.f));
			PutBox(Batcher, M::WoodDark, Identity, Table + FVector(75.f, 0.f, 22.f), FVector(30.f, 140.f, 44.f));
			PutBox(Batcher, M::WoodDark, Identity, Table + FVector(-75.f, 0.f, 22.f), FVector(30.f, 140.f, 44.f));
		}
		PutBox(Batcher, M::WoodLight, Identity, FVector(-W * 0.5f + 70.f, 0.f, FloorZ + 55.f), FVector(60.f, D * 0.6f, 110.f));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			PutShape(Batcher, EShape::Cylinder, M::WoodLight, Identity, FVector(W * 0.5f + 60.f, D * 0.5f - 50.f - Index * 55.f, FloorZ - 10.f + 45.f),
				FVector(50.f, 50.f, 90.f));
		}
		Batcher.SetCollision(false);
		for (int32 Index = 0; Index < 2; ++Index)
		{
			PutShape(Batcher, EShape::Sphere, M::LanternPaper, Identity, FVector(0.f, (Index - 0.5f) * D * 0.4f, FloorZ + 220.f), FVector(34.f, 34.f, 46.f));
		}
		AddLight(FVector(0.f, 0.f, FloorZ + 200.f), 3000.f, 900.f, Warm);
		break;
	}
	case EDBBuildingType::Smithy:
	{
		Batcher.SetCollision(true);
		const FVector Hearth(-W * 0.5f + 80.f, 0.f, FloorZ);
		PutBox(Batcher, M::StoneRuin, Identity, Hearth + FVector(0.f, 0.f, 50.f), FVector(120.f, 160.f, 100.f));
		PutBox(Batcher, M::MetalIron, Identity, FVector(W * 0.1f, D * 0.15f, FloorZ + 35.f), FVector(70.f, 30.f, 70.f));
		Batcher.SetCollision(false);
		PutBox(Batcher, M::LanternFire, Identity, Hearth + FVector(10.f, 0.f, 102.f), FVector(80.f, 110.f, 6.f));
		// Chimney through the roof.
		PutBox(Batcher, M::StoneRuin, Identity, Hearth + FVector(0.f, 0.f, 100.f + (Floors * FloorHeight + 180.f) * 0.5f),
			FVector(70.f, 70.f, Floors * FloorHeight + 180.f));
		AddLight(Hearth + FVector(60.f, 0.f, 140.f), 4000.f, 900.f, FLinearColor(1.f, 0.35f, 0.1f));
		break;
	}
	case EDBBuildingType::MerchantHouse:
	{
		Batcher.SetCollision(true);
		for (int32 Index = 0; Index < ModulesY; ++Index)
		{
			const float Y = -D * 0.5f + (Index + 0.5f) * ModuleSize;
			PutBox(Batcher, M::WoodLight, Identity, FVector(W * 0.5f - 60.f, Y, FloorZ + 40.f), FVector(60.f, ModuleSize * 0.6f, 80.f));
			if (Random.FRand() < 0.6f)
			{
				PutBox(Batcher, M::FabricLinen, Identity, FVector(W * 0.5f - 60.f, Y, FloorZ + 95.f), FVector(50.f, ModuleSize * 0.4f, 30.f));
			}
		}
		break;
	}
	case EDBBuildingType::Shrine:
	case EDBBuildingType::TempleHall:
	{
		Batcher.SetCollision(false);
		// Shimenawa rope with paper streamers across the front beam, offering box at the entrance.
		const float RopeZ = FloorZ + WallHeight - 5.f;
		PutShape(Batcher, EShape::Cylinder, M::FabricLinen, Identity, FVector(W * 0.5f + 16.f, 0.f, RopeZ), FVector(16.f, 16.f, D * 0.8f), FRotator(0.f, 0.f, 90.f));
		for (int32 Index = -2; Index <= 2; ++Index)
		{
			PutBox(Batcher, M::PaperShoji, Identity, FVector(W * 0.5f + 17.f, Index * D * 0.16f, RopeZ - 30.f), FVector(1.f, 10.f, 40.f));
		}
		Batcher.SetCollision(true);
		PutBox(Batcher, M::WoodDark, Identity, FVector(W * 0.5f - 50.f, 0.f, FloorZ + 30.f), FVector(50.f, 110.f, 60.f));
		break;
	}
	case EDBBuildingType::SmallHouse:
	case EDBBuildingType::LargeHouse:
	case EDBBuildingType::VillageHall:
		if (Random.FRand() < 0.6f)
		{
			// Firewood stack along a side wall.
			Batcher.SetCollision(true);
			for (int32 Row = 0; Row < 3; ++Row)
			{
				for (int32 Log = 0; Log < 6 - Row; ++Log)
				{
					PutShape(Batcher, EShape::Cylinder, M::WoodDark, Identity, FVector(-W * 0.25f + Log * 22.f + Row * 11.f, D * 0.5f + 60.f, 11.f + Row * 20.f),
						FVector(20.f, 20.f, 90.f), FRotator(0.f, 0.f, 90.f));
				}
			}
		}
		break;
	default:
		break;
	}
}

FBox ADBModularBuilding::GetFootprintBounds() const
{
	const float W = ModulesX * ModuleSize;
	const float D = ModulesY * ModuleSize;
	const float Overhang = 150.f;
	const FBox Local(FVector(-W * 0.5f - Overhang, -D * 0.5f - Overhang, 0.f), FVector(W * 0.5f + Overhang + 250.f, D * 0.5f + Overhang, Floors * FloorHeight + 600.f));
	return Local.TransformBy(GetActorTransform());
}
