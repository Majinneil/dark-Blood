#include "World/DBSettlementBuilder.h"

#include "Art/DBArtBuilder.h"
#include "Art/DBShipArt.h"
#include "Math/RandomStream.h"
#include "World/DBRealmLayout.h"

namespace
{
	using B = EDBBuildingType;
	using M = EDBArtMaterial;
	using DBArtBuild::FArtBuilder;

	const TCHAR* const PolyHaven = TEXT("/Game/DarkBlood/Art/Environment/PolyHaven/");

	FString Model(const TCHAR* Folder, const TCHAR* Res, const TCHAR* Mesh)
	{
		return FString::Printf(TEXT("%s%s/%s_%s/StaticMeshes/%s"), PolyHaven, Folder, Folder, Res, Mesh);
	}

	/** How a settlement type builds its houses. */
	struct FStyle
	{
		FName Region = TEXT("Village");
		float Wealth = 0.4f;
		int32 Floors = 1;
		TArray<B> Types = {B::SmallHouse};
		/** Distance between houses along a street and from the street center (cm). */
		float Lot = 1500.f;
		float Setback = 900.f;
		float Damage = 0.f;
	};

	struct FContext
	{
		FArtBuilder& Builder;
		FRandomStream Random;
		const FDBRealmSettlement& Site;
		float Radius;
		int32 NextSeed;
		/** What is already built: house centers and street segments (A, B, half width), so crossings stay free. */
		TArray<FVector> Houses;
		TArray<TTuple<FVector, FVector, float>> Roads;
		/** Areas kept free for large buildings (center, radius). */
		TArray<TPair<FVector, float>> Reserved;
		/** The street whose houses are being placed: its own houses stand beside it by design. */
		int32 CurrentRoad = INDEX_NONE;

		int32 Seed() { return NextSeed++; }

		bool IsFree(const FVector& At) const
		{
			for (const FVector& Other : Houses)
			{
				if (FVector::DistSquared2D(At, Other) < FMath::Square(1150.f))
				{
					return false;
				}
			}
			for (const TPair<FVector, float>& Area : Reserved)
			{
				if (FVector::DistSquared2D(At, Area.Key) < FMath::Square(Area.Value))
				{
					return false;
				}
			}
			for (int32 Index = 0; Index < Roads.Num(); ++Index)
			{
				if (Index == CurrentRoad)
				{
					continue;
				}
				const TTuple<FVector, FVector, float>& Road = Roads[Index];
				const FVector OnRoad = FMath::ClosestPointOnSegment(FVector(At.X, At.Y, 0.f), FVector(Road.Get<0>().X, Road.Get<0>().Y, 0.f),
					FVector(Road.Get<1>().X, Road.Get<1>().Y, 0.f));
				if (FVector::Dist2D(At, OnRoad) < Road.Get<2>() + 650.f)
				{
					return false;
				}
			}
			return true;
		}
	};

	void House(FContext& C, const FStyle& Style, const FVector& At, float Yaw, B Type, int32 X = 0, int32 Y = 0)
	{
		if (!C.IsFree(At))
		{
			return;
		}
		C.Houses.Add(At);
		const int32 ModulesX = X > 0 ? X : C.Random.RandRange(2, 3);
		const int32 ModulesY = Y > 0 ? Y : C.Random.RandRange(2, 4);
		const int32 Floors = Style.Floors > 1 && C.Random.FRand() < 0.6f ? Style.Floors : 1;
		C.Builder.Building(At, Yaw, Type, C.Seed(), Style.Region, FMath::Clamp(Style.Wealth + C.Random.FRandRange(-0.15f, 0.15f), 0.f, 1.f), ModulesX, ModulesY,
			Floors, Style.Damage);
	}

	/** Registers a street so houses keep off it; returns its index (a street planned twice is registered once). */
	int32 PlanRoad(FContext& C, const FVector& A, const FVector& Bp, float Width)
	{
		for (int32 Index = 0; Index < C.Roads.Num(); ++Index)
		{
			if (C.Roads[Index].Get<0>().Equals(A) && C.Roads[Index].Get<1>().Equals(Bp))
			{
				return Index;
			}
		}
		return C.Roads.Emplace(A, Bp, Width * 0.5f);
	}

