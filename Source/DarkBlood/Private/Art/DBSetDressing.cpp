#include "Art/DBSetDressing.h"

#include "Art/DBArtBatcher.h"
#include "Art/DBModularBuilding.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RandomStream.h"

using EShape = FDBArtBatcher::EShape;
using M = EDBArtMaterial;

namespace
{
	void Put(FDBArtBatcher& Batcher, EShape Shape, M Material, const FTransform& Parent, const FVector& Center, const FVector& Size,
		const FRotator& Rotation = FRotator::ZeroRotator)
	{
		Batcher.Shape(Shape, Material, FTransform(Rotation, Center, Size / 100.f) * Parent);
	}

	/** Box from A to B (X axis along the segment). */
	void Beam(FDBArtBatcher& Batcher, M Material, const FTransform& Parent, const FVector& A, const FVector& B, float Thickness, float Height)
	{
		const FVector Dir = B - A;
		Put(Batcher, EShape::Cube, Material, Parent, (A + B) * 0.5f, FVector(Dir.Size(), Thickness, Height), Dir.Rotation());
	}

	const FTransform Identity = FTransform::Identity;

	/** Authored mesh from a set, scaled so its largest horizontal extent is Size cm. False when the set is empty. */
	bool PlaceSetMesh(FDBArtBatcher& Batcher, EDBArtMeshSet Set, FRandomStream& Random, const FVector& Location, float Size, bool bCollision,
		const FTransform& Parent = FTransform::Identity)
	{
		UStaticMesh* Mesh = UDBArtMaterialSubsystem::PickMesh(Set, Random);
		if (!Mesh)
		{
			return false;
		}
		const FVector Extent = Mesh->GetBounds().BoxExtent;
		const float Scale = Size / FMath::Max(1.f, 2.f * FMath::Max(Extent.X, Extent.Y));
		Batcher.SetCollision(bCollision);
		Batcher.Mesh(Mesh, nullptr, FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Location, FVector(Scale)) * Parent);
		return true;
	}

	/** Authored mesh at its natural size times Scale. */
	bool PlaceSetMeshScaled(FDBArtBatcher& Batcher, EDBArtMeshSet Set, FRandomStream& Random, const FVector& Location, float Scale, bool bCollision)
	{
		UStaticMesh* Mesh = UDBArtMaterialSubsystem::PickMesh(Set, Random);
		if (!Mesh)
		{
			return false;
		}
		Batcher.SetCollision(bCollision);
		Batcher.Mesh(Mesh, nullptr, FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Location, FVector(Scale)));
		return true;
	}
}

// ---- Base ------------------------------------------------------------------------------------------

ADBArtActor::ADBArtActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void ADBArtActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ADBArtActor::Rebuild()
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
	FDBArtBatcher Batcher(*this, *Root, Pieces);
	Build(Batcher);
	InstanceCount = Batcher.GetInstanceCount();
}

void ADBArtActor::AddLight(const FVector& LocalPosition, float Lumens, float Radius, const FLinearColor& Color)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
	Light->SetupAttachment(Root);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetRelativeLocation(LocalPosition);
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(Lumens);
	Light->SetAttenuationRadius(Radius);
	Light->SetLightColor(Color);
	Light->SetSourceRadius(10.f);
	Light->SetCastShadows(bLightsCastShadows);
	Light->RegisterComponent();
	Lights.Add(Light);
}

// ---- Gate ------------------------------------------------------------------------------------------

