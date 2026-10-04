// DARK BLOOD - engine-free check of the realm layout (Source/DarkBlood/.../World/DBRealmLayout.cpp).
// Lays the river water surfaces exactly like DBBuildRealmCommandlet and checks them against the terrain:
//   - the water level never rises downstream,
//   - the channel bed stays under the water,
//   - no flat water edge hangs above lower ground (waterfalls may: their sides hang free in front of the cliff).
// Usage: RealmLayoutCheck [heights.bin [grid meters]] - optionally dumps a height grid (int32 N, N*N float32).
#include "World/DBRealmLayout.h"

#include <cstdio>
#include <cstdlib>

namespace
{
	constexpr double RiverSurfaceMinLevel = 2.5; // as DBBuildRealmCommandlet
	constexpr double WaterfallMinDrop = 3.0;

	struct FSurface
	{
		FVector2D Mid, Dir;
		double HalfLength, HalfWidth, MidWater, Slope;
	};

	bool InSettlement(const FVector2D& P)
	{
		for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
		{
			if (FVector2D::Distance(P, Site.Center) < Site.Radius * 2.2)
			{
				return true;
			}
		}
		return FVector2D::Distance(P, DBRealm::GetCapital().Center) < DBRealm::CapitalFlatRadius * 2.4;
	}
}

int main(int argc, char** argv)
{
	for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
	{
		printf("settlement %-16s ground %7.2f m\n", Site.Name, Site.GroundHeight);
	}

	std::vector<FSurface> Surfaces;
	int Rising = 0, Falls = 0;
	double Tallest = 0.0, Kilometers = 0.0;
	for (const TArray<FDBRiverPoint>& Course : DBRealm::GetRiverCourses())
	{
		for (int32 Index = 0; Index + 1 < Course.Num(); ++Index)
		{
			const FDBRiverPoint& A = Course[Index];
			const FDBRiverPoint& B = Course[Index + 1];
			Rising += B.Water > A.Water ? 1 : 0;
			if (std::max(A.Water, B.Water) < RiverSurfaceMinLevel || InSettlement(A.Position))
			{
				continue;
			}
			const double Length = FVector2D::Distance(A.Position, B.Position);
			const FVector2D Dir = (B.Position - A.Position) * (1.0 / Length);
			const double Slope = (B.Water - A.Water) / Length;
			const bool bFalls = A.Water - B.Water > WaterfallMinDrop;
			const double Upstream = bFalls ? 0.0 : 0.15 * Length;
			const double Downstream = 0.15 * Length;
			const FVector2D From = A.Position - Dir * Upstream;
			const FVector2D To = B.Position + Dir * Downstream;
			Surfaces.push_back({(From + To) * 0.5, Dir, (Length + Upstream + Downstream) * 0.5, A.HalfWidth + DBRealm::RiverSurfaceMargin,
				A.Water + Slope * (Length + Downstream - Upstream) * 0.5, Slope});
			Kilometers += Length / 1000.0;
			if (bFalls)
			{
				++Falls;
				Tallest = std::max(Tallest, A.Water - B.Water);
			}
		}
	}

	auto Covered = [&Surfaces](const FVector2D& P, size_t Near)
	{
		const size_t First = Near > 60 ? Near - 60 : 0;
		for (size_t Other = First; Other < std::min(Surfaces.size(), Near + 60); ++Other)
		{
			const FSurface& S = Surfaces[Other];
			const FVector2D D = P - S.Mid;
			if (std::abs(FVector2D::DotProduct(D, S.Dir)) <= S.HalfLength && std::abs(D.X * -S.Dir.Y + D.Y * S.Dir.X) <= S.HalfWidth)
			{
				return true;
			}
		}
		return false;
	};
	long Samples = 0, Floating = 0, Poking = 0;
	double Worst = 0.0;
	for (size_t Index = 0; Index < Surfaces.size(); ++Index)
	{
		const FSurface& S = Surfaces[Index];
		const FVector2D Side(-S.Dir.Y, S.Dir.X);
		for (double U = -S.HalfLength * 0.75; U <= S.HalfLength * 0.75; U += 2.0)
		{
			for (const double Sign : {-1.0, 1.0})
			{
				++Samples;
				const double Water = S.MidWater + U * S.Slope;
				const FVector2D Outside = S.Mid + S.Dir * U + Side * (Sign * (S.HalfWidth + 1.0));
				if (std::abs(S.Slope) <= 0.3 && !Covered(Outside, Index))
				{
					const double Ground = DBRealm::SampleHeight(Outside.X, Outside.Y);
					if (Ground < Water - 0.3)
					{
						++Floating;
						Worst = std::max(Worst, Water - Ground);
					}
				}
				const FVector2D Inside = S.Mid + S.Dir * U + Side * (Sign * std::max(0.0, S.HalfWidth - DBRealm::RiverSurfaceMargin - 1.0));
				Poking += DBRealm::SampleHeight(Inside.X, Inside.Y) > Water ? 1 : 0;
			}
		}
	}
	printf("rivers: %.1f km of highland water in %zu surfaces, %d waterfall segments (tallest %.0f m), rising %d\n", Kilometers, Surfaces.size(),
		Falls, Tallest, Rising);
	printf("rivers: ground above the water in the channel %.3f %%, flat water edges above lower ground %.3f %% (worst %.1f m)\n",
		100.0 * Poking / std::max(Samples, 1L), 100.0 * Floating / std::max(Samples, 1L), Worst);

	if (argc > 1)
	{
		const double Step = argc > 2 ? atof(argv[2]) : 16.0;
		const int32 N = static_cast<int32>(2.0 * DBRealm::HalfSize / Step) + 1;
		std::vector<float> Heights(static_cast<size_t>(N) * N);
		for (int32 Row = 0; Row < N; ++Row)
		{
			for (int32 Column = 0; Column < N; ++Column)
			{
				Heights[static_cast<size_t>(Row) * N + Column] = static_cast<float>(DBRealm::SampleHeight(-DBRealm::HalfSize + Column * Step, -DBRealm::HalfSize + Row * Step));
			}
		}
		if (FILE* File = fopen(argv[1], "wb"))
		{
			fwrite(&N, sizeof(N), 1, File);
			fwrite(Heights.data(), sizeof(float), Heights.size(), File);
			fclose(File);
			printf("heights: %d x %d (%.0f m) -> %s\n", N, N, Step, argv[1]);
		}
	}
	const bool bOk = Rising == 0 && Poking * 1000 <= Samples && Floating * 1000 <= Samples;
	printf("%s\n", bOk ? "OK" : "FAILED");
	return bOk ? 0 : 1;
}
