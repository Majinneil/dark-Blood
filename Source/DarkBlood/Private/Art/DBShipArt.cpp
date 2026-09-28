#include "Art/DBShipArt.h"

#include "Art/DBArtBatcher.h"
#include "Art/DBModelLibrary.h"

namespace
{
	using M = EDBArtMaterial;

	/** A round spar, rope or pole from A to B. */
	void Line(FDBArtBatcher& Batcher, M Material, const FVector& A, const FVector& B, float Diameter)
	{
		const FVector Direction = B - A;
		const float Length = Direction.Size();
		if (Length > 1.f && !Direction.ContainsNaN())
		{
			Batcher.Cylinder(Material, (A + B) * 0.5f, Diameter, Length, FRotationMatrix::MakeFromZ(Direction / Length).Rotator());
		}
	}

	/** A flat plate spanning the quad A0-A1 (one edge) to B0-B1 (the opposite edge), Thickness across. */
	void Plate(FDBArtBatcher& Batcher, M Material, const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1, float Thickness)
	{
		const FVector Along = ((B0 + B1) - (A0 + A1)) * 0.5f;
		const FVector Up = ((A1 + B1) - (A0 + B0)) * 0.5f;
		// Degenerate quads (a bow tip, edges almost parallel) are skipped: their frame would be undefined.
		if (Along.Size() < 1.f || Up.Size() < 1.f || FVector::CrossProduct(Along.GetSafeNormal(), Up.GetSafeNormal()).Size() < 0.05f)
		{
			return;
		}
		const FRotator Rotation = FRotationMatrix::MakeFromXZ(Along, Up).Rotator();
		if (Rotation.ContainsNaN())
		{
			return;
		}
		const FVector Center = (A0 + A1 + B0 + B1) * 0.25f;
		Batcher.Box(Material, Center, FVector(Along.Size() + 6.f, Thickness, Up.Size() + 4.f), Rotation);
	}

	/** Hull outline: stations along the keel (T = 0 stern .. 1 bow). */
	struct FHull
	{
		float Length = 3000.f;
		float Beam = 800.f;
		float DeckZ = 400.f;
		float Depth = 180.f;
		float BowRise = 250.f;
		float SternRise = 200.f;
		/** Width of the transom relative to the beam. */
		float SternWidth = 0.7f;
		int32 Stations = 18;
		int32 Strakes = 6;
		M Planks = M::WoodWet;
		M PlanksAlt = M::WoodDark;
		M Wale = M::WoodLacquerBlack;
		M Deck = M::WoodLight;

		float X(float T) const { return -Length * 0.5f + T * Length; }
		float HalfWidth(float T) const
		{
			if (T < 0.45f)
			{
				return Beam * 0.5f * FMath::Lerp(SternWidth, 1.f, FMath::Sin(T / 0.45f * HALF_PI));
			}
			const float U = (T - 0.45f) / 0.55f;
			return Beam * 0.5f * FMath::Max(0.03f, FMath::Pow(FMath::Max(0.f, FMath::Cos(U * HALF_PI)), 0.75f)); // cos(pi/2) is slightly negative in float
		}
		/** Top of the side planking: a sheer rising to bow and stern. */
		float Sheer(float T) const
		{
			return DeckZ + 80.f + SternRise * FMath::Square(FMath::Max(0.f, 0.3f - T) / 0.3f) + BowRise * FMath::Square(FMath::Max(0.f, T - 0.55f) / 0.45f);
		}
		float Bottom(float T) const
		{
			const float BowLift = 1.f - 0.85f * FMath::Square(FMath::Max(0.f, T - 0.7f) / 0.3f);
			const float SternLift = 1.f - 0.4f * FMath::Square(FMath::Max(0.f, 0.12f - T) / 0.12f);
			return -Depth * BowLift * SternLift;
		}
		/** A point on the planking: F = 0 at the bottom, 1 at the sheer; the bilge is rounded. */
		FVector Point(float T, float F, float Side) const
		{
			const float Z = FMath::Lerp(Bottom(T), Sheer(T), F);
			return FVector(X(T), Side * HalfWidth(T) * (0.55f + 0.45f * FMath::Sqrt(F)), Z);
		}
	};