void ADBGate::Build(FDBArtBatcher& Batcher)
{
	const float Half = Width * 0.5f;
	if (Style == EDBGateStyle::Torii)
	{
		Batcher.SetCollision(true);
		for (const float Sign : {-1.f, 1.f})
		{
			Put(Batcher, EShape::Cylinder, M::WoodLacquerBlack, Identity, FVector(0.f, Sign * Half, 30.f), FVector(62.f, 62.f, 60.f));
			Put(Batcher, EShape::Cylinder, M::WoodLacquerRed, Identity, FVector(0.f, Sign * Half, Height * 0.5f), FVector(44.f, 44.f, Height));
		}
		Batcher.SetCollision(false);
		// Nuki (tie beam), gakuzuka (center strut), shimaki + kasagi (top lintels, the kasagi curving up at the ends).
		Put(Batcher, EShape::Cube, M::WoodLacquerRed, Identity, FVector(0.f, 0.f, Height * 0.74f), FVector(24.f, Width + 130.f, 30.f));
		Put(Batcher, EShape::Cube, M::WoodLacquerRed, Identity, FVector(0.f, 0.f, Height * 0.74f + (Height * 0.26f - 20.f) * 0.5f + 15.f),
			FVector(20.f, 26.f, Height * 0.26f - 20.f));
		Put(Batcher, EShape::Cube, M::WoodLacquerBlack, Identity, FVector(1.f, 0.f, Height * 0.74f + 40.f), FVector(4.f, 60.f, 50.f));
		Put(Batcher, EShape::Cube, M::WoodLacquerRed, Identity, FVector(0.f, 0.f, Height + 14.f), FVector(34.f, Width + 200.f, 28.f));
		Put(Batcher, EShape::Cube, M::WoodLacquerBlack, Identity, FVector(0.f, 0.f, Height + 44.f), FVector(48.f, Width + 180.f, 32.f));
		for (const float Sign : {-1.f, 1.f})
		{
			const FVector Inner(0.f, Sign * (Width * 0.5f + 90.f), Height + 44.f);
			const FVector Outer(0.f, Sign * (Width * 0.5f + 150.f), Height + 66.f);
			Beam(Batcher, M::WoodLacquerBlack, Identity, Inner, Outer, 48.f, 32.f);
		}
		return;
	}

	// Roofed gate: two main posts, two rear posts, lintel, small gable roof, open door leaves.
	Batcher.SetCollision(true);
	for (const float Sign : {-1.f, 1.f})
	{
		Put(Batcher, EShape::Cube, M::StoneTemple, Identity, FVector(0.f, Sign * Half, 20.f), FVector(70.f, 70.f, 40.f));
		Put(Batcher, EShape::Cube, M::WoodDark, Identity, FVector(0.f, Sign * Half, Height * 0.5f), FVector(40.f, 40.f, Height));
		Put(Batcher, EShape::Cube, M::WoodDark, Identity, FVector(-140.f, Sign * Half, Height * 0.45f), FVector(26.f, 26.f, Height * 0.9f));
		// Door leaves swung open against the posts.
		Put(Batcher, EShape::Cube, M::WoodDark, Identity, FVector(-70.f, Sign * (Half - 20.f), Height * 0.42f), FVector(130.f, 10.f, Height * 0.8f));
	}
	Batcher.SetCollision(false);
	Put(Batcher, EShape::Cube, M::WoodDark, Identity, FVector(0.f, 0.f, Height - 20.f), FVector(44.f, Width + 120.f, 40.f));
	Put(Batcher, EShape::Cube, M::WoodDark, Identity, FVector(-70.f, 0.f, Height + 10.f), FVector(200.f, Width + 100.f, 20.f));
	const float Pitch = 30.f;
	const float Run = 170.f;
	const float Tan = FMath::Tan(FMath::DegreesToRadians(Pitch));
	const float RidgeZ = Height + 25.f + Run * 0.6f * Tan;
	for (const float Sign : {-1.f, 1.f})
	{
		const FTransform Slab = FTransform(FRotator(Sign > 0.f ? -Pitch : Pitch, 0.f, 0.f), FVector(-70.f + Sign * Run * 0.5f, 0.f, RidgeZ - Run * 0.5f * Tan));
		Batcher.SetCollision(true);
		Put(Batcher, EShape::Cube, M::RoofTile, Slab, FVector::ZeroVector, FVector(Run / FMath::Cos(FMath::DegreesToRadians(Pitch)), Width + 260.f, 14.f));
		Batcher.SetCollision(false);
		for (float V = -(Width + 260.f) * 0.5f + 15.f; V < (Width + 260.f) * 0.5f; V += 30.f)
		{
			Put(Batcher, EShape::Cylinder, M::RoofTile, Slab, FVector(0.f, V, 10.f), FVector(11.f, 11.f, Run * 1.15f), FRotator(90.f, 0.f, 0.f));
		}
	}
	Put(Batcher, EShape::Cube, M::RoofTile, Identity, FVector(-70.f, 0.f, RidgeZ + 16.f), FVector(30.f, Width + 260.f, 28.f));
}

// ---- Lantern -----------------------------------------------------------------------------------------

void ADBLantern::Build(FDBArtBatcher& Batcher)
{
	const FLinearColor Warm(1.f, 0.58f, 0.3f);
	if (Style == EDBLanternStyle::Stone)
	{
		Batcher.SetCollision(true);
		Put(Batcher, EShape::Cylinder, StoneMaterial, Identity, FVector(0.f, 0.f, 10.f), FVector(72.f, 72.f, 20.f));
		Put(Batcher, EShape::Cylinder, StoneMaterial, Identity, FVector(0.f, 0.f, 65.f), FVector(30.f, 30.f, 90.f));
		Put(Batcher, EShape::Cube, StoneMaterial, Identity, FVector(0.f, 0.f, 119.f), FVector(70.f, 70.f, 18.f));
		Put(Batcher, EShape::Cube, StoneMaterial, Identity, FVector(0.f, 0.f, 151.f), FVector(48.f, 48.f, 46.f));
		Batcher.SetCollision(false);
		for (const float Sign : {-1.f, 1.f})
		{
			Put(Batcher, EShape::Cube, bLit ? M::LanternFire : M::Void, Identity, FVector(Sign * 24.5f, 0.f, 151.f), FVector(1.f, 26.f, 24.f));
			Put(Batcher, EShape::Cube, bLit ? M::LanternFire : M::Void, Identity, FVector(0.f, Sign * 24.5f, 151.f), FVector(26.f, 1.f, 24.f));
		}
		Put(Batcher, EShape::Cone, StoneMaterial, Identity, FVector(0.f, 0.f, 194.f), FVector(100.f, 100.f, 42.f));
		Put(Batcher, EShape::Sphere, StoneMaterial, Identity, FVector(0.f, 0.f, 222.f), FVector(22.f, 22.f, 26.f));
		if (bLit)
		{
			AddLight(FVector(0.f, 0.f, 151.f), 700.f, 600.f, Warm);
		}
		return;
	}
	Batcher.SetCollision(true);
	Put(Batcher, EShape::Cube, M::WoodDark, Identity, FVector(0.f, 0.f, 110.f), FVector(12.f, 12.f, 220.f));
	Batcher.SetCollision(false);
	Put(Batcher, EShape::Cube, M::LanternPaper, Identity, FVector(0.f, 0.f, 245.f), FVector(34.f, 34.f, 48.f));
	Put(Batcher, EShape::Cube, M::WoodLacquerBlack, Identity, FVector(0.f, 0.f, 273.f), FVector(46.f, 46.f, 8.f));
	Put(Batcher, EShape::Cube, M::WoodLacquerBlack, Identity, FVector(0.f, 0.f, 222.f), FVector(40.f, 40.f, 5.f));
	if (bLit)
	{
		AddLight(FVector(0.f, 0.f, 245.f), 900.f, 750.f, Warm);
	}
}