	/** Houses on both sides of a straight street from A to B, entrances facing the street. */
	void Street(FContext& C, const FStyle& Style, const FVector& A, const FVector& Bp, float Width, bool bLanterns = true,
		EDBLanternStyle Lantern = EDBLanternStyle::WoodPost)
	{
		C.Builder.Spline(EDBSplineDressing::Road, {A, Bp}, Width, C.Seed());
		// Streets are known before houses go up (a layout may plan them all first), so no house stands on a later street.
		C.CurrentRoad = PlanRoad(C, A, Bp, Width);
		const FVector Dir = (Bp - A).GetSafeNormal2D();
		const FVector Side(-Dir.Y, Dir.X, 0.f);
		const float Length = FVector::Dist2D(A, Bp);
		for (float T = Style.Lot * 0.8f; T < Length - Style.Lot * 0.5f; T += Style.Lot * C.Random.FRandRange(0.9f, 1.15f))
		{
			for (const float Sign : {-1.f, 1.f})
			{
				if (C.Random.FRand() < 0.12f)
				{
					continue; // a gap: gardens, alleys
				}
				const FVector At = A + Dir * T + Side * Sign * (Width * 0.5f + Style.Setback);
				// Entrance (+X) faces the street.
				const float Yaw = (-Side * Sign).Rotation().Yaw;
				House(C, Style, At, Yaw, Style.Types[C.Random.RandRange(0, Style.Types.Num() - 1)]);
			}
			if (bLanterns && C.Random.FRand() < 0.55f)
			{
				C.Builder.Lantern(A + Dir * T + Side * (Width * 0.5f + 60.f), Lantern);
			}
		}
		C.CurrentRoad = INDEX_NONE;
	}

	/** Fills the blocks between the streets in [Min, Max] (local XY) with houses on a jittered grid, each facing its
	 *  nearest street; Chance thins it out (dense city blocks vs. scattered farmsteads). Streets must be planned first. */
	void Infill(FContext& C, const FStyle& Style, const FVector2D& Min, const FVector2D& Max, float Spacing, float Chance)
	{
		for (float X = Min.X; X <= Max.X; X += Spacing)
		{
			for (float Y = Min.Y; Y <= Max.Y; Y += Spacing)
			{
				if (C.Random.FRand() > Chance)
				{
					continue;
				}
				const FVector At(X + C.Random.FRandRange(-0.15f, 0.15f) * Spacing, Y + C.Random.FRandRange(-0.15f, 0.15f) * Spacing, 0.f);
				FVector Facing = FVector::ForwardVector;
				float Best = TNumericLimits<float>::Max();
				for (const TTuple<FVector, FVector, float>& Road : C.Roads)
				{
					const FVector OnRoad = FMath::ClosestPointOnSegment(At, Road.Get<0>(), Road.Get<1>());
					const float Distance = FVector::Dist2D(At, OnRoad);
					if (Distance < Best && Distance > 1.f)
					{
						Best = Distance;
						Facing = (OnRoad - At).GetSafeNormal2D();
					}
				}
				// Entrance (+X) towards the street, snapped to the street grid.
				const float Yaw = FMath::GridSnap(Facing.Rotation().Yaw, 90.f);
				House(C, Style, At, Yaw, Style.Types[C.Random.RandRange(0, Style.Types.Num() - 1)]);
			}
		}
	}

	FVector Polar(float Radius, float Degrees)
	{
		return FRotator(0.f, Degrees, 0.f).Vector() * Radius;
	}