	void BuildHull(FDBArtBatcher& Batcher, const FHull& Hull, bool bCollision)
	{
		const int32 N = Hull.Stations;
		// Decks, keel and transom carry the collision; the planking is visual.
		Batcher.SetCollision(bCollision);
		for (int32 Index = 0; Index < N; ++Index)
		{
			const float T0 = Index / float(N);
			const float T1 = (Index + 1) / float(N);
			const float Width = 2.f * FMath::Min(Hull.HalfWidth(T0), Hull.HalfWidth(T1)) * 0.94f;
			if (Width > 30.f)
			{
				Batcher.Box(Hull.Deck, FVector((Hull.X(T0) + Hull.X(T1)) * 0.5f, 0.f, Hull.DeckZ - 10.f), FVector(Hull.X(T1) - Hull.X(T0) + 4.f, Width, 20.f));
			}
		}
		const float Transom = Hull.HalfWidth(0.f);
		Batcher.Box(Hull.PlanksAlt, FVector(Hull.X(0.f) + 20.f, 0.f, (Hull.Bottom(0.f) + Hull.Sheer(0.f)) * 0.5f),
			FVector(40.f, Transom * 2.f, Hull.Sheer(0.f) - Hull.Bottom(0.f)));
		Batcher.SetCollision(false);

		for (const float Side : {-1.f, 1.f})
		{
			for (int32 Index = 0; Index < N; ++Index)
			{
				const float T0 = Index / float(N);
				const float T1 = (Index + 1) / float(N);
				for (int32 Strake = 0; Strake < Hull.Strakes; ++Strake)
				{
					const float F0 = Strake / float(Hull.Strakes);
					const float F1 = (Strake + 1) / float(Hull.Strakes);
					const M Material = Strake == Hull.Strakes - 2 ? Hull.Wale : (Strake % 2 == 0 ? Hull.Planks : Hull.PlanksAlt);
					Plate(Batcher, Material, Hull.Point(T0, F0, Side), Hull.Point(T0, F1, Side), Hull.Point(T1, F0, Side), Hull.Point(T1, F1, Side), 14.f);
				}
				// Gunwale rail and the bottom planking.
				Line(Batcher, Hull.Wale, Hull.Point(T0, 1.f, Side) + FVector(0.f, 0.f, 8.f), Hull.Point(T1, 1.f, Side) + FVector(0.f, 0.f, 8.f), 20.f);
				Plate(Batcher, Hull.PlanksAlt, Hull.Point(T0, 0.f, Side), Hull.Point(T0, 0.f, 0.f) * FVector(1.f, 0.f, 1.f), Hull.Point(T1, 0.f, Side),
					Hull.Point(T1, 0.f, 0.f) * FVector(1.f, 0.f, 1.f), 14.f);
			}
		}
		// Stem post rising out of the bow.
		const FVector StemFoot(Hull.X(1.f) - 40.f, 0.f, Hull.Bottom(1.f));
		const FVector StemTop(Hull.X(1.f) + Hull.Length * 0.03f, 0.f, Hull.Sheer(1.f) + 40.f);
		Line(Batcher, Hull.PlanksAlt, StemFoot, StemTop, FMath::Max(24.f, Hull.Beam * 0.07f));
		// Rudder at the stern.
		const float RudderTop = Hull.Sheer(0.f);
		Batcher.Box(Hull.PlanksAlt, FVector(Hull.X(0.f) - Hull.Length * 0.015f, 0.f, (RudderTop + Hull.Bottom(0.f) - 60.f) * 0.5f),
			FVector(Hull.Length * 0.012f + 20.f, FMath::Max(20.f, Hull.Beam * 0.04f), RudderTop - Hull.Bottom(0.f) + 60.f), FRotator(-10.f, 0.f, 0.f));
	}