// ---- Spline dressing ---------------------------------------------------------------------------------

ADBSplineDressing::ADBSplineDressing()
{
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(Root);
	Spline->SetMobility(EComponentMobility::Static);
}

float ADBSplineDressing::DistanceTo(const FVector& WorldLocation) const
{
	const FVector Closest = Spline->FindLocationClosestToWorldLocation(WorldLocation, ESplineCoordinateSpace::World);
	return FVector::Dist2D(Closest, WorldLocation);
}

void ADBSplineDressing::Build(FDBArtBatcher& Batcher)
{
	const float Length = Spline->GetSplineLength();
	if (Length < 10.f)
	{
		return;
	}
	FRandomStream Random(Seed);
	auto At = [&](float Distance) { return Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local); };
	auto RotAt = [&](float Distance)
	{
		FRotator Rotation = Spline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
		Rotation.Pitch = 0.f;
		Rotation.Roll = 0.f;
		return Rotation;
	};

	switch (Type)
	{
	case EDBSplineDressing::StonePath:
	{
		Batcher.SetCollision(false);
		for (float Distance = 40.f; Distance < Length; Distance += Random.FRandRange(75.f, 95.f))
		{
			const FRotator Rotation = RotAt(Distance);
			const FVector Side = Rotation.RotateVector(FVector(0.f, 1.f, 0.f));
			const FVector Center = At(Distance) + Side * Random.FRandRange(-0.18f, 0.18f) * Width + FVector(0.f, 0.f, 2.f);
			const float Size = Random.FRandRange(0.55f, 0.75f) * Width;
			Put(Batcher, EShape::Cylinder, M::StoneTemple, Identity, Center, FVector(Size, Size * Random.FRandRange(0.7f, 0.95f), 6.f),
				FRotator(0.f, Random.FRandRange(0.f, 180.f), 0.f));
		}
		break;
	}
	case EDBSplineDressing::Road:
	{
		Batcher.SetCollision(false);
		const float Step = 200.f;
		for (float Distance = 0.f; Distance < Length; Distance += Step)
		{
			const float Mid = FMath::Min(Distance + Step * 0.5f, Length);
			Put(Batcher, EShape::Cube, M::GroundEarth, Identity, At(Mid) + FVector(0.f, 0.f, 1.2f), FVector(Step * 1.08f, Width, 1.f), RotAt(Mid));
		}
		for (float Distance = 30.f; Distance < Length; Distance += Random.FRandRange(60.f, 140.f))
		{
			const FRotator Rotation = RotAt(Distance);
			const FVector Side = Rotation.RotateVector(FVector(0.f, 1.f, 0.f));
			const float Sign = Random.FRand() < 0.5f ? -1.f : 1.f;
			const FVector Center = At(Distance) + Side * Sign * (Width * 0.5f + Random.FRandRange(-10.f, 30.f));
			const float Size = Random.FRandRange(25.f, 60.f);
			if (!PlaceSetMesh(Batcher, EDBArtMeshSet::Rock, Random, Center, Size, false))
			{
				Put(Batcher, EShape::Sphere, M::StoneMossy, Identity, Center, FVector(Size, Size * 0.8f, Size * 0.45f), FRotator(0.f, Random.FRandRange(0.f, 180.f), 0.f));
			}
			if (Random.FRand() < 0.35f)
			{
				PlaceSetMesh(Batcher, Random.FRand() < 0.5f ? EDBArtMeshSet::Shrub : EDBArtMeshSet::Fern, Random,
					Center + Side * Sign * Random.FRandRange(40.f, 120.f), Random.FRandRange(60.f, 140.f), false);
			}
		}
		break;
	}
	case EDBSplineDressing::WoodFence:
	case EDBSplineDressing::BambooFence:
	{
		const bool bBamboo = Type == EDBSplineDressing::BambooFence;
		const float PostSpacing = 200.f;
		const float Height = bBamboo ? 180.f : 115.f;
		for (float Distance = 0.f; Distance <= Length + 1.f; Distance += PostSpacing)
		{
			const float D0 = FMath::Min(Distance, Length);
			const float D1 = FMath::Min(Distance + PostSpacing, Length);
			const FVector A = At(D0);
			Batcher.SetCollision(true);
			Put(Batcher, EShape::Cube, M::WoodDark, Identity, A + FVector(0.f, 0.f, Height * 0.5f), FVector(12.f, 12.f, Height), RotAt(D0));
			if (D1 - D0 < 5.f)
			{
				continue;
			}
			const FVector B = At(D1);
			for (const float RailZ : {Height * 0.35f, Height * 0.8f})
			{
				Beam(Batcher, bBamboo ? M::WoodLight : M::WoodDark, Identity, A + FVector(0.f, 0.f, RailZ), B + FVector(0.f, 0.f, RailZ), 6.f, 8.f);
			}
			if (bBamboo)
			{
				Batcher.SetCollision(false);
				const FVector Dir = (B - A).GetSafeNormal();
				for (float T = 8.f; T < (B - A).Size() - 4.f; T += 8.f)
				{
					Put(Batcher, EShape::Cylinder, M::WoodLight, Identity, A + Dir * T + FVector(0.f, 3.f, Height * 0.5f), FVector(6.f, 6.f, Height - Random.FRandRange(0.f, 12.f)));
				}
			}
		}
		break;
	}
	case EDBSplineDressing::CastleWall:
	{
		const float Segment = 300.f;
		const float Height = 260.f;
		for (float Distance = 0.f; Distance < Length; Distance += Segment)
		{
			const float D1 = FMath::Min(Distance + Segment, Length);
			const FVector A = At(Distance);
			const FVector B = At(D1);
			Batcher.SetCollision(true);
			Beam(Batcher, M::StoneTemple, Identity, A + FVector(0.f, 0.f, 35.f), B + FVector(0.f, 0.f, 35.f), 80.f, 70.f);
			Beam(Batcher, M::PlasterLime, Identity, A + FVector(0.f, 0.f, 70.f + (Height - 70.f) * 0.5f), B + FVector(0.f, 0.f, 70.f + (Height - 70.f) * 0.5f), 56.f,
				Height - 70.f);
			Batcher.SetCollision(false);
			// Tiled cap: two sloped slabs and a ridge.
			const FVector Dir = B - A;
			const FRotator Rotation = Dir.Rotation();
			const FTransform Frame(Rotation, (A + B) * 0.5f);
			for (const float Sign : {-1.f, 1.f})
			{
				Put(Batcher, EShape::Cube, M::RoofTile, Frame, FVector(0.f, Sign * 30.f, Height + 14.f), FVector(Dir.Size() + 6.f, 66.f, 8.f),
					FRotator(0.f, 0.f, Sign * 24.f));
			}
			Put(Batcher, EShape::Cube, M::RoofTile, Frame, FVector(0.f, 0.f, Height + 30.f), FVector(Dir.Size() + 6.f, 18.f, 16.f));
			Put(Batcher, EShape::Cube, M::WoodDark, Frame, FVector(0.f, 0.f, Height + 2.f), FVector(Dir.Size(), 62.f, 8.f));
		}
		break;
	}
	case EDBSplineDressing::Stream:
	{
		Batcher.SetCollision(false);
		const float Step = 150.f;
		for (float Distance = 0.f; Distance < Length; Distance += Step)
		{
			const float Mid = FMath::Min(Distance + Step * 0.5f, Length);
			Put(Batcher, EShape::Cube, M::Water, Identity, At(Mid) + FVector(0.f, 0.f, 3.f), FVector(Step * 1.15f, Width, 1.f), RotAt(Mid));
			Put(Batcher, EShape::Cube, M::StoneWet, Identity, At(Mid) + FVector(0.f, 0.f, 1.5f), FVector(Step * 1.15f, Width + 140.f, 1.f), RotAt(Mid));
		}
		for (float Distance = 20.f; Distance < Length; Distance += Random.FRandRange(35.f, 80.f))
		{
			const FRotator Rotation = RotAt(Distance);
			const FVector Side = Rotation.RotateVector(FVector(0.f, 1.f, 0.f));
			const float Sign = Random.FRand() < 0.5f ? -1.f : 1.f;
			const float Size = Random.FRandRange(30.f, 95.f);
			const FVector Center = At(Distance) + Side * Sign * (Width * 0.5f + Random.FRandRange(0.f, 60.f));
			if (!PlaceSetMesh(Batcher, EDBArtMeshSet::Rock, Random, Center, Size, false))
			{
				Put(Batcher, EShape::Sphere, Random.FRand() < 0.5f ? M::StoneWet : M::StoneMossy, Identity, Center, FVector(Size, Size * 0.85f, Size * 0.5f),
					FRotator(0.f, Random.FRandRange(0.f, 180.f), 0.f));
			}
			if (Random.FRand() < 0.3f)
			{
				PlaceSetMesh(Batcher, EDBArtMeshSet::Fern, Random, Center + Side * Sign * Random.FRandRange(50.f, 120.f), Random.FRandRange(70.f, 130.f), false);
			}
		}
		break;
	}
	}
}

