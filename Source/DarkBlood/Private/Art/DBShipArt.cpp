#include "Art/DBShipArt.h"

#include "Art/DBArtBatcher.h"

namespace
{
	using M = EDBArtMaterial;

	/** A round spar or rope from A to B. */
	void Line(FDBArtBatcher& Batcher, M Material, const FVector& A, const FVector& B, float Diameter)
	{
		const FVector Direction = B - A;
		const float Length = Direction.Size();
		if (Length > 1.f)
		{
			Batcher.Cylinder(Material, (A + B) * 0.5f, Diameter, Length, FRotationMatrix::MakeFromZ(Direction / Length).Rotator());
		}
	}

	/** Hull of a wasen (Japanese plank boat): flat bottom, flared sides with black wales, rising prow, square stern. */
	void Hull(FDBArtBatcher& Batcher, float Length, float Beam, float DeckZ, bool bCollision)
	{
		const float HalfL = Length * 0.5f;
		const float HalfB = Beam * 0.5f;
		// The straight midbody ends at BowStart; from there two planked sides close to a pointed, rising bow.
		const float BowStart = HalfL - Length * 0.28f;
		const float BodyCenter = (BowStart - HalfL) * 0.5f;
		const float BodyLength = BowStart + HalfL;
		Batcher.SetCollision(bCollision);
		Batcher.Box(M::WoodDark, FVector(BodyCenter, 0.f, -40.f), FVector(BodyLength, Beam * 0.7f, 160.f));
		Batcher.Box(M::WoodDark, FVector(BodyCenter, 0.f, DeckZ * 0.45f), FVector(BodyLength, Beam * 0.9f, DeckZ * 0.8f));
		Batcher.Box(M::WoodLight, FVector(BodyCenter, 0.f, DeckZ - 10.f), FVector(BodyLength, Beam * 0.92f, 20.f));
		// Low square stern (transom) and a raised stern deck.
		Batcher.Box(M::WoodDark, FVector(-HalfL + 40.f, 0.f, (DeckZ + 160.f) * 0.5f), FVector(90.f, Beam, DeckZ + 160.f));
		Batcher.Box(M::WoodLight, FVector(-HalfL + 250.f, 0.f, DeckZ + 140.f), FVector(420.f, Beam, 18.f));
		Batcher.SetCollision(false);
		const float BowLength = Length * 0.36f;
		const float BowAngle = FMath::RadiansToDegrees(FMath::Atan2(HalfB * 0.9f, BowLength));
		for (const float Side : {-1.f, 1.f})
		{
			// Flared side planking with two lacquered wales along the midbody.
			Batcher.Box(M::WoodWet, FVector(BodyCenter, Side * (HalfB - 10.f), DeckZ * 0.55f), FVector(BodyLength, 30.f, DeckZ * 0.95f), FRotator(0.f, 0.f, -Side * 8.f));
			for (const float Z : {DeckZ * 0.3f, DeckZ * 0.68f})
			{
				Batcher.Box(M::WoodLacquerBlack, FVector(BodyCenter, Side * (HalfB + 6.f), Z), FVector(BodyLength, 16.f, 26.f));
			}
			// Bow sides: closing towards the stem and sweeping up (the sheer of a wasen).
			const FVector BowMid(BowStart + BowLength * 0.5f, Side * HalfB * 0.45f, DeckZ * 0.6f);
			Batcher.Box(M::WoodWet, BowMid, FVector(BowLength * 1.04f, 34.f, DeckZ * 1.05f), FRotator(9.f, -Side * BowAngle, 0.f));
			Batcher.Box(M::WoodLacquerBlack, BowMid + FVector(0.f, Side * 20.f, DeckZ * 0.2f), FVector(BowLength * 1.04f, 14.f, 26.f), FRotator(9.f, -Side * BowAngle, 0.f));
			// Bulwark (kakita) with posts along the midbody.
			Batcher.Box(M::WoodDark, FVector(BodyCenter, Side * (HalfB - 5.f), DeckZ + 55.f), FVector(BodyLength * 0.92f, 18.f, 110.f));
			for (float X = -HalfL + 200.f; X < BowStart; X += 230.f)
			{
				Batcher.Box(M::WoodLacquerBlack, FVector(X, Side * (HalfB - 2.f), DeckZ + 60.f), FVector(18.f, 26.f, 125.f));
			}
		}
		// Bow deck between the closing sides, the stem post (misaki) leaning forward with its black cap.
		Batcher.Box(M::WoodLight, FVector(BowStart + BowLength * 0.35f, 0.f, DeckZ + 30.f), FVector(BowLength * 0.7f, Beam * 0.5f, 16.f), FRotator(9.f, 0.f, 0.f));
		Batcher.Box(M::WoodDark, FVector(BowStart + BowLength * 0.4f, 0.f, DeckZ * 0.35f), FVector(BowLength * 0.8f, Beam * 0.45f, DeckZ * 0.6f));
		const FVector StemFoot(BowStart + BowLength * 0.95f, 0.f, DeckZ * 0.2f);
		const FVector StemTop(BowStart + BowLength * 1.12f, 0.f, DeckZ + 330.f);
		Line(Batcher, M::WoodDark, StemFoot, StemTop, 70.f);
		Batcher.Box(M::WoodLacquerBlack, StemTop + FVector(0.f, 0.f, 20.f), FVector(80.f, 80.f, 40.f));
		// Big rudder and tiller at the stern.
		Batcher.Box(M::WoodDark, FVector(-HalfL - 60.f, 0.f, DeckZ * 0.3f), FVector(60.f, 50.f, DeckZ + 200.f), FRotator(-12.f, 0.f, 0.f));
		Line(Batcher, M::WoodDark, FVector(-HalfL - 20.f, 0.f, DeckZ + 190.f), FVector(-HalfL + 330.f, 0.f, DeckZ + 230.f), 18.f);
	}