	/** A battened junk sail set fore-and-aft on its mast: cloth panels between battens, a crest when Crest != Count. */
	void JunkSail(FDBArtBatcher& Batcher, const FVector& MastFoot, float MastHeight, float Width, float Height, M Cloth, M CrestRing, M Crest, int32 Battens,
		float Billow, float Side)
	{
		const float Top = MastFoot.Z + MastHeight - 60.f;
		const float Base = Top - Height;
		const float Y = Side * 30.f;
		for (int32 Panel = 0; Panel < Battens; ++Panel)
		{
			const float F0 = Panel / float(Battens);
			const float F1 = (Panel + 1) / float(Battens);
			const float F = (F0 + F1) * 0.5f;
			// Fan shape: wider aloft, the upper panels reach further aft; the cloth bellies between the battens.
			const float PanelWidth = Width * (0.94f + 0.06f * F);
			const float CenterX = MastFoot.X - Width * 0.18f - Width * 0.03f * F;
			const float Belly = Y + Side * Billow * FMath::Sin(PI * F);
			const float Z0 = Base + Height * F0;
			const float Z1 = Base + Height * F1;
			Batcher.Box(Cloth, FVector(CenterX, Belly, (Z0 + Z1) * 0.5f), FVector(PanelWidth, 8.f, Z1 - Z0 + 2.f));
			const float BattenY = Y + Side * Billow * FMath::Sin(PI * F1);
			Line(Batcher, M::WoodDark, FVector(CenterX - PanelWidth * 0.52f, BattenY, Z1), FVector(CenterX + PanelWidth * 0.52f, BattenY, Z1), 9.f);
			// Sheet lines from the batten ends down to the deck aft.
			if (Panel % 2 == 1)
			{
				Line(Batcher, M::WoodWet, FVector(CenterX - PanelWidth * 0.5f, BattenY, Z1), FVector(CenterX - PanelWidth * 0.75f, Y * 3.f, MastFoot.Z + 120.f), 4.f);
			}
		}
		Line(Batcher, M::WoodDark, FVector(MastFoot.X - Width * 0.18f - Width * 0.5f, Y, Base), FVector(MastFoot.X - Width * 0.18f + Width * 0.45f, Y, Base), 11.f);
		if (Crest != M::Count)
		{
			const FVector Center(MastFoot.X - Width * 0.23f, Y + Side * (Billow * 0.9f + 8.f), Base + Height * 0.55f);
			const float Diameter = FMath::Min(Width, Height) * 0.48f;
			for (const float Face : {-1.f, 1.f})
			{
				const FVector At = Center + FVector(0.f, Face * 7.f, 0.f);
				Batcher.Cylinder(CrestRing, At, Diameter, 3.f, FRotator(0.f, 0.f, 90.f));
				Batcher.Cylinder(Crest, At + FVector(0.f, Face * 2.f, 0.f), Diameter * 0.8f, 3.f, FRotator(0.f, 0.f, 90.f));
				Batcher.Cylinder(CrestRing, At + FVector(0.f, Face * 4.f, 0.f), Diameter * 0.32f, 3.f, FRotator(0.f, 0.f, 90.f));
			}
		}
	}

	void Mast(FDBArtBatcher& Batcher, const FVector& Foot, float Height, float Diameter)
	{
		Batcher.Box(M::WoodDark, Foot + FVector(0.f, 0.f, 40.f), FVector(Diameter * 2.5f, Diameter * 2.5f, 80.f));
		Batcher.Cylinder(M::WoodLight, Foot + FVector(0.f, 0.f, Height * 0.5f), Diameter, Height);
		Batcher.Sphere(M::WoodDark, Foot + FVector(0.f, 0.f, Height), FVector(Diameter * 1.6f));
	}

	/** Tiled roof with a ridge along X and upturned eaves. */
	void Roof(FDBArtBatcher& Batcher, const FVector& Center, float LengthX, float WidthY, float Pitch)
	{
		for (const float Side : {-1.f, 1.f})
		{
			Batcher.Box(M::RoofTile, Center + FVector(0.f, Side * WidthY * 0.25f, 0.f), FVector(LengthX, WidthY * 0.56f, 18.f), FRotator(0.f, 0.f, Side * Pitch));
			for (const float End : {-1.f, 1.f})
			{
				Batcher.Box(M::RoofTile, Center + FVector(End * LengthX * 0.5f, Side * WidthY * 0.52f, -WidthY * 0.12f), FVector(60.f, 60.f, 16.f),
					FRotator(End * 25.f, 0.f, Side * 20.f));
			}
		}
		Batcher.Box(M::RoofTile, Center + FVector(0.f, 0.f, WidthY * 0.14f), FVector(LengthX + 30.f, 34.f, 34.f));
	}

	void Lantern(FDBArtBatcher& Batcher, const FVector& At, float Size, TArray<FVector>* OutLanterns)
	{
		Batcher.Sphere(M::LanternFire, At, FVector(Size, Size, Size * 1.3f));
		Batcher.Box(M::WoodLacquerBlack, At + FVector(0.f, 0.f, Size * 0.7f), FVector(Size * 0.5f, Size * 0.5f, 10.f));
		if (OutLanterns)
		{
			OutLanterns->Add(At);
		}
	}