// ---- Ground patch ------------------------------------------------------------------------------------

void ADBGroundPatch::Build(FDBArtBatcher& Batcher)
{
	Batcher.SetCollision(bCollision);
	if (bRound)
	{
		Put(Batcher, EShape::Cylinder, Material, Identity, FVector(0.f, 0.f, Layer), FVector(Size.X, Size.Y, 1.f));
		return;
	}
	if (TileSize < 20.f)
	{
		// Collision slabs are thick (top at the layer height) so nothing tunnels through them.
		const float Thickness = bCollision ? 100.f : 1.f;
		Put(Batcher, EShape::Cube, Material, Identity, FVector(0.f, 0.f, Layer - Thickness * 0.5f + 0.5f), FVector(Size.X, Size.Y, Thickness));
		return;
	}
	Batcher.SetCollision(false);
	// Paving slabs with dark joints; rows are offset like laid stone.
	FRandomStream Random(Seed);
	Put(Batcher, EShape::Cube, M::Void, Identity, FVector(0.f, 0.f, Layer), FVector(Size.X, Size.Y, 1.f));
	const int32 Rows = FMath::Max(1, FMath::FloorToInt(Size.Y / TileSize));
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float Y = -Size.Y * 0.5f + (Row + 0.5f) * TileSize;
		float X = -Size.X * 0.5f + (Row % 2) * TileSize * 0.5f;
		while (X < Size.X * 0.5f - 10.f)
		{
			const float Length = FMath::Min(TileSize * Random.FRandRange(0.9f, 1.6f), Size.X * 0.5f - X);
			Put(Batcher, EShape::Cube, Material, Identity, FVector(X + Length * 0.5f, Y, Layer + 1.f + Random.FRandRange(0.f, 0.6f)),
				FVector(Length - 3.f, TileSize - 3.f, 1.f));
			X += Length;
		}
	}
}