	/** Wall ring (square) with roofed gates where the main streets leave. */
	void WalledSquare(FContext& C, float Half)
	{
		const FVector Corners[] = {FVector(-Half, -Half, 0.f), FVector(Half, -Half, 0.f), FVector(Half, Half, 0.f), FVector(-Half, Half, 0.f)};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector A = Corners[Index];
			const FVector Bp = Corners[(Index + 1) % 4];
			const FVector Mid = (A + Bp) * 0.5f;
			const FVector Dir = (Bp - A).GetSafeNormal2D();
			C.Builder.Spline(EDBSplineDressing::CastleWall, {A, Mid - Dir * 280.f}, 60.f, C.Seed());
			C.Builder.Spline(EDBSplineDressing::CastleWall, {Mid + Dir * 280.f, Bp}, 60.f, C.Seed());
			if (ADBGate* Gate = C.Builder.Begin<ADBGate>(Mid, Dir.Rotation().Yaw + 90.f))
			{
				Gate->Style = EDBGateStyle::RoofedGate;
				Gate->Width = 480.f;
				Gate->Height = 460.f;
				C.Builder.Finish(Gate, Mid, Dir.Rotation().Yaw + 90.f);
			}
		}
	}

	void Pier(FContext& C, const FVector& Start, float Length, float Width)
	{
		const FVector Center = Start + FVector(Length * 0.5f, 0.f, 0.f);
		if (ADBBridge* Deck = C.Builder.Begin<ADBBridge>(Center))
		{
			Deck->Length = Length;
			Deck->Width = Width;
			Deck->ArchHeight = 0.f;
			Deck->bLacquered = false;
			Deck->Seed = C.Seed();
			C.Builder.Finish(Deck, Center);
		}
	}

	/** A moored ship; the keel sits below the water line (water is at local Z = -GroundHeight). */
	/** A moored Japanese ship floating at the water line (the frame stands on the leveled ground). */
	void Ship(FContext& C, const FVector& At, float Yaw, EDBShipStyle Style)
	{
		const FVector Local = At + FVector(0.f, 0.f, static_cast<float>(-C.Site.GroundHeight * 100.0));
		if (ADBShipModel* Model = C.Builder.Begin<ADBShipModel>(Local, Yaw))
		{
			Model->Style = Style;
			Model->Seed = C.Seed();
			C.Builder.Finish(Model, Local, Yaw);
		}
	}

	void Cargo(FContext& C, const FVector& At)
	{
		const FString Crate = Model(TEXT("wooden_crate_02"), TEXT("1k"), TEXT("wooden_crate_02_crate"));
		const FString Barrel = Model(TEXT("wooden_barrels_01"), TEXT("1k"), TEXT("wooden_barrels_01_barrel01"));
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector Offset(C.Random.FRandRange(-250.f, 250.f), C.Random.FRandRange(-250.f, 250.f), 0.f);
			C.Builder.Prop(At + Offset, C.Random.FRandRange(0.f, 360.f), {C.Random.FRand() < 0.5f ? *Crate : *Barrel}, 0.f);
		}
	}

	/** Harbor: quay street along the shore, warehouses facing the water, piers and moored ships, the town behind. */
	void Harbor(FContext& C, const FStyle& Town, bool bGreat)
	{
		const float Shore = static_cast<float>(C.Site.ShoreDistance * 100.0);
		const float Half = C.Radius * 0.9f;
		FStyle Quay = Town;
		Quay.Types = {B::Warehouse, B::Warehouse, B::MerchantHouse};
		Quay.Lot = 1700.f;
		C.Builder.Spline(EDBSplineDressing::Road, {FVector(Shore - 600.f, -Half, 0.f), FVector(Shore - 600.f, Half, 0.f)}, 700.f, C.Seed());
		PlanRoad(C, FVector(Shore - 600.f, -Half, 0.f), FVector(Shore - 600.f, Half, 0.f), 700.f);
		// The town behind the warehouses: streets running inland, crossed by streets parallel to the quay (planned first so
		// no house stands on a crossing).
		const float Back = Shore - 2900.f;
		TArray<TPair<FVector, FVector>> Streets;
		const int32 Inland = bGreat ? 5 : 3;
		for (int32 Index = 0; Index < Inland; ++Index)
		{
			const float Y = FMath::Lerp(-Half * 0.8f, Half * 0.8f, Index / float(Inland - 1));
			Streets.Emplace(FVector(Back, Y, 0.f), FVector(-Half, Y, 0.f));
		}
		const int32 Cross = bGreat ? 2 : 1;
		for (int32 Index = 1; Index <= Cross; ++Index)
		{
			const float X = FMath::Lerp(Back, -Half, Index / float(Cross + 1));
			Streets.Emplace(FVector(X, -Half * 0.85f, 0.f), FVector(X, Half * 0.85f, 0.f));
		}
		for (const TPair<FVector, FVector>& Street : Streets)
		{
			PlanRoad(C, Street.Key, Street.Value, 450.f);
		}
		for (float Y = -Half + 900.f; Y < Half - 900.f; Y += Quay.Lot)
		{
			if (C.Random.FRand() < 0.85f)
			{
				House(C, Quay, FVector(Shore - 1900.f, Y, 0.f), 0.f, Quay.Types[C.Random.RandRange(0, Quay.Types.Num() - 1)], 3, 3);
			}
			C.Builder.Lantern(FVector(Shore - 180.f, Y + 400.f, 0.f), EDBLanternStyle::WoodPost);
		}
		const int32 Piers = bGreat ? 5 : 3;
		for (int32 Index = 0; Index < Piers; ++Index)
		{
			const float Y = FMath::Lerp(-Half * 0.75f, Half * 0.75f, Index / float(Piers - 1));
			const float Length = C.Random.FRandRange(4500.f, 7500.f);
			Pier(C, FVector(Shore - 300.f, Y, -40.f), Length, 450.f);
			// The capital's harbor holds war ships between the merchant and fighting ships.
			const EDBShipStyle Style = bGreat && Index % 2 == 0 ? EDBShipStyle::War : (Index == 1 ? EDBShipStyle::Fighting : EDBShipStyle::Merchant);
			Ship(C, FVector(Shore + Length * 0.6f, Y + (Index % 2 == 0 ? 1300.f : -1100.f), 0.f), C.Random.FRand() < 0.5f ? 0.f : 180.f, Style);
			for (int32 Boat = 0; Boat < 2; ++Boat)
			{
				Ship(C, FVector(Shore + C.Random.FRandRange(600.f, Length * 0.8f), Y + (Boat == 0 ? -420.f : 420.f), 0.f), C.Random.FRandRange(-15.f, 15.f),
					EDBShipStyle::Boat);
			}
			Cargo(C, FVector(Shore - 900.f, Y + 400.f, 0.f));
		}
		for (const TPair<FVector, FVector>& Planned : Streets)
		{
			Street(C, Town, Planned.Key, Planned.Value, 450.f);
		}
		FStyle Backyard = Town;
		Backyard.Floors = 1;
		Infill(C, Backyard, FVector2D(-Half, -Half * 0.85f), FVector2D(Back - 600.f, Half * 0.85f), 1700.f, bGreat ? 0.7f : 0.45f);
		C.Builder.Fx(EDBAmbientFx::Fireflies, FVector(Shore - 1500.f, 0.f, 0.f), FVector(Half * 0.6f, Half, 200.f), 60, C.Seed(), true);
	}

	void RicePaddies(FContext& C, float Inner, float Outer)
	{
		const float Cell = 1600.f;
		for (float X = -Outer; X < Outer; X += Cell)
		{
			for (float Y = -Outer; Y < Outer; Y += Cell)
			{
				const float Distance = FVector2D(X, Y).Size();
				if (Distance < Inner || Distance > Outer || C.Random.FRand() < 0.1f)
				{
					continue;
				}
				// Flooded paddy inside an earth dike.
				C.Builder.Ground(FVector(X, Y, 0.f), FVector2D(Cell - 60.f, Cell - 60.f), M::GroundEarth, 3.f);
				C.Builder.Ground(FVector(X, Y, 0.f), FVector2D(Cell - 260.f, Cell - 260.f), M::Water, 6.f);
			}
		}
	}

	void VillageLayout(FContext& C, const FDBRealmSettlement& Site)
	{
		using S = EDBSettlementType;
		FArtBuilder& Builder = C.Builder;
		const float R = C.Radius;
		const bool bForest = Site.Type == S::ForestSettlement;
		const bool bSnow = Site.Type == S::SnowSettlement;
		const FStyle Village{TEXT("Village"), bSnow ? 0.25f : 0.35f, 1, {B::SmallHouse, B::SmallHouse, B::LargeHouse, B::Warehouse}, 1400.f, 800.f};
		// A bending main street through the village, houses along it.
		const FVector A(-R * 0.9f, -R * 0.2f, 0.f);
		const FVector Mid(0.f, R * 0.15f, 0.f);
		const FVector Bp(R * 0.9f, -R * 0.1f, 0.f);
		Street(C, Village, A, Mid, 450.f);
		Street(C, Village, Mid, Bp, 450.f);
		House(C, Village, FVector(0.f, -R * 0.35f, 0.f), 90.f, B::VillageHall, 3, 4);
		if (Site.Type != S::Village)
		{
			House(C, Village, FVector(R * 0.3f, R * 0.55f, 0.f), -90.f, B::Shrine, 2, 2);
		}
		// Farmsteads scattered off the main street.
		Infill(C, Village, FVector2D(-R * 0.75f, -R * 0.75f), FVector2D(R * 0.75f, R * 0.75f), 2000.f, 0.18f);
		Builder.Spline(bForest || Site.Type == S::MountainVillage ? EDBSplineDressing::BambooFence : EDBSplineDressing::WoodFence,
			{Polar(R, 200.f), Polar(R, 250.f), Polar(R, 300.f), Polar(R, 340.f)}, 0.f, C.Seed());
		if (bForest)
		{
			Builder.Fx(EDBAmbientFx::Fireflies, FVector::ZeroVector, FVector(R, R, 200.f), 120, C.Seed(), true);
		}
		if (bSnow)
		{
			Builder.Fx(EDBAmbientFx::Embers, FVector(0.f, -R * 0.35f + 900.f, 0.f), FVector(150.f, 150.f, 220.f), 40, C.Seed());
		}
		if (Site.Type == S::Village)
		{
			Builder.Scatter(FVector(R * 0.6f, R * 0.6f, 0.f), FVector(R * 0.3f, R * 0.3f, 500.f), EDBBiome::CherryGrove, 5.f, 3.f, C.Seed(), 1.f);
		}
	}
}