	/** Two rows of gun ports with cannon muzzles along both sides of the midbody. */
	void GunDecks(FDBArtBatcher& Batcher, const FHull& Hull)
	{
		for (const float Side : {-1.f, 1.f})
		{
			for (const float F : {0.52f, 0.8f})
			{
				for (float T = 0.2f; T < 0.72f; T += 0.055f)
				{
					const FVector Port = Hull.Point(T, F, Side);
					const FVector Out = FVector(0.f, Side, 0.f);
					Batcher.Box(M::WoodLacquerRed, Port + Out * 6.f, FVector(70.f, 10.f, 60.f));
					Batcher.Cylinder(M::MetalIron, Port + Out * 45.f, 28.f, 90.f, FRotator(0.f, 0.f, 90.f));
				}
			}
		}
	}

	/** Dragon figurehead: lacquered neck and head, bronze horns, glowing eyes. */
	void DragonHead(FDBArtBatcher& Batcher, const FHull& Hull)
	{
		constexpr float S = 1.7f; // figurehead scale
		const FVector Neck0(Hull.X(1.f) - 60.f, 0.f, Hull.Sheer(1.f) - 60.f);
		const FVector Neck1 = Neck0 + FVector(220.f, 0.f, 200.f) * S;
		const FVector Head = Neck1 + FVector(120.f, 0.f, 20.f) * S;
		Line(Batcher, M::WoodLacquerRed, Neck0, Neck1, 110.f * S);
		Batcher.Box(M::WoodLacquerRed, Head, FVector(240.f, 110.f, 110.f) * S, FRotator(-8.f, 0.f, 0.f));
		Batcher.Box(M::WoodLacquerBlack, Head + FVector(60.f, 0.f, -70.f) * S, FVector(200.f, 90.f, 36.f) * S, FRotator(12.f, 0.f, 0.f));
		for (const float Side : {-1.f, 1.f})
		{
			Batcher.Cone(M::MetalBronze, Head + FVector(-80.f, Side * 40.f, 110.f) * S, 30.f * S, 140.f * S, FRotator(-35.f, 0.f, 0.f));
			Batcher.Sphere(M::LanternFire, Head + FVector(60.f, Side * 50.f, 25.f) * S, FVector(22.f * S));
			Batcher.Box(M::FabricCrimson, Neck1 + FVector(-60.f, Side * 50.f, -10.f) * S, FVector(160.f, 6.f, 80.f) * S, FRotator(20.f, Side * 10.f, 0.f));
		}
		Batcher.Cone(M::MetalBronze, Head + FVector(150.f, 0.f, 50.f) * S, 26.f * S, 80.f * S, FRotator(-60.f, 0.f, 0.f));
	}