// ---- Bridge ------------------------------------------------------------------------------------------

void ADBBridge::Build(FDBArtBatcher& Batcher)
{
	const M Rail = bLacquered ? M::WoodLacquerRed : M::WoodDark;
	auto DeckZ = [&](float X) { const float T = 2.f * X / Length; return ArchHeight * (1.f - T * T); };
	const float Plank = 25.f;
	Batcher.SetCollision(true);
	for (float X = -Length * 0.5f; X < Length * 0.5f; X += Plank)
	{
		const float Mid = X + Plank * 0.5f;
		const float Slope = FMath::RadiansToDegrees(FMath::Atan((DeckZ(X + Plank) - DeckZ(X)) / Plank));
		Put(Batcher, EShape::Cube, M::WoodWet, Identity, FVector(Mid, 0.f, DeckZ(Mid) + 4.f), FVector(Plank - 1.5f, Width, 8.f), FRotator(Slope, 0.f, 0.f));
	}
	Batcher.SetCollision(false);
	for (const float Sign : {-1.f, 1.f})
	{
		const float Y = Sign * (Width * 0.5f - 6.f);
		const int32 Posts = FMath::Max(2, FMath::RoundToInt(Length / 150.f));
		FVector Previous;
		for (int32 Index = 0; Index <= Posts; ++Index)
		{
			const float X = -Length * 0.5f + Index * Length / Posts;
			const FVector Base(X, Y, DeckZ(X) + 8.f);
			Put(Batcher, EShape::Cube, Rail, Identity, Base + FVector(0.f, 0.f, 45.f), FVector(12.f, 12.f, 90.f));
			Put(Batcher, EShape::Sphere, M::MetalBronze, Identity, Base + FVector(0.f, 0.f, 98.f), FVector(16.f, 16.f, 20.f));
			const FVector Top = Base + FVector(0.f, 0.f, 82.f);
			if (Index > 0)
			{
				Beam(Batcher, Rail, Identity, Previous, Top, 10.f, 10.f);
				Beam(Batcher, Rail, Identity, Previous - FVector(0.f, 0.f, 45.f), Top - FVector(0.f, 0.f, 45.f), 6.f, 6.f);
			}
			Previous = Top;
		}
		// Stringer under the deck.
		Beam(Batcher, M::WoodDark, Identity, FVector(-Length * 0.5f, Y, -5.f), FVector(0.f, Y, ArchHeight - 12.f), 16.f, 24.f);
		Beam(Batcher, M::WoodDark, Identity, FVector(0.f, Y, ArchHeight - 12.f), FVector(Length * 0.5f, Y, -5.f), 16.f, 24.f);
	}
	for (const float X : {-Length * 0.2f, Length * 0.2f})
	{
		for (const float Sign : {-1.f, 1.f})
		{
			Put(Batcher, EShape::Cylinder, M::WoodDark, Identity, FVector(X, Sign * Width * 0.35f, DeckZ(X) * 0.5f - 20.f), FVector(22.f, 22.f, DeckZ(X) + 40.f));
		}
	}
}

// ---- Dungeon entrance --------------------------------------------------------------------------------

void ADBDungeonEntrance::Build(FDBArtBatcher& Batcher)
{
	FRandomStream Random(Seed);
	// Rock mound around the door (entrance faces +X).
	Batcher.SetCollision(true);
	for (int32 Index = 0; Index < 11; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(Random.FRandRange(-110.f, 110.f) + 180.f);
		const float Radius = Random.FRandRange(250.f, 520.f);
		const float Size = Random.FRandRange(320.f, 680.f);
		const FVector Center(FMath::Cos(Angle) * Radius - 120.f, FMath::Sin(Angle) * Radius, Size * 0.22f);
		if (!PlaceSetMesh(Batcher, EDBArtMeshSet::Boulder, Random, FVector(Center.X, Center.Y, -Size * 0.08f), Size * 1.1f, true))
		{
			Put(Batcher, EShape::Sphere, Index % 3 == 0 ? M::StoneMossy : M::StoneMountain, Identity, Center, FVector(Size, Size * 0.8f, Size * 0.62f),
				FRotator(Random.FRandRange(-8.f, 8.f), Random.FRandRange(0.f, 360.f), 0.f));
		}
	}
	// Stone door frame with a dark tunnel behind.
	for (const float Sign : {-1.f, 1.f})
	{
		Put(Batcher, EShape::Cube, M::StoneRuin, Identity, FVector(40.f, Sign * 160.f, 170.f), FVector(70.f, 60.f, 340.f));
	}
	Put(Batcher, EShape::Cube, M::StoneRuin, Identity, FVector(40.f, 0.f, 365.f), FVector(90.f, 440.f, 60.f));
	Batcher.SetCollision(false);
	Put(Batcher, EShape::Cube, M::Void, Identity, FVector(-60.f, 0.f, 170.f), FVector(200.f, 262.f, 340.f));
	// Corruption: dark soil, glowing cracks, dead wood.
	const float Spread = 450.f + Corruption * 450.f;
	Put(Batcher, EShape::Cylinder, Corruption > 0.3f ? M::DarkBloodSoil : M::GroundEarth, Identity, FVector(250.f, 0.f, 2.f), FVector(Spread * 2.f, Spread * 1.6f, 1.f));
	const int32 Cracks = FMath::RoundToInt(14.f * Corruption);
	for (int32 Index = 0; Index < Cracks; ++Index)
	{
		const FVector Start(Random.FRandRange(60.f, 200.f), Random.FRandRange(-120.f, 120.f), 3.5f);
		const float Yaw = Random.FRandRange(-70.f, 70.f);
		const float Len = Random.FRandRange(120.f, 380.f);
		Put(Batcher, EShape::Cube, M::DarkBloodVeins, Identity, Start + FRotator(0.f, Yaw, 0.f).Vector() * Len * 0.5f, FVector(Len, Random.FRandRange(4.f, 10.f), 1.f),
			FRotator(0.f, Yaw, 0.f));
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector Base(Random.FRandRange(200.f, 600.f), Random.FRandRange(-500.f, 500.f), 0.f);
		const float Height = Random.FRandRange(300.f, 520.f);
		if (PlaceSetMeshScaled(Batcher, EDBArtMeshSet::DeadTree, Random, Base, Random.FRandRange(0.9f, 1.3f), true))
		{
			continue;
		}
		Put(Batcher, EShape::Cylinder, M::WoodBurnt, Identity, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(30.f, 30.f, Height),
			FRotator(Random.FRandRange(-8.f, 8.f), 0.f, Random.FRandRange(-8.f, 8.f)));
		for (int32 Branch = 0; Branch < 3; ++Branch)
		{
			const FVector From = Base + FVector(0.f, 0.f, Height * Random.FRandRange(0.55f, 0.9f));
			const FVector To = From + FRotator(Random.FRandRange(20.f, 50.f), Random.FRandRange(0.f, 360.f), 0.f).Vector() * Random.FRandRange(90.f, 180.f);
			Beam(Batcher, M::WoodBurnt, Identity, From, To, 10.f, 10.f);
		}
	}
	if (Corruption > 0.f)
	{
		AddLight(FVector(-30.f, 0.f, 150.f), 2500.f + 3000.f * Corruption, 900.f, FLinearColor(1.f, 0.07f, 0.04f));
	}
}