namespace DBSettlements
{
	float FrameYaw(const FDBRealmSettlement& Site, int32 Seed)
	{
		if (!Site.SeaDirection.IsZero())
		{
			return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Site.SeaDirection.Y, Site.SeaDirection.X)));
		}
		return FRandomStream(Seed).FRandRange(0.f, 90.f);
	}

	void Build(FArtBuilder& Builder, const FDBRealmSettlement& Site, int32 Seed)
	{
		using S = EDBSettlementType;
		FContext C{Builder, FRandomStream(Seed), Site, Site.Radius * 100.f, Seed * 100 + 1};
		const float R = C.Radius;
		// Packed ground under the settlement; at the coast it ends at the shore (+X faces the sea) instead of covering the water.
		if (Site.SeaDirection.IsZero())
		{
			Builder.Ground(FVector::ZeroVector, FVector2D(R * 1.7f, R * 1.7f), M::GroundEarth, 1.f, 0.f, true);
		}
		else
		{
			// Only the working waterfront is packed earth; the town behind shows the meadow between its streets.
			const float Shore = static_cast<float>(Site.ShoreDistance * 100.0);
			Builder.Ground(FVector(Shore - 1300.f, 0.f, 0.f), FVector2D(2600.f, R * 1.7f), M::GroundEarth, 1.f);
		}

		switch (Site.Type)
		{
		case S::Capital:
		{
			const FStyle City{TEXT("Capital"), 0.9f, 2, {B::LargeHouse, B::MerchantHouse, B::LargeHouse, B::Guardhouse}, 1800.f, 1000.f};
			Builder.Ground(FVector::ZeroVector, FVector2D(R * 1.5f, R * 1.5f), M::GroundCourtyard, 1.4f);
			Street(C, City, FVector(-R * 0.85f, 0.f, 0.f), FVector(R * 0.85f, 0.f, 0.f), 700.f, true, EDBLanternStyle::Stone);
			Street(C, City, FVector(0.f, -R * 0.85f, 0.f), FVector(0.f, R * 0.85f, 0.f), 700.f, true, EDBLanternStyle::Stone);
			WalledSquare(C, R * 0.92f);
			Builder.Building(FVector(-R * 0.5f, -R * 0.5f, 0.f), 45.f, B::TempleHall, C.Seed(), TEXT("Temple"), 0.9f, 5, 4, 2);
			Builder.Scatter(FVector(R * 0.5f, R * 0.5f, 0.f), FVector(R * 0.3f, R * 0.3f, 500.f), EDBBiome::CherryGrove, 6.f, 4.f, C.Seed(), 1.f);
			Builder.Fx(EDBAmbientFx::CherryPetals, FVector(R * 0.5f, R * 0.5f, 0.f), FVector(R * 0.3f, R * 0.3f, 500.f), 220, C.Seed());
			break;
		}
		case S::GreatCity:
		{
			const FStyle City{TEXT("Capital"), 0.7f, 2, {B::MerchantHouse, B::Tavern, B::LargeHouse, B::MerchantHouse, B::Warehouse}, 1600.f, 900.f};
			// Cross streets and the temple precinct are planned first: no house stands on them.
			for (const float Offset : {-0.25f, 0.25f})
			{
				PlanRoad(C, FVector(R * Offset, -R * 0.9f, 0.f), FVector(R * Offset, R * 0.9f, 0.f), 500.f);
			}
			C.Reserved.Emplace(FVector(0.f, R * 0.75f, 0.f), 2600.f);
			for (const float Offset : {-0.5f, 0.f, 0.5f})
			{
				Street(C, City, FVector(-R * 0.9f, R * Offset, 0.f), FVector(R * 0.9f, R * Offset, 0.f), 600.f);
			}
			for (const float Offset : {-0.25f, 0.25f})
			{
				Street(C, City, FVector(R * Offset, -R * 0.9f, 0.f), FVector(R * Offset, R * 0.9f, 0.f), 500.f);
			}
			Builder.Building(FVector(0.f, R * 0.75f, 0.f), -90.f, B::TempleHall, C.Seed(), TEXT("Temple"), 0.8f, 4, 4, 2);
			FStyle Backyard = City;
			Backyard.Floors = 1;
			Infill(C, Backyard, FVector2D(-R * 0.85f, -R * 0.85f), FVector2D(R * 0.85f, R * 0.85f), 1800.f, 0.75f);
			break;
		}
		case S::TavernTown:
		{
			const FStyle Town{TEXT("Village"), 0.55f, 2, {B::Tavern, B::Tavern, B::MerchantHouse, B::SmallHouse}, 1500.f, 850.f};
			Street(C, Town, FVector(-R * 0.9f, 0.f, 0.f), FVector(R * 0.9f, 0.f, 0.f), 550.f);
			Street(C, Town, FVector(0.f, -R * 0.9f, 0.f), FVector(0.f, R * 0.9f, 0.f), 550.f);
			Infill(C, Town, FVector2D(-R * 0.8f, -R * 0.8f), FVector2D(R * 0.8f, R * 0.8f), 1800.f, 0.35f);
			break;
		}
		case S::HarborTown:
		{
			const bool bGreat = FCString::Strcmp(Site.Name, TEXT("Hauptstadthafen")) == 0;
			const FStyle Town{bGreat ? FName(TEXT("Capital")) : FName(TEXT("Village")), bGreat ? 0.8f : 0.5f, 2,
				{B::MerchantHouse, B::Tavern, B::LargeHouse, B::SmallHouse}, 1500.f, 850.f};
			Harbor(C, Town, bGreat);
			break;
		}
		case S::FishingVillage:
		{
			const FStyle Huts{TEXT("Village"), 0.2f, 1, {B::SmallHouse}, 1300.f, 700.f};
			const float Shore = static_cast<float>(Site.ShoreDistance * 100.0);
			for (int32 Index = 0; Index < 7; ++Index)
			{
				House(C, Huts, FVector(Shore - C.Random.FRandRange(1200.f, 3500.f), C.Random.FRandRange(-R * 0.7f, R * 0.7f), 0.f), C.Random.FRandRange(-20.f, 20.f),
					B::SmallHouse, 2, 2);
			}
			Pier(C, FVector(Shore - 200.f, -R * 0.2f, -40.f), 3800.f, 300.f);
			Pier(C, FVector(Shore - 200.f, R * 0.3f, -40.f), 3000.f, 300.f);
			for (int32 Boat = 0; Boat < 6; ++Boat)
			{
				const float Y = (Boat < 3 ? -R * 0.2f : R * 0.3f) + (Boat % 3 - 1) * 380.f;
				Ship(C, FVector(Shore + C.Random.FRandRange(800.f, 2600.f), Y, 0.f), C.Random.FRandRange(-20.f, 20.f), EDBShipStyle::Boat);
			}
			Cargo(C, FVector(Shore - 700.f, 0.f, 0.f));
			break;
		}
		case S::RiceVillage:
		{
			const FStyle Farm{TEXT("Village"), 0.3f, 1, {B::SmallHouse, B::SmallHouse, B::Warehouse}, 1400.f, 800.f};
			Street(C, Farm, FVector(-R * 0.4f, 0.f, 0.f), FVector(R * 0.4f, 0.f, 0.f), 400.f);
			House(C, Farm, FVector(0.f, R * 0.25f, 0.f), -90.f, B::VillageHall, 3, 4);
			RicePaddies(C, R * 0.45f, R * 1.0f);
			break;
		}
		case S::TempleSettlement:
		{
			Builder.Building(FVector(R * 0.55f, 0.f, 0.f), 180.f, B::TempleHall, C.Seed(), TEXT("Temple"), 0.9f, 6, 5, 2);
			Builder.Spline(EDBSplineDressing::StonePath, {FVector(-R * 0.95f, 0.f, 0.f), FVector(R * 0.35f, 0.f, 0.f)}, 260.f, C.Seed());
			const FString Torii = TEXT("/Game/DarkBlood/Art/Fab/Torii_Pikas/scene/StaticMeshes/Torri_Gate_Torri_gate_0");
			const FString Rope = TEXT("/Game/DarkBlood/Art/Fab/Torii_Pikas/scene/StaticMeshes/Torri_Gate_Rope_Gold_0");
			for (const float X : {-0.8f, -0.5f, -0.2f})
			{
				if (!Builder.Prop(FVector(R * X, 0.f, 0.f), 0.f, {*Torii, *Rope}, 620.f, FRotator(0.f, 90.f, 0.f)))
				{
					if (ADBGate* Gate = Builder.Begin<ADBGate>(FVector(R * X, 0.f, 0.f)))
					{
						Gate->Style = EDBGateStyle::Torii;
						Builder.Finish(Gate, FVector(R * X, 0.f, 0.f));
					}
				}
				Builder.Lantern(FVector(R * X + 350.f, -420.f, 0.f), EDBLanternStyle::Stone);
				Builder.Lantern(FVector(R * X + 350.f, 420.f, 0.f), EDBLanternStyle::Stone);
			}
			const FStyle Monks{TEXT("Temple"), 0.6f, 1, {B::Shrine, B::SmallHouse}, 1500.f, 900.f};
			for (const float Sign : {-1.f, 1.f})
			{
				House(C, Monks, FVector(R * 0.1f, Sign * R * 0.6f, 0.f), Sign * -90.f, B::Shrine, 2, 2);
				House(C, Monks, FVector(R * 0.55f, Sign * R * 0.7f, 0.f), Sign * -90.f, B::SmallHouse);
			}
			Builder.Scatter(FVector(-R * 0.3f, 0.f, 0.f), FVector(R * 0.35f, R * 0.45f, 500.f), EDBBiome::ShrineGarden, 3.f, 3.f, C.Seed(), 0.4f);
			Builder.Fx(EDBAmbientFx::CherryPetals, FVector(-R * 0.3f, 0.f, 0.f), FVector(R * 0.4f, R * 0.5f, 600.f), 300, C.Seed());
			break;
		}
		case S::BorderOutpost:
		{
			const FStyle Fort{TEXT("Capital"), 0.5f, 2, {B::Guardhouse}, 1600.f, 900.f};
			WalledSquare(C, R * 0.75f);
			for (const FVector& At : {FVector(-R * 0.4f, -R * 0.4f, 0.f), FVector(R * 0.4f, -R * 0.4f, 0.f), FVector(-R * 0.4f, R * 0.4f, 0.f), FVector(R * 0.4f, R * 0.4f, 0.f)})
			{
				House(C, Fort, At, (-At).Rotation().Yaw, B::Guardhouse, 3, 3);
			}
			Builder.Spline(EDBSplineDressing::Road, {FVector(-R * 1.2f, 0.f, 0.f), FVector(R * 1.2f, 0.f, 0.f)}, 500.f, C.Seed());
			Builder.Fx(EDBAmbientFx::Embers, FVector::ZeroVector, FVector(200.f, 200.f, 250.f), 40, C.Seed());
			break;
		}
		case S::CaravanTown:
		{
			const FStyle Market{TEXT("Village"), 0.45f, 1, {B::MerchantHouse, B::Warehouse, B::Tavern}, 1600.f, 900.f};
			Street(C, Market, FVector(-R, 0.f, 0.f), FVector(R, 0.f, 0.f), 700.f);
			Infill(C, Market, FVector2D(-R * 0.8f, -R * 0.6f), FVector2D(R * 0.8f, R * 0.6f), 1900.f, 0.3f);
			for (const float X : {-0.5f, 0.f, 0.5f})
			{
				Cargo(C, FVector(R * X, 0.f, 0.f));
			}
			break;
		}
		case S::MiningTown:
		{
			const FStyle Mine{TEXT("Village"), 0.3f, 1, {B::Smithy, B::Warehouse, B::SmallHouse}, 1500.f, 850.f, 0.1f};
			Street(C, Mine, FVector(-R * 0.9f, 0.f, 0.f), FVector(R * 0.9f, 0.f, 0.f), 500.f);
			Infill(C, Mine, FVector2D(-R * 0.8f, -R * 0.5f), FVector2D(R * 0.8f, R * 0.5f), 1900.f, 0.25f);
			for (const float X : {-0.4f, 0.3f})
			{
				Builder.Fx(EDBAmbientFx::Embers, FVector(R * X, 1300.f, 0.f), FVector(200.f, 200.f, 300.f), 50, C.Seed());
			}
			Builder.Scatter(FVector(0.f, R * 0.8f, 0.f), FVector(R, R * 0.3f, 600.f), EDBBiome::Corrupted, 0.f, 6.f, C.Seed());
			break;
		}
		case S::RiverSettlement:
		{
			const FStyle River{TEXT("Village"), 0.4f, 1, {B::SmallHouse, B::LargeHouse, B::MerchantHouse}, 1400.f, 800.f};
			Builder.Spline(EDBSplineDressing::Stream, {FVector(0.f, -R * 1.1f, 0.f), FVector(200.f, 0.f, 0.f), FVector(0.f, R * 1.1f, 0.f)}, 500.f, C.Seed());
			if (ADBBridge* Bridge = Builder.Begin<ADBBridge>(FVector(200.f, 0.f, 0.f)))
			{
				Bridge->Length = 900.f;
				Bridge->Width = 380.f;
				Builder.Finish(Bridge, FVector(200.f, 0.f, 0.f));
			}
			Street(C, River, FVector(-R * 0.9f, 0.f, 0.f), FVector(-400.f, 0.f, 0.f), 450.f);
			Street(C, River, FVector(800.f, 0.f, 0.f), FVector(R * 0.9f, 0.f, 0.f), 450.f);
			break;
		}
		case S::OasisTown:
		{
			const FStyle Oasis{TEXT("Village"), 0.5f, 1, {B::MerchantHouse, B::Warehouse, B::SmallHouse}, 1500.f, 850.f};
			Builder.Ground(FVector::ZeroVector, FVector2D(R * 0.75f, R * 0.65f), M::GroundForest, 3.f, 0.f, true);
			Builder.Ground(FVector::ZeroVector, FVector2D(R * 0.55f, R * 0.45f), M::Water, 5.f, 0.f, true);
			for (int32 Index = 0; Index < 12; ++Index)
			{
				const float Angle = Index * 30.f + C.Random.FRandRange(-8.f, 8.f);
				House(C, Oasis, Polar(R * C.Random.FRandRange(0.55f, 0.8f), Angle), Angle + 180.f, Oasis.Types[C.Random.RandRange(0, 2)]);
			}
			Builder.Scatter(FVector::ZeroVector, FVector(R * 0.35f, R * 0.35f, 400.f), EDBBiome::WetForest, 5.f, 6.f, C.Seed(), 1.5f);
			break;
		}
		default:
			VillageLayout(C, Site);
			break;
		}
	}
}