	/** Cargo on deck: crates, barrels and sacks around X, within HalfSpan. */
	void Cargo(FDBArtBatcher& Batcher, FRandomStream& Random, float X, float HalfSpan, float HalfWidth, float DeckZ, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector At(X + Random.FRandRange(-HalfSpan, HalfSpan), Random.FRandRange(-HalfWidth, HalfWidth), DeckZ);
			const float Pick = Random.FRand();
			if (Pick < 0.5f)
			{
				const float Size = Random.FRandRange(80.f, 130.f);
				const int32 Stack = Random.RandRange(1, 2);
				for (int32 Level = 0; Level < Stack; ++Level)
				{
					Batcher.Box(M::WoodLight, At + FVector(0.f, 0.f, Size * (Level + 0.5f)), FVector(Size), FRotator(0.f, Random.FRandRange(-12.f, 12.f), 0.f));
				}
			}
			else if (Pick < 0.8f)
			{
				Batcher.Cylinder(M::WoodDark, At + FVector(0.f, 0.f, 55.f), 70.f, 110.f);
			}
			else
			{
				Batcher.Sphere(M::FabricLinen, At + FVector(0.f, 0.f, 30.f), FVector(90.f, 70.f, 60.f));
			}
		}
	}

	void WarShip(FDBArtBatcher& Batcher, FRandomStream& Random, bool bCollision, TArray<FVector>* OutLanterns)
	{
		const FDBShipSpec& Spec = DBShipArt::GetSpec(EDBShipStyle::War);
		FHull Hull;
		Hull.Length = Spec.Length;
		Hull.Beam = Spec.Beam;
		Hull.DeckZ = Spec.DeckZ;
		Hull.Depth = 260.f;
		Hull.BowRise = 320.f;
		Hull.SternRise = 420.f;
		Hull.Stations = 22;
		Hull.Strakes = 7;
		Hull.Wale = M::WoodLacquerRed;
		BuildHull(Batcher, Hull, bCollision);
		GunDecks(Batcher, Hull);
		DragonHead(Batcher, Hull);

		// Stern castle: two storeys of lit windows, the helm on its roof deck, a pagoda roof on the upper storey.
		const float CastleLength = Hull.Length * 0.26f;
		const float CastleX = Hull.X(0.f) + 80.f + CastleLength * 0.5f;
		const float CastleWidth = Hull.HalfWidth(0.12f) * 1.9f;
		Batcher.SetCollision(bCollision);
		Batcher.Box(M::WoodDark, FVector(CastleX, 0.f, Spec.DeckZ + 140.f), FVector(CastleLength, CastleWidth, 280.f));
		Batcher.Box(M::WoodLight, FVector(CastleX, 0.f, Spec.SternDeckZ - 10.f), FVector(CastleLength + 40.f, CastleWidth + 40.f, 20.f));
		Batcher.SetCollision(false);
		const float UpperX = CastleX - CastleLength * 0.12f;
		Batcher.Box(M::WoodLacquerBlack, FVector(UpperX, 0.f, Spec.SternDeckZ + 120.f), FVector(CastleLength * 0.55f, CastleWidth * 0.7f, 240.f));
		Roof(Batcher, FVector(UpperX, 0.f, Spec.SternDeckZ + 300.f), CastleLength * 0.72f, CastleWidth * 0.95f, 24.f);
		for (const float Side : {-1.f, 1.f})
		{
			for (const float Z : {Spec.DeckZ + 90.f, Spec.DeckZ + 200.f})
			{
				Batcher.Box(M::PaperShojiLit, FVector(CastleX, Side * (CastleWidth * 0.5f + 4.f), Z), FVector(CastleLength * 0.85f, 6.f, 60.f));
			}
			Batcher.Box(M::PaperShojiLit, FVector(UpperX, Side * (CastleWidth * 0.35f + 4.f), Spec.SternDeckZ + 130.f), FVector(CastleLength * 0.45f, 6.f, 70.f));
			// Stern galleries and rails on the castle deck.
			Line(Batcher, M::WoodLacquerRed, FVector(CastleX - CastleLength * 0.5f, Side * CastleWidth * 0.52f, Spec.SternDeckZ + 60.f),
				FVector(CastleX + CastleLength * 0.5f, Side * CastleWidth * 0.52f, Spec.SternDeckZ + 60.f), 12.f);
		}
		Batcher.Box(M::PaperShojiLit, FVector(Hull.X(0.f) + 4.f, 0.f, Spec.DeckZ + 150.f), FVector(6.f, CastleWidth * 0.8f, 70.f));

		// Forecastle with a bronze-trimmed rail.
		Batcher.Box(M::WoodDark, FVector(Hull.X(0.8f), 0.f, Spec.DeckZ + 60.f), FVector(Hull.Length * 0.12f, Hull.HalfWidth(0.8f) * 1.7f, 120.f));

		// Three masts with crimson junk sails and golden crests, a small foresail raked over the bow.
		struct FRig { float T; float Height; float Width; float SailHeight; };
		for (const FRig& Rig : {FRig{0.74f, 3000.f, 1400.f, 1900.f}, FRig{0.5f, 3700.f, 1800.f, 2500.f}, FRig{0.27f, 2900.f, 1300.f, 1800.f}})
		{
			const FVector Foot(Hull.X(Rig.T), 0.f, Spec.DeckZ);
			Mast(Batcher, Foot, Rig.Height, 60.f);
			JunkSail(Batcher, Foot, Rig.Height, Rig.Width, Rig.SailHeight, M::FabricCrimson, M::MetalBronze, M::FabricBlack, 7, 90.f, 1.f);
			Lantern(Batcher, Foot + FVector(0.f, 0.f, Rig.Height * 0.3f), 30.f, OutLanterns);
			for (const float Side : {-1.f, 1.f})
			{
				Line(Batcher, M::WoodWet, Foot + FVector(0.f, 0.f, Rig.Height - 80.f), FVector(Foot.X - 250.f, Side * Hull.HalfWidth(Rig.T), Hull.Sheer(Rig.T)), 6.f);
			}
		}
		const FVector Bowsprit0(Hull.X(0.93f), 0.f, Hull.Sheer(0.93f));
		const FVector Bowsprit1 = Bowsprit0 + FVector(700.f, 0.f, 900.f);
		Line(Batcher, M::WoodLight, Bowsprit0, Bowsprit1, 40.f);
		Batcher.Box(M::FabricCrimson, (Bowsprit0 + Bowsprit1) * 0.5f + FVector(-120.f, 30.f, -150.f), FVector(600.f, 8.f, 520.f), FRotator(0.f, 0.f, 0.f));
		// Crimson banners along the rails and lanterns on the gunwale.
		for (float T = 0.3f; T < 0.85f; T += 0.09f)
		{
			for (const float Side : {-1.f, 1.f})
			{
				const FVector Foot = Hull.Point(T, 1.f, Side);
				Line(Batcher, M::WoodDark, Foot, Foot + FVector(0.f, 0.f, 520.f), 9.f);
				Batcher.Box(Random.FRand() < 0.75f ? M::FabricCrimson : M::FabricBlack, Foot + FVector(-60.f, 0.f, 380.f), FVector(120.f, 4.f, 240.f));
			}
		}
		for (float T = 0.25f; T < 0.9f; T += 0.16f)
		{
			for (const float Side : {-1.f, 1.f})
			{
				Lantern(Batcher, Hull.Point(T, 1.f, Side) + FVector(0.f, Side * 30.f, 60.f), 24.f, nullptr);
			}
		}
		Lantern(Batcher, FVector(Hull.X(0.f) - 30.f, 0.f, Spec.SternDeckZ + 120.f), 45.f, OutLanterns);
	}

	void FightingShip(FDBArtBatcher& Batcher, FRandomStream& Random, bool bCollision, TArray<FVector>* OutLanterns)
	{
		const FDBShipSpec& Spec = DBShipArt::GetSpec(EDBShipStyle::Fighting);
		FHull Hull;
		Hull.Length = Spec.Length;
		Hull.Beam = Spec.Beam;
		Hull.DeckZ = Spec.DeckZ;
		Hull.Depth = 170.f;
		Hull.BowRise = 520.f;
		Hull.SternRise = 220.f;
		Hull.SternWidth = 0.6f;
		Hull.Planks = M::WoodDark;
		Hull.PlanksAlt = M::WoodWet;
		Hull.Wale = M::WoodLacquerRed;
		BuildHull(Batcher, Hull, bCollision);

		// Iron-shod ram at the water line.
		Batcher.Cone(M::MetalIron, FVector(Hull.X(1.f) + 160.f, 0.f, 20.f), 90.f, 380.f, FRotator(-90.f, 0.f, 0.f));
		// Low stern platform with a small roofed cabin.
		Batcher.SetCollision(bCollision);
		Batcher.Box(M::WoodLight, FVector(Hull.X(0.08f), 0.f, Spec.SternDeckZ - 10.f), FVector(Hull.Length * 0.17f, Hull.HalfWidth(0.08f) * 1.9f, 20.f));
		Batcher.SetCollision(false);
		Batcher.Box(M::WoodLacquerBlack, FVector(Hull.X(0.16f), 0.f, Spec.DeckZ + 110.f), FVector(Hull.Length * 0.08f, Hull.HalfWidth(0.16f) * 1.2f, 220.f));
		Roof(Batcher, FVector(Hull.X(0.16f), 0.f, Spec.DeckZ + 250.f), Hull.Length * 0.11f, Hull.HalfWidth(0.16f) * 1.5f, 26.f);
		// Shield wall along the rails, crimson and black.
		for (const float Side : {-1.f, 1.f})
		{
			int32 Count = 0;
			for (float T = 0.18f; T < 0.82f; T += 0.035f, ++Count)
			{
				const FVector At = Hull.Point(T, 1.f, Side) + FVector(0.f, Side * 12.f, 30.f);
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Hull.HalfWidth(T + 0.01f) - Hull.HalfWidth(T - 0.01f), Hull.Length * 0.02f)) * Side;
				Batcher.Box(Count % 2 == 0 ? M::WoodLacquerBlack : M::WoodLacquerRed, At, FVector(90.f, 12.f, 100.f), FRotator(0.f, Yaw, 0.f));
			}
		}
		// Three masts with black junk sails and crimson crests.
		struct FRig { float T; float Height; float Width; float SailHeight; };
		for (const FRig& Rig : {FRig{0.72f, 2400.f, 1000.f, 1600.f}, FRig{0.49f, 2900.f, 1300.f, 2000.f}, FRig{0.24f, 2200.f, 900.f, 1400.f}})
		{
			const FVector Foot(Hull.X(Rig.T), 0.f, Spec.DeckZ);
			Mast(Batcher, Foot, Rig.Height, 45.f);
			JunkSail(Batcher, Foot, Rig.Height, Rig.Width, Rig.SailHeight, M::FabricBlack, M::FabricCrimson, M::FabricBlack, 6, 110.f, 1.f);
			// Pennant at the masthead.
			Batcher.Box(M::FabricCrimson, Foot + FVector(-90.f, 0.f, Rig.Height + 20.f), FVector(180.f, 4.f, 50.f));
		}
		Lantern(Batcher, FVector(Hull.X(0.f) - 20.f, 0.f, Spec.SternDeckZ + 120.f), 35.f, OutLanterns);
		Lantern(Batcher, FVector(Hull.X(1.f), 0.f, Hull.Sheer(1.f) + 30.f), 30.f, OutLanterns);
	}

	void MerchantShip(FDBArtBatcher& Batcher, FRandomStream& Random, bool bCollision, TArray<FVector>* OutLanterns)
	{
		const FDBShipSpec& Spec = DBShipArt::GetSpec(EDBShipStyle::Merchant);
		FHull Hull;
		Hull.Length = Spec.Length;
		Hull.Beam = Spec.Beam;
		Hull.DeckZ = Spec.DeckZ;
		Hull.Depth = 220.f;
		Hull.BowRise = 300.f;
		Hull.SternRise = 260.f;
		Hull.SternWidth = 0.8f;
		BuildHull(Batcher, Hull, bCollision);

		// Raised stern deck with an open pavilion (posts, curved roof).
		const float DeckX = Hull.X(0.1f);
		const float DeckLength = Hull.Length * 0.2f;
		const float DeckWidth = Hull.HalfWidth(0.1f) * 1.9f;
		Batcher.SetCollision(bCollision);
		Batcher.Box(M::WoodDark, FVector(DeckX, 0.f, (Spec.DeckZ + Spec.SternDeckZ) * 0.5f), FVector(DeckLength, DeckWidth, Spec.SternDeckZ - Spec.DeckZ));
		Batcher.Box(M::WoodLight, FVector(DeckX, 0.f, Spec.SternDeckZ - 10.f), FVector(DeckLength + 30.f, DeckWidth + 30.f, 20.f));
		Batcher.SetCollision(false);
		const float PavilionX = DeckX + DeckLength * 0.12f;
		for (const float SideX : {-1.f, 1.f})
		{
			for (const float SideY : {-1.f, 1.f})
			{
				Batcher.Box(M::WoodLacquerRed, FVector(PavilionX + SideX * DeckLength * 0.3f, SideY * DeckWidth * 0.32f, Spec.SternDeckZ + 130.f), FVector(22.f, 22.f, 260.f));
			}
		}
		Roof(Batcher, FVector(PavilionX, 0.f, Spec.SternDeckZ + 300.f), DeckLength * 0.85f, DeckWidth * 0.9f, 26.f);
		Lantern(Batcher, FVector(PavilionX, 0.f, Spec.SternDeckZ + 220.f), 30.f, OutLanterns);

		// Cargo between the masts.
		Cargo(Batcher, Random, Hull.X(0.55f), Hull.Length * 0.22f, Hull.Beam * 0.28f, Spec.DeckZ, 22);

		// Three masts with canvas junk sails.
		struct FRig { float T; float Height; float Width; float SailHeight; };
		for (const FRig& Rig : {FRig{0.77f, 2600.f, 1200.f, 1700.f}, FRig{0.52f, 3200.f, 1600.f, 2200.f}, FRig{0.3f, 2400.f, 1100.f, 1500.f}})
		{
			const FVector Foot(Hull.X(Rig.T), 0.f, Spec.DeckZ);
			Mast(Batcher, Foot, Rig.Height, 50.f);
			JunkSail(Batcher, Foot, Rig.Height, Rig.Width, Rig.SailHeight, M::FabricLinen, M::Count, M::Count, 7, 120.f, 1.f);
		}
		Lantern(Batcher, FVector(Hull.X(1.f), 0.f, Hull.Sheer(1.f) + 40.f), 30.f, OutLanterns);
	}

	void SmallBoat(FDBArtBatcher& Batcher, FRandomStream& Random, bool bCollision, TArray<FVector>* OutLanterns)
	{
		const FDBShipSpec& Spec = DBShipArt::GetSpec(EDBShipStyle::Boat);
		FHull Hull;
		Hull.Length = Spec.Length;
		Hull.Beam = Spec.Beam;
		Hull.DeckZ = Spec.DeckZ;
		Hull.Depth = 40.f;
		Hull.BowRise = 70.f;
		Hull.SternRise = 40.f;
		Hull.SternWidth = 0.5f;
		Hull.Stations = 10;
		Hull.Strakes = 3;
		Hull.Wale = M::WoodDark;
		BuildHull(Batcher, Hull, bCollision);
		for (const float T : {0.3f, 0.6f})
		{
			Batcher.Box(M::WoodLight, FVector(Hull.X(T), 0.f, Spec.DeckZ + 30.f), FVector(35.f, Hull.HalfWidth(T) * 1.8f, 8.f));
		}
		// Small canvas sail, lantern on a pole at the bow, the sculling oar over the stern.
		const FVector Foot(Hull.X(0.62f), 0.f, Spec.DeckZ);
		Mast(Batcher, Foot, 700.f, 14.f);
		JunkSail(Batcher, Foot, 700.f, 380.f, 480.f, M::FabricLinen, M::Count, M::Count, 4, 30.f, 1.f);
		const FVector PoleFoot(Hull.X(0.95f), 0.f, Hull.Sheer(0.95f));
		const FVector PoleTop = PoleFoot + FVector(60.f, 0.f, 200.f);
		Line(Batcher, M::WoodDark, PoleFoot, PoleTop, 8.f);
		Lantern(Batcher, PoleTop + FVector(30.f, 0.f, -40.f), 16.f, OutLanterns);
		Line(Batcher, M::WoodLight, FVector(Hull.X(0.1f), 20.f, Spec.DeckZ + 40.f), FVector(Hull.X(0.f) - 380.f, 60.f, -30.f), 10.f);
		if (Random.FRand() < 0.6f)
		{
			Batcher.Sphere(M::FabricLinen, FVector(Hull.X(0.4f), 0.f, Spec.DeckZ + 20.f), FVector(70.f, 60.f, 35.f));
		}
	}
}