// ---- Scatter volume ----------------------------------------------------------------------------------

ADBScatterVolume::ADBScatterVolume()
{
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->SetupAttachment(Root);
	Area->SetBoxExtent(FVector(1500.f, 1000.f, 400.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetMobility(EComponentMobility::Static);
}

bool ADBScatterVolume::IsExcluded(const FVector& World, float Clearance) const
{
	const UWorld* MyWorld = GetWorld();
	if (!MyWorld)
	{
		return false;
	}
	for (TActorIterator<ADBModularBuilding> It(MyWorld); It; ++It)
	{
		if (It->GetFootprintBounds().ExpandBy(BuildingClearance * Clearance).IsInsideXY(World))
		{
			return true;
		}
	}
	for (TActorIterator<ADBSplineDressing> It(MyWorld); It; ++It)
	{
		const float Needed = It->IsPathLike() ? It->Width * 0.5f + RoadClearance : 120.f;
		if (It->DistanceTo(World) < Needed * Clearance)
		{
			return true;
		}
	}
	for (TActorIterator<ADBArtActor> It(MyWorld); It; ++It)
	{
		// Only locally built, deterministic landmarks: server and clients must place the same trees
		// (trunks collide), so replicated gameplay actors are kept clear by the slice layout instead.
		const bool bLandmark = It->IsA<ADBDungeonEntrance>() || It->IsA<ADBGate>() || It->IsA<ADBBridge>() || It->IsA<ADBLantern>();
		if (bLandmark && FVector::Dist2D(It->GetActorLocation(), World) < GameplayClearance * Clearance)
		{
			return true;
		}
	}
	return false;
}

void ADBScatterVolume::PlaceDevPlant(FDBArtBatcher& Batcher, const FVector& Local, float Scale, FRandomStream& Random) const
{
	// Imported CC0 meshes for the biome first (Poly Haven set); cherry and bamboo have no CC0 model yet.
	switch (Biome)
	{
	case EDBBiome::TemperateForest:
	case EDBBiome::WetForest:
	case EDBBiome::MountainForest:
	case EDBBiome::Roadside:
		if (Biome != EDBBiome::WetForest && Random.FRand() < 0.08f && PlaceSetMeshScaled(Batcher, EDBArtMeshSet::DeadTree, Random, Local, Scale, true))
		{
			return;
		}
		if (PlaceSetMeshScaled(Batcher, EDBArtMeshSet::Tree, Random, Local, Scale * Random.FRandRange(0.9f, 1.35f), true))
		{
			return;
		}
		break;
	case EDBBiome::Corrupted:
		if (Random.FRand() < 0.6f && PlaceSetMeshScaled(Batcher, EDBArtMeshSet::DeadTree, Random, Local, Scale * 1.2f, true))
		{
			return;
		}
		break;
	case EDBBiome::ShrineGarden:
		if (Random.FRand() < 0.35f && PlaceSetMeshScaled(Batcher, EDBArtMeshSet::Tree, Random, Local, Scale, true))
		{
			return;
		}
		break;
	default:
		break;
	}

	// DEV stand-ins until CC0 / Fab vegetation meshes are assigned in Plants. Recognizable by biome, not final art.
	switch (Biome)
	{
	case EDBBiome::Bamboo:
	{
		Batcher.SetCollision(false);
		const int32 Culms = Random.RandRange(5, 9);
		for (int32 Index = 0; Index < Culms; ++Index)
		{
			const FVector Offset(Random.FRandRange(-60.f, 60.f), Random.FRandRange(-60.f, 60.f), 0.f);
			const float Height = Random.FRandRange(700.f, 1100.f) * Scale;
			Put(Batcher, EShape::Cylinder, M::FoliageLeaves, Identity, Local + Offset + FVector(0.f, 0.f, Height * 0.5f), FVector(9.f, 9.f, Height),
				FRotator(Random.FRandRange(-4.f, 4.f), 0.f, Random.FRandRange(-4.f, 4.f)));
		}
		return;
	}
	case EDBBiome::MountainForest:
	{
		Batcher.SetCollision(true);
		const float Height = Random.FRandRange(700.f, 1100.f) * Scale;
		Put(Batcher, EShape::Cylinder, M::BarkCedar, Identity, Local + FVector(0.f, 0.f, Height * 0.3f), FVector(40.f, 40.f, Height * 0.6f));
		Batcher.SetCollision(false);
		for (int32 Tier = 0; Tier < 4; ++Tier)
		{
			const float Width = (420.f - Tier * 85.f) * Scale;
			Put(Batcher, EShape::Cone, M::FoliageLeaves, Identity, Local + FVector(0.f, 0.f, Height * (0.3f + Tier * 0.17f) + Width * 0.3f),
				FVector(Width, Width, Width * 0.8f), FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f));
		}
		return;
	}
	case EDBBiome::Corrupted:
	{
		Batcher.SetCollision(true);
		const float Height = Random.FRandRange(350.f, 650.f) * Scale;
		Put(Batcher, EShape::Cylinder, M::WoodBurnt, Identity, Local + FVector(0.f, 0.f, Height * 0.5f), FVector(34.f, 34.f, Height),
			FRotator(Random.FRandRange(-6.f, 6.f), 0.f, Random.FRandRange(-6.f, 6.f)));
		Batcher.SetCollision(false);
		for (int32 Branch = 0; Branch < 4; ++Branch)
		{
			const FVector From = Local + FVector(0.f, 0.f, Height * Random.FRandRange(0.5f, 0.95f));
			const FVector To = From + FRotator(Random.FRandRange(15.f, 55.f), Random.FRandRange(0.f, 360.f), 0.f).Vector() * Random.FRandRange(100.f, 220.f);
			Beam(Batcher, M::WoodBurnt, Identity, From, To, 12.f, 12.f);
		}
		return;
	}
	default:
		break;
	}

	// Broadleaf / cherry: trunk, a few branches and clustered canopy volumes.
	const bool bCherry = Biome == EDBBiome::CherryGrove || (Biome == EDBBiome::ShrineGarden && Random.FRand() < 0.6f);
	const float Height = (bCherry ? Random.FRandRange(380.f, 560.f) : Random.FRandRange(650.f, 1000.f)) * Scale;
	const float Trunk = (bCherry ? 34.f : 50.f) * Scale;
	Batcher.SetCollision(true);
	Put(Batcher, EShape::Cylinder, bCherry ? M::BarkSakura : M::BarkCedar, Identity, Local + FVector(0.f, 0.f, Height * 0.5f), FVector(Trunk, Trunk, Height),
		FRotator(Random.FRandRange(-3.f, 3.f), 0.f, Random.FRandRange(-3.f, 3.f)));
	Batcher.SetCollision(false);
	const int32 Clusters = Random.RandRange(4, 7);
	for (int32 Index = 0; Index < Clusters; ++Index)
	{
		const float Radius = (bCherry ? Random.FRandRange(170.f, 280.f) : Random.FRandRange(220.f, 380.f)) * Scale;
		const FVector Offset(Random.FRandRange(-1.f, 1.f) * Radius * 0.7f, Random.FRandRange(-1.f, 1.f) * Radius * 0.7f, Random.FRandRange(-0.25f, 0.2f) * Height);
		const FVector Center = Local + FVector(0.f, 0.f, Height * (bCherry ? 0.92f : 0.85f)) + Offset;
		Put(Batcher, EShape::Sphere, bCherry ? M::FoliageSakura : M::FoliageLeaves, Identity, Center, FVector(Radius * 2.f, Radius * 1.8f, Radius * 1.3f),
			FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f));
		Beam(Batcher, bCherry ? M::BarkSakura : M::BarkCedar, Identity, Local + FVector(0.f, 0.f, Height * 0.6f), Center - FVector(0.f, 0.f, Radius * 0.3f),
			Trunk * 0.35f, Trunk * 0.35f);
	}
}