	void Bezaisen(FDBArtBatcher& Batcher, FRandomStream& Random, bool bCollision)
	{
		const float Length = 2700.f;
		const float DeckZ = 380.f;
		Hull(Batcher, Length, 740.f, DeckZ, bCollision);

		// Stern cabin with shoji panels under a tiled roof.
		const float CabinX = -900.f;
		Batcher.Box(M::WoodLight, FVector(CabinX, 0.f, DeckZ + 130.f), FVector(480.f, 520.f, 260.f));
		for (const float Side : {-1.f, 1.f})
		{
			Batcher.Box(M::PaperShoji, FVector(CabinX, Side * 262.f, DeckZ + 140.f), FVector(380.f, 6.f, 160.f));
			Batcher.Box(M::RoofTile, FVector(CabinX, Side * 150.f, DeckZ + 310.f), FVector(600.f, 340.f, 22.f), FRotator(0.f, 0.f, Side * 28.f));
		}
		Batcher.Box(M::RoofTile, FVector(CabinX, 0.f, DeckZ + 365.f), FVector(620.f, 40.f, 40.f));

		// Single mast, top yard and boom, and the great square sail of vertical cloth strips with a crimson crest.
		const FVector MastFoot(150.f, 0.f, DeckZ);
		const float MastTop = DeckZ + 2100.f;
		Batcher.Box(M::WoodDark, MastFoot + FVector(0.f, 0.f, 40.f), FVector(120.f, 120.f, 80.f));
		Batcher.Cylinder(M::WoodLight, FVector(MastFoot.X, 0.f, (DeckZ + MastTop) * 0.5f), 50.f, MastTop - DeckZ);
		const float SailTop = MastTop - 60.f;
		const float SailBottom = DeckZ + 520.f;
		const float SailWidth = 1460.f;
		const float SailX = MastFoot.X + 60.f;
		Batcher.Cylinder(M::WoodDark, FVector(SailX - 15.f, 0.f, SailTop + 20.f), 28.f, SailWidth + 90.f, FRotator(0.f, 0.f, 90.f));
		Batcher.Cylinder(M::WoodDark, FVector(SailX - 15.f, 0.f, SailBottom - 20.f), 22.f, SailWidth, FRotator(0.f, 0.f, 90.f));
		const float SailMid = (SailTop + SailBottom) * 0.5f;
		Batcher.Box(M::FabricLinen, FVector(SailX, 0.f, SailMid), FVector(12.f, SailWidth, SailTop - SailBottom));
		Batcher.Box(M::FabricLinen, FVector(SailX + 30.f, 0.f, SailMid), FVector(12.f, SailWidth * 0.75f, (SailTop - SailBottom) * 0.86f)); // billow
		for (float Y = -SailWidth * 0.5f + 115.f; Y < SailWidth * 0.5f; Y += 115.f)
		{
			Batcher.Box(M::WoodWet, FVector(SailX + (FMath::Abs(Y) < SailWidth * 0.375f ? 38.f : 8.f), Y, SailMid), FVector(6.f, 6.f, SailTop - SailBottom));
		}
		Batcher.Cylinder(M::FabricCrimson, FVector(SailX + 42.f, 0.f, SailMid + 80.f), 520.f, 6.f, FRotator(90.f, 0.f, 0.f));

		// Stays to bow and stern, shrouds to the sides.
		const FVector Top(MastFoot.X, 0.f, MastTop - 30.f);
		Line(Batcher, M::WoodWet, Top, FVector(Length * 0.5f + Length * 0.18f, 0.f, DeckZ + 330.f), 7.f);
		Line(Batcher, M::WoodWet, Top, FVector(-Length * 0.5f + 60.f, 0.f, DeckZ + 190.f), 7.f);
		for (const float Side : {-1.f, 1.f})
		{
			for (const float X : {-200.f, 450.f})
			{
				Line(Batcher, M::WoodWet, Top, FVector(X, Side * 360.f, DeckZ + 140.f), 5.f);
			}
			// Paper lanterns at the stern and a crimson banner (nobori).
			Batcher.Sphere(M::LanternPaper, FVector(-Length * 0.5f + 60.f, Side * 300.f, DeckZ + 330.f), FVector(45.f, 45.f, 60.f));
		}
		Line(Batcher, M::WoodDark, FVector(-Length * 0.5f + 40.f, 0.f, DeckZ + 480.f), FVector(-Length * 0.5f + 40.f, 0.f, DeckZ + 980.f), 10.f);
		Batcher.Box(M::FabricCrimson, FVector(-Length * 0.5f + 40.f, -75.f, DeckZ + 840.f), FVector(4.f, 150.f, 260.f));

		// Cargo lashed on deck.
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const FVector At(Random.FRandRange(-500.f, 900.f), Random.FRandRange(-220.f, 220.f), DeckZ + 45.f);
			Batcher.Box(Random.FRand() < 0.5f ? M::WoodLight : M::FabricLinen, At, FVector(Random.FRandRange(90.f, 140.f), Random.FRandRange(80.f, 120.f), 90.f),
				FRotator(0.f, Random.FRandRange(-10.f, 10.f), 0.f));
		}
	}

	void Atakebune(FDBArtBatcher& Batcher, FRandomStream& Random, bool bCollision)
	{
		const float Length = 3600.f;
		const float Beam = 1050.f;
		const float DeckZ = 400.f;
		Hull(Batcher, Length, Beam, DeckZ, bCollision);

		// Armored box of black lacquered planks with arrow ports, a fighting deck on top.
		Batcher.SetCollision(bCollision);
		Batcher.Box(M::WoodLacquerBlack, FVector(-100.f, 0.f, DeckZ + 240.f), FVector(Length * 0.86f, Beam + 40.f, 480.f));
		Batcher.Box(M::WoodDark, FVector(-100.f, 0.f, DeckZ + 490.f), FVector(Length * 0.88f, Beam + 70.f, 20.f));
		Batcher.SetCollision(false);
		for (const float Side : {-1.f, 1.f})
		{
			for (float X = -Length * 0.4f; X < Length * 0.35f; X += 280.f)
			{
				Batcher.Box(M::WoodDark, FVector(X, Side * (Beam * 0.5f + 22.f), DeckZ + 300.f), FVector(90.f, 6.f, 45.f));
			}
			// Oars along the sides.
			for (float X = -Length * 0.38f; X < Length * 0.3f; X += 240.f)
			{
				Line(Batcher, M::WoodLight, FVector(X, Side * Beam * 0.5f, DeckZ + 60.f), FVector(X - 120.f, Side * (Beam * 0.5f + 650.f), -30.f), 16.f);
			}
		}

		// Castle (yagura): two storeys of white plaster under tiled roofs, bronze ridge ornaments.
		const float CastleX = -350.f;
		const float Floor1 = DeckZ + 500.f;
		Batcher.Box(M::PlasterLime, FVector(CastleX, 0.f, Floor1 + 190.f), FVector(1100.f, 700.f, 380.f));
		for (const float Side : {-1.f, 1.f})
		{
			Batcher.Box(M::PaperShojiLit, FVector(CastleX, Side * 352.f, Floor1 + 200.f), FVector(800.f, 6.f, 120.f));
			Batcher.Box(M::RoofTile, FVector(CastleX, Side * 265.f, Floor1 + 420.f), FVector(1320.f, 520.f, 24.f), FRotator(0.f, 0.f, Side * 25.f));
			Batcher.Box(M::RoofTile, FVector(CastleX, Side * 175.f, Floor1 + 830.f), FVector(880.f, 350.f, 24.f), FRotator(0.f, 0.f, Side * 28.f));
			Batcher.Box(M::MetalBronze, FVector(CastleX + Side * 440.f, 0.f, Floor1 + 930.f), FVector(60.f, 30.f, 70.f));
		}
		Batcher.Box(M::PlasterLime, FVector(CastleX, 0.f, Floor1 + 610.f), FVector(700.f, 460.f, 320.f));
		Batcher.Box(M::RoofTile, FVector(CastleX, 0.f, Floor1 + 905.f), FVector(900.f, 40.f, 40.f));

		// Mast with a furled sail, banners along the fighting deck.
		Batcher.Cylinder(M::WoodLight, FVector(900.f, 0.f, DeckZ + 1400.f), 45.f, 1800.f);
		Batcher.Cylinder(M::FabricLinen, FVector(940.f, 0.f, DeckZ + 1900.f), 70.f, 1200.f, FRotator(0.f, 0.f, 90.f));
		for (float X = -Length * 0.38f; X < Length * 0.35f; X += 520.f)
		{
			for (const float Side : {-1.f, 1.f})
			{
				const FVector Foot(X, Side * (Beam * 0.5f + 20.f), DeckZ + 500.f);
				Line(Batcher, M::WoodDark, Foot, Foot + FVector(0.f, 0.f, 520.f), 9.f);
				Batcher.Box(Random.FRand() < 0.7f ? M::FabricCrimson : M::FabricIndigo, Foot + FVector(0.f, -Side * 60.f, 380.f), FVector(4.f, 120.f, 240.f));
			}
		}
	}

	void Kobaya(FDBArtBatcher& Batcher, FRandomStream& Random, bool bCollision)
	{
		Batcher.SetCollision(bCollision);
		Batcher.Box(M::WoodWet, FVector(0.f, 0.f, 20.f), FVector(760.f, 200.f, 80.f));
		Batcher.SetCollision(false);
		for (const float Side : {-1.f, 1.f})
		{
			Batcher.Box(M::WoodWet, FVector(0.f, Side * 108.f, 55.f), FVector(800.f, 14.f, 70.f), FRotator(0.f, 0.f, -Side * 10.f));
			Batcher.Box(M::WoodLacquerBlack, FVector(0.f, Side * 116.f, 88.f), FVector(810.f, 10.f, 12.f));
		}
		Batcher.Box(M::WoodWet, FVector(430.f, 0.f, 75.f), FVector(180.f, 120.f, 60.f), FRotator(22.f, 0.f, 0.f));
		Batcher.Box(M::WoodDark, FVector(-390.f, 0.f, 60.f), FVector(30.f, 210.f, 100.f));
		for (const float X : {-150.f, 150.f})
		{
			Batcher.Box(M::WoodLight, FVector(X, 0.f, 70.f), FVector(40.f, 200.f, 10.f));
		}
		// A long sculling oar (ro) over the stern, nets or baskets in the boat.
		Line(Batcher, M::WoodLight, FVector(-300.f, 40.f, 90.f), FVector(-750.f, 90.f, -20.f), 12.f);
		if (Random.FRand() < 0.6f)
		{
			Batcher.Box(M::FabricLinen, FVector(Random.FRandRange(-100.f, 150.f), 0.f, 75.f), FVector(160.f, 120.f, 30.f));
		}
	}
}

void DBShipArt::Build(FDBArtBatcher& Batcher, EDBShipStyle Style, int32 Seed, bool bCollision)
{
	FRandomStream Random(Seed);
	switch (Style)
	{
	case EDBShipStyle::Atakebune: Atakebune(Batcher, Random, bCollision); break;
	case EDBShipStyle::Kobaya: Kobaya(Batcher, Random, bCollision); break;
	default: Bezaisen(Batcher, Random, bCollision); break;
	}
	Batcher.SetCollision(false);
}

void ADBShipModel::Build(FDBArtBatcher& Batcher)
{
	DBShipArt::Build(Batcher, Style, Seed);
}