const FDBShipSpec& DBShipArt::GetSpec(EDBShipStyle Style)
{
	static const FDBShipSpec Specs[] = {
		{4800.f, 1300.f, 520.f, 820.f, 850.f, 7.f, 3.f, NSLOCTEXT("DarkBlood", "ShipWar", "Kriegsschiff"), TEXT("junk_red_large"), 350.f},
		{3400.f, 760.f, 380.f, 520.f, 1900.f, 18.f, 2.f, NSLOCTEXT("DarkBlood", "ShipFighting", "Kampfschiff"), TEXT("junk_red_small"), 250.f},
		{3800.f, 1150.f, 420.f, 580.f, 1150.f, 10.f, 2.5f, NSLOCTEXT("DarkBlood", "ShipMerchant", "Handelsschiff"), TEXT("junk_merchant"), 300.f},
		{900.f, 230.f, 70.f, 70.f, 1000.f, 35.f, 0.6f, NSLOCTEXT("DarkBlood", "ShipBoat", "Kleines Boot"), TEXT("wooden_boat"), 25.f},
	};
	return Specs[FMath::Clamp(static_cast<int32>(Style), 0, static_cast<int32>(UE_ARRAY_COUNT(Specs)) - 1)];
}

bool DBShipArt::BuildModel(FDBArtBatcher& Batcher, EDBShipStyle Style)
{
	const FDBShipSpec& Spec = GetSpec(Style);
	return Spec.ModelKey && DBModels::BuildScaledToLength(Batcher, Spec.ModelKey, Spec.Length, FTransform(FVector(0.f, 0.f, -Spec.ModelDraft)));
}