void ADBScatterVolume::PlaceDevUndergrowth(FDBArtBatcher& Batcher, const FVector& Local, float Scale, FRandomStream& Random) const
{
	Batcher.SetCollision(false);
	const float Roll = Random.FRand();
	{
		// Imported CC0 ground cover: shrubs, ferns, moss, rocks, boulders and stumps weighted by biome.
		const bool bCorrupted = Biome == EDBBiome::Corrupted;
		EDBArtMeshSet Set = EDBArtMeshSet::Shrub;
		float Size = Random.FRandRange(80.f, 160.f) * Scale;
		bool bCollision = false;
		if (bCorrupted)
		{
			Set = Roll < 0.7f ? EDBArtMeshSet::Rock : EDBArtMeshSet::Stump;
			Size = Random.FRandRange(40.f, 140.f) * Scale;
		}
		else if (Roll < 0.28f)
		{
			Set = EDBArtMeshSet::Fern;
		}
		else if (Roll < 0.48f)
		{
			Set = EDBArtMeshSet::Moss;
			Size = Random.FRandRange(60.f, 140.f) * Scale;
		}
		else if (Roll < 0.7f)
		{
			Set = EDBArtMeshSet::Rock;
			Size = Random.FRandRange(30.f, 110.f) * Scale;
			bCollision = Size > 90.f;
		}
		else if (Roll < 0.76f)
		{
			Set = EDBArtMeshSet::Boulder;
			Size = Random.FRandRange(150.f, 320.f) * Scale;
			bCollision = true;
		}
		else if (Roll < 0.8f)
		{
			Set = EDBArtMeshSet::Stump;
			Size = Random.FRandRange(60.f, 110.f) * Scale;
			bCollision = true;
		}
		if (PlaceSetMesh(Batcher, Set, Random, Local, Size, bCollision))
		{
			return;
		}
	}
	if (Roll < 0.35f)
	{
		const float Size = Random.FRandRange(40.f, 130.f) * Scale;
		Batcher.SetCollision(Size > 90.f);
		Put(Batcher, EShape::Sphere, Biome == EDBBiome::Corrupted ? M::StoneCorrupted : M::StoneMossy, Identity, Local + FVector(0.f, 0.f, Size * 0.15f),
			FVector(Size, Size * 0.8f, Size * 0.55f), FRotator(Random.FRandRange(-10.f, 10.f), Random.FRandRange(0.f, 360.f), 0.f));
		return;
	}
	const M Material = Biome == EDBBiome::Corrupted ? M::FoliageDead : M::FoliageLeaves;
	const float Size = Random.FRandRange(70.f, 150.f) * Scale;
	Put(Batcher, EShape::Sphere, Material, Identity, Local + FVector(0.f, 0.f, Size * 0.3f), FVector(Size, Size * 0.9f, Size * 0.7f),
		FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f));
}

void ADBScatterVolume::Build(FDBArtBatcher& Batcher)
{
	PlacedCount = 0;
	RejectedCount = 0;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FRandomStream Random(Seed);
	const FVector Extent = Area->GetScaledBoxExtent();
	const float AreaM2 = (Extent.X * 2.f / 100.f) * (Extent.Y * 2.f / 100.f);
	const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxSlopeDegrees));
	// Static and dynamic world geometry (the blockout floor is WorldDynamic), never pawns.
	FCollisionObjectQueryParams GroundTypes(ECC_WorldStatic);
	GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	auto Scatter = [&](int32 Count, float Clearance, TFunctionRef<void(const FVector&, float, const FVector&)> Place)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector LocalXY(Random.FRandRange(-Extent.X, Extent.X), Random.FRandRange(-Extent.Y, Extent.Y), 0.f);
			// Soft edges: keep probability falls off towards the border of the volume.
			const float Edge = FMath::Min(1.f - FMath::Abs(LocalXY.X) / Extent.X, 1.f - FMath::Abs(LocalXY.Y) / Extent.Y);
			if (EdgeFalloff > 0.f && Random.FRand() > FMath::Clamp(Edge / EdgeFalloff, 0.f, 1.f))
			{
				++RejectedCount;
				continue;
			}
			const FVector Start = GetActorTransform().TransformPosition(LocalXY + FVector(0.f, 0.f, Extent.Z));
			const FVector End = GetActorTransform().TransformPosition(LocalXY - FVector(0.f, 0.f, Extent.Z + 2000.f));
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(DBScatter), false, this);
			if (!World->LineTraceSingleByObjectType(Hit, Start, End, GroundTypes, Params) || Hit.ImpactNormal.Z < MinNormalZ
				|| IsExcluded(Hit.ImpactPoint, Clearance))
			{
				++RejectedCount;
				continue;
			}
			const FVector Local = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
			Place(Local, Random.FRandRange(0.8f, 1.25f), Hit.ImpactNormal);
			++PlacedCount;
		}
	};

	auto PlaceAuthored = [&](const TArray<FDBScatterEntry>& Entries, const FVector& Local, const FVector& Normal) -> bool
	{
		float Total = 0.f;
		for (const FDBScatterEntry& Entry : Entries)
		{
			Total += Entry.Weight;
		}
		float Pick = Random.FRand() * Total;
		for (const FDBScatterEntry& Entry : Entries)
		{
			Pick -= Entry.Weight;
			if (Pick > 0.f)
			{
				continue;
			}
			UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();
			if (!Mesh)
			{
				return false;
			}
			const FQuat Align = Entry.bAlignToSlope ? FQuat::FindBetweenNormals(FVector::UpVector, GetActorTransform().InverseTransformVector(Normal)) : FQuat::Identity;
			const FQuat Yaw(FVector::UpVector, Random.FRandRange(0.f, 2.f * PI));
			Batcher.SetCollision(Entry.bCollision);
			Batcher.Mesh(Mesh, nullptr, FTransform(Align * Yaw, Local, FVector(Random.FRandRange(Entry.ScaleRange.X, Entry.ScaleRange.Y))));
			return true;
		}
		return false;
	};

	const int32 PlantCount = FMath::RoundToInt(AreaM2 / 100.f * Density);
	Scatter(PlantCount, 1.f, [&](const FVector& Local, float Scale, const FVector& Normal)
	{
		if (Plants.Num() == 0 || !PlaceAuthored(Plants, Local, Normal))
		{
			PlaceDevPlant(Batcher, Local, Scale, Random);
		}
	});
	const int32 UndergrowthCount = FMath::RoundToInt(AreaM2 / 100.f * UndergrowthDensity);
	Scatter(UndergrowthCount, 0.5f, [&](const FVector& Local, float Scale, const FVector& Normal)
	{
		if (Undergrowth.Num() == 0 || !PlaceAuthored(Undergrowth, Local, Normal))
		{
			PlaceDevUndergrowth(Batcher, Local, Scale, Random);
		}
	});
}