void DBShipArt::Build(FDBArtBatcher& Batcher, EDBShipStyle Style, int32 Seed, bool bCollision, TArray<FVector>* OutLanterns)
{
	FRandomStream Random(Seed);
	switch (Style)
	{
	case EDBShipStyle::War: WarShip(Batcher, Random, bCollision, OutLanterns); break;
	case EDBShipStyle::Fighting: FightingShip(Batcher, Random, bCollision, OutLanterns); break;
	case EDBShipStyle::Boat: SmallBoat(Batcher, Random, bCollision, OutLanterns); break;
	default: MerchantShip(Batcher, Random, bCollision, OutLanterns); break;
	}
	Batcher.SetCollision(false);
}

void ADBShipModel::Build(FDBArtBatcher& Batcher)
{
	Batcher.SetCollision(false);
	if (DBShipArt::BuildModel(Batcher, Style))
	{
		return;
	}
	TArray<FVector> Lanterns;
	DBShipArt::Build(Batcher, Style, Seed, true, &Lanterns);
	// Warm light at the main lanterns (fades out with distance like every small light).
	for (int32 Index = 0; Index < FMath::Min(Lanterns.Num(), 3); ++Index)
	{
		AddLight(Lanterns[Index], Style == EDBShipStyle::Boat ? 400.f : 1600.f, Style == EDBShipStyle::Boat ? 600.f : 1400.f, FLinearColor(1.f, 0.55f, 0.25f));
	}
}
