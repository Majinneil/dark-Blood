#include "World/DBRealmLayout.h"

namespace
{
	// World map (1536 px wide image): the continent spans x 200..1480 px and y 0..700 px.
	FVector2D FromMap(double Px, double Py)
	{
		return FVector2D((Px - 840.0) * 12.7, (Py - 350.0) * 23.2);
	}

	double Noise(double X, double Y)
	{
		return FMath::PerlinNoise2D(FVector2D(static_cast<float>(X), static_cast<float>(Y)));
	}

	/** Fractal noise in ~[-1, 1]; Scale in meters. */
	double Fbm(double X, double Y, double Scale, int32 Octaves = 5, double Offset = 0.0)
	{
		double Total = 0.0;
		double Amplitude = 1.0;
		double Norm = 0.0;
		double Frequency = 1.0 / Scale;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			Total += Noise(X * Frequency + Offset + Octave * 17.13, Y * Frequency - Offset + Octave * 9.71) * Amplitude;
			Norm += Amplitude;
			Amplitude *= 0.5;
			Frequency *= 2.03;
		}
		return Total / Norm;
	}

	/** Ridged multifractal in ~[0, 1]: sharp crests. */
	double Ridged(double X, double Y, double Scale, int32 Octaves = 6, double Offset = 0.0)
	{
		double Total = 0.0;
		double Weight = 1.0;
		double Amplitude = 0.5;
		double Norm = 0.0;
		double Frequency = 1.0 / Scale;
		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			double Signal = 1.0 - FMath::Abs(Noise(X * Frequency + Offset + Octave * 5.3, Y * Frequency + Offset * 0.7 - Octave * 3.1));
			Signal *= Signal * Weight;
			Weight = FMath::Clamp(Signal * 1.8, 0.0, 1.0);
			Total += Signal * Amplitude;
			Norm += Amplitude;
			Frequency *= 2.1;
			Amplitude *= 0.52;
		}
		return Total / Norm;
	}

	double Smooth(double Edge0, double Edge1, double X)
	{
		const double T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	TArray<FDBRealmRegion> BuildRegions()
	{
		using B = EDBRealmBiome;
		auto R = [](const TCHAR* Id, const TCHAR* Name, B Biome, double Px, double Py, float Radius)
		{
			return FDBRealmRegion{FName(Id), Name, Biome, FromMap(Px, Py), Radius};
		};
		return {
			R(TEXT("Capital"), TEXT("Hauptstadt"), B::Capital, 640, 85, 1500.f),
			R(TEXT("Region01"), TEXT("Kirschbluetental"), B::CherryValley, 430, 165, 1800.f),
			R(TEXT("Region03"), TEXT("Bambuswaelder"), B::BambooForest, 890, 140, 1800.f),
			R(TEXT("Region04"), TEXT("Nebelberge"), B::MistMountains, 1260, 85, 2400.f),
			R(TEXT("Region11"), TEXT("Kuestenland"), B::Coast, 300, 245, 2000.f),
			R(TEXT("Region09"), TEXT("Reisfelder"), B::RiceFields, 560, 300, 1800.f),
			R(TEXT("Region08"), TEXT("Wald der Geister"), B::SpiritForest, 830, 300, 1700.f),
			R(TEXT("Region05"), TEXT("Feuergebirge"), B::FireMountains, 1100, 260, 2300.f),
			R(TEXT("Region02"), TEXT("Eisoede"), B::IceWaste, 1380, 300, 2200.f),
			R(TEXT("Region06"), TEXT("Wuestenlande"), B::Desert, 380, 460, 2300.f),
			R(TEXT("Region12"), TEXT("Himmelstempel"), B::SkyTemple, 650, 430, 1700.f),
			R(TEXT("Region13"), TEXT("Daemonenoede"), B::DemonWaste, 930, 460, 2000.f),
			R(TEXT("Region14"), TEXT("Vasallenfestung"), B::VassalFortress, 1260, 480, 1800.f),
			R(TEXT("Region10"), TEXT("Grossstadt"), B::GreatCity, 560, 570, 1800.f),
			R(TEXT("Region07"), TEXT("Flusslande"), B::Riverlands, 850, 590, 2000.f),
			R(TEXT("TheEnd"), TEXT("Das Ende"), B::TheEnd, 1270, 620, 2000.f),
		};
	}

	/** Rivers from the mountains to the sea (map pixels). */
	const TArray<TArray<FVector2D>>& GetRivers()
	{
		static const TArray<TArray<FVector2D>> Rivers = []()
		{
			const double Paths[][8][2] = {
				{{660, 130}, {600, 200}, {520, 260}, {420, 300}, {300, 330}, {170, 340}, {-1, -1}},
				{{1180, 140}, {1000, 190}, {880, 250}, {820, 340}, {840, 450}, {860, 560}, {800, 650}, {720, 730}},
				{{1150, 330}, {1230, 400}, {1330, 420}, {1520, 440}, {-1, -1}},
				{{560, 470}, {540, 560}, {480, 650}, {420, 730}, {-1, -1}},
			};
			TArray<TArray<FVector2D>> Result;
			for (const auto& Path : Paths)
			{
				TArray<FVector2D>& River = Result.AddDefaulted_GetRef();
				for (const auto& Point : Path)
				{
					if (Point[0] >= 0.0)
					{
						River.Add(FromMap(Point[0], Point[1]));
					}
				}
			}
			return Result;
		}();
		return Rivers;
	}

	double DistanceToRivers(double X, double Y)
	{
		// Meanders: the query point wanders sideways, so the straight course becomes a winding one.
		const FVector2D P(X + 260.0 * Fbm(X, Y, 900.0, 3, 51.0), Y + 260.0 * Fbm(X, Y, 900.0, 3, 52.0));
		double Best = TNumericLimits<double>::Max();
		for (const TArray<FVector2D>& River : GetRivers())
		{
			for (int32 Index = 0; Index + 1 < River.Num(); ++Index)
			{
				const FVector2D A = River[Index];
				const FVector2D B = River[Index + 1];
				const FVector2D AB = B - A;
				const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / AB.SizeSquared(), 0.0, 1.0);
				Best = FMath::Min(Best, FVector2D::Distance(P, A + AB * T));
			}
		}
		return Best;
	}

	/** Terrain character of one biome in meters (before blending, coast and rivers). */
	double BiomeHeight(EDBRealmBiome Biome, double X, double Y)
	{
		using B = EDBRealmBiome;
		switch (Biome)
		{
		case B::Capital: return 70.0 + 22.0 * Fbm(X, Y, 900.0, 4, 1.0);
		case B::CherryValley: return 26.0 + 34.0 * Fbm(X, Y, 1200.0, 5, 2.0);
		case B::BambooForest: return 48.0 + 60.0 * Fbm(X, Y, 900.0, 5, 3.0);
		case B::MistMountains: return 240.0 + 760.0 * FMath::Pow(Ridged(X, Y, 2600.0, 6, 4.0), 1.4);
		case B::Coast: return 7.0 + 22.0 * Fbm(X, Y, 700.0, 5, 5.0);
		case B::RiceFields:
		{
			// Terraces: the ground steps in 2.5 m paddies.
			const double Raw = 14.0 + 24.0 * Fbm(X, Y, 1600.0, 4, 6.0);
			const double Step = Raw / 2.5;
			return (FMath::FloorToDouble(Step) + Smooth(0.82, 1.0, FMath::Frac(Step))) * 2.5;
		}
		case B::SpiritForest: return 3.0 + 11.0 * Fbm(X, Y, 600.0, 5, 7.0);
		case B::FireMountains: return 220.0 + 640.0 * FMath::Pow(Ridged(X, Y, 2000.0, 6, 8.0), 1.3);
		case B::IceWaste: return 190.0 + 110.0 * Fbm(X, Y, 1500.0, 4, 9.0) + 220.0 * FMath::Square(Ridged(X, Y, 1800.0, 5, 10.0));
		case B::Desert:
		{
			const double Dune = FMath::Abs(FMath::Sin((X * 0.8 + Y * 0.6) / 190.0 + Fbm(X, Y, 900.0, 3, 11.0) * 3.0));
			return 30.0 + 26.0 * Dune + 22.0 * Fbm(X, Y, 2200.0, 3, 12.0);
		}
		case B::SkyTemple: return 1.0 + 12.0 * Fbm(X, Y, 800.0, 4, 13.0);
		case B::DemonWaste: return 60.0 + 150.0 * FMath::Pow(Ridged(X, Y, 900.0, 6, 14.0), 1.5);
		case B::VassalFortress: return 95.0 + 45.0 * Fbm(X, Y, 1000.0, 4, 15.0);
		case B::GreatCity: return 15.0 + 9.0 * Fbm(X, Y, 1500.0, 4, 16.0);
		case B::Riverlands: return 3.5 + 8.0 * Fbm(X, Y, 900.0, 5, 17.0);
		case B::TheEnd: return 150.0 + 460.0 * FMath::Pow(Ridged(X, Y, 1100.0, 6, 18.0), 1.8);
		default: return 10.0;
		}
	}

	/** Base paint of a biome: Meadow, Forest, Rock, Snow, Sand, Soil, Corrupt, Lava. */
	void BiomeLayers(EDBRealmBiome Biome, float Out[8])
	{
		using B = EDBRealmBiome;
		FMemory::Memzero(Out, sizeof(float) * 8);
		switch (Biome)
		{
		case B::Capital: Out[0] = 0.6f; Out[5] = 0.4f; break;
		case B::CherryValley: Out[0] = 0.85f; Out[5] = 0.15f; break;
		case B::BambooForest: Out[1] = 0.8f; Out[0] = 0.2f; break;
		case B::MistMountains: Out[2] = 0.6f; Out[0] = 0.2f; Out[1] = 0.2f; break;
		case B::Coast: Out[0] = 0.65f; Out[4] = 0.35f; break;
		case B::RiceFields: Out[0] = 0.7f; Out[5] = 0.3f; break;
		case B::SpiritForest: Out[1] = 0.9f; Out[5] = 0.1f; break;
		case B::FireMountains: Out[7] = 0.15f; Out[2] = 0.85f; break;
		case B::IceWaste: Out[3] = 0.8f; Out[2] = 0.2f; break;
		case B::Desert: Out[4] = 1.f; break;
		case B::SkyTemple: Out[0] = 0.8f; Out[2] = 0.2f; break;
		case B::DemonWaste: Out[6] = 0.8f; Out[2] = 0.2f; break;
		case B::VassalFortress: Out[5] = 0.5f; Out[2] = 0.5f; break;
		case B::GreatCity: Out[0] = 0.5f; Out[5] = 0.5f; break;
		case B::Riverlands: Out[0] = 0.7f; Out[1] = 0.3f; break;
		case B::TheEnd: Out[6] = 0.6f; Out[7] = 0.1f; Out[2] = 0.3f; break;
		default: Out[0] = 1.f; break;
		}
	}

	/** Organic region borders: positions are warped by large-scale noise before measuring distances. */
	FVector2D Warp(double X, double Y)
	{
		return FVector2D(X + 2600.0 * Fbm(X, Y, 3000.0, 3, 41.0) + 700.0 * Fbm(X, Y, 900.0, 2, 43.0),
			Y + 2600.0 * Fbm(X, Y, 3000.0, 3, 42.0) + 700.0 * Fbm(X, Y, 900.0, 2, 44.0));
	}

	/** Influence of each region (0 outside ~1.8 radii). */
	double RegionWeight(const FDBRealmRegion& Region, double X, double Y)
	{
		const double Distance = FVector2D::Distance(Warp(X, Y), Region.Center);
		const double T = FMath::Max(0.0, 1.0 - Distance / (Region.Radius * 1.8));
		return T * T * T;
	}
}

namespace DBRealm
{
	double SampleRawHeight(double X, double Y);

	const TArray<FDBRealmRegion>& GetRegions()
	{
		static const TArray<FDBRealmRegion> Regions = BuildRegions();
		return Regions;
	}

	const FDBRealmRegion& GetCapital()
	{
		return GetRegions()[0];
	}

	const TArray<FDBRealmSettlement>& GetSettlements()
	{
		static const TArray<FDBRealmSettlement> Settlements = []()
		{
			using S = EDBSettlementType;
			const TArray<FDBRealmRegion>& Regions = GetRegions();
			auto Center = [&Regions](FName Id) { return Regions.FindByPredicate([Id](const FDBRealmRegion& R) { return R.RegionId == Id; })->Center; };
			// Leveled height: the raw terrain at the center, never below 3 m (harbors sit just above the water).
			auto Site = [](const TCHAR* Name, S Type, const FVector2D& At, float Radius)
			{
				return FDBRealmSettlement{Name, Type, At, Radius, FMath::Max(3.0, SampleRawHeight(At.X, At.Y))};
			};
			// Coastal site: walk from a point in a direction until the land ends, then step back inland.
			auto Coastal = [&Site](const TCHAR* Name, S Type, const FVector2D& From, const FVector2D& Direction, double Inland, float Radius)
			{
				FVector2D Point = From;
				for (int32 Step = 0; Step < 400 && SampleRawHeight(Point.X, Point.Y) > 0.5; ++Step)
				{
					Point += Direction * 25.0;
				}
				FDBRealmSettlement Result = Site(Name, Type, Point - Direction * Inland, Radius);
				Result.GroundHeight = 3.0;
				Result.SeaDirection = Direction;
				Result.ShoreDistance = Inland;
				return Result;
			};
			const FVector2D Capital = Center(TEXT("Capital"));
			return TArray<FDBRealmSettlement>{
				// The capital districts lie west of the slice courtyard (the slice spans about -10..120 m east of the start).
				Site(TEXT("Hauptstadt"), S::Capital, Capital + FVector2D(-330.0, 40.0), 230.f),
				// The great harbor of the capital: the nearest coast north of the plateau.
				Coastal(TEXT("Hauptstadthafen"), S::HarborTown, Capital + FVector2D(0.0, -450.0), FVector2D(0.0, -1.0), 200.0, 320.f),
				Site(TEXT("Grossstadt"), S::GreatCity, Center(TEXT("Region10")), 380.f),
				Site(TEXT("Dorf"), S::Village, Center(TEXT("Region01")) + FVector2D(250.0, 300.0), 170.f),
				Site(TEXT("Bergdorf"), S::MountainVillage, Center(TEXT("Region04")) + FVector2D(-1900.0, 1300.0), 150.f),
				Coastal(TEXT("Hafenstadt"), S::HarborTown, Center(TEXT("Region11")), FVector2D(-1.0, 0.0), 160.0, 220.f),
				Site(TEXT("Reisdorf"), S::RiceVillage, Center(TEXT("Region09")) + FVector2D(0.0, -300.0), 170.f),
				Site(TEXT("Waldsiedlung"), S::ForestSettlement, Center(TEXT("Region03")) + FVector2D(-300.0, 400.0), 150.f),
				Site(TEXT("Bergwerksstadt"), S::MiningTown, Center(TEXT("Region05")) + FVector2D(-1500.0, 900.0), 190.f),
				Site(TEXT("Tempelsiedlung"), S::TempleSettlement, Center(TEXT("Region12")) + FVector2D(300.0, -200.0), 190.f),
				Site(TEXT("Grenzposten"), S::BorderOutpost, Center(TEXT("Region14")) + FVector2D(-1400.0, -300.0), 120.f),
				Site(TEXT("Karawanenstadt"), S::CaravanTown, Center(TEXT("Region06")) + FVector2D(1300.0, -900.0), 200.f),
				Site(TEXT("Tavernenstadt"), S::TavernTown, (Center(TEXT("Region09")) + Center(TEXT("Region08"))) * 0.5 + FVector2D(0.0, 500.0), 180.f),
				Coastal(TEXT("Fischerdorf"), S::FishingVillage, Center(TEXT("Region07")), FVector2D(0.0, 1.0), 110.0, 150.f),
				Site(TEXT("Schneesiedlung"), S::SnowSettlement, Center(TEXT("Region02")) + FVector2D(-500.0, 600.0), 160.f),
				Site(TEXT("Flusssiedlung"), S::RiverSettlement, Center(TEXT("Region07")) + FVector2D(-500.0, -700.0), 170.f),
				Site(TEXT("Oasenstadt"), S::OasisTown, Center(TEXT("Region06")), 200.f),
			};
		}();
		return Settlements;
	}

	double SampleHeight(double X, double Y)
	{
		double Height = SampleRawHeight(X, Y);
		// Settlements stand on leveled ground that blends into the terrain around them.
		for (const FDBRealmSettlement& Site : GetSettlements())
		{
			const double Distance = FVector2D::Distance(FVector2D(X, Y), Site.Center);
			if (Distance < Site.Radius * 2.2)
			{
				// Coastal sites keep their water: only land (and the shallows up to the quay) is leveled.
				const double Seaward = FVector2D::DotProduct(FVector2D(X, Y) - Site.Center, Site.SeaDirection);
				if (!Site.SeaDirection.IsZero() && Seaward > Site.ShoreDistance - 15.0)
				{
					Height = FMath::Min(Height, FMath::Lerp(-6.0, Height, Smooth(Site.ShoreDistance + 60.0, Site.ShoreDistance - 15.0, Seaward)));
					continue;
				}
				Height = FMath::Lerp(Site.GroundHeight, Height, Smooth(Site.Radius, Site.Radius * 2.2, Distance));
			}
		}
		return Height;
	}

	double SampleRawHeight(double X, double Y)
	{
		const TArray<FDBRealmRegion>& Regions = GetRegions();
		double Sum = 0.0;
		double Height = 0.0;
		double Coverage = -1.0;
		for (const FDBRealmRegion& Region : Regions)
		{
			const double Weight = RegionWeight(Region, X, Y);
			if (Weight > 1e-4)
			{
				Sum += Weight;
				Height += Weight * BiomeHeight(Region.Biome, X, Y);
			}
			Coverage = FMath::Max(Coverage, 1.0 - FVector2D::Distance(Warp(X, Y), Region.Center) / (Region.Radius * 1.65));
		}
		Height = Sum > 0.0 ? Height / Sum : 5.0;

		// The capital stands on a flat plateau (the story start and the visual slice are built there).
		const double CapitalDistance = FVector2D::Distance(FVector2D(X, Y), GetCapital().Center);
		Height = FMath::Lerp(70.0, Height, Smooth(CapitalFlatRadius, CapitalFlatRadius * 2.4, CapitalDistance));

		// Rivers: a bed below sea level in the lowlands (the sea plane fills it); in high land a gorge 80 m deep.
		const double River = DistanceToRivers(X, Y) + 25.0 * Fbm(X, Y, 300.0, 3, 21.0);
		const double Bed = FMath::Max(-3.0, Height - 80.0);
		Height = FMath::Lerp(Bed, Height, Smooth(18.0, 190.0, River));

		// Coast: a ragged island in the sea with bays and headlands, open water along the realm border.
		Coverage += 0.28 * Fbm(X, Y, 1800.0, 4, 22.0) + 0.14 * Fbm(X, Y, 500.0, 3, 23.0);
		// Bays and inlets as on the world map (map pixels, radius in meters).
		static const double Bays[][3] = {{320, 300, 1500}, {440, 650, 1100}, {720, 650, 950}, {1400, 420, 1000}, {400, 60, 1000}, {1020, 660, 800}, {1450, 170, 800}};
		for (const auto& Bay : Bays)
		{
			// Ragged shores: the distance is disturbed by noise so no bay is a circle.
			const double Distance = FVector2D::Distance(FVector2D(X, Y), FromMap(Bay[0], Bay[1])) * (1.0 + 0.55 * Fbm(X, Y, 700.0, 4, 45.0));
			Coverage = FMath::Min(Coverage, Distance / Bay[2] - 0.6);
		}
		const double Border = FMath::Min(HalfSize - FMath::Abs(X), HalfSize - FMath::Abs(Y));
		Coverage = FMath::Min(Coverage, Border / 2600.0 - 0.3 + 0.2 * Fbm(X, Y, 1200.0, 3, 24.0));
		const double Land = Smooth(0.0, 0.22, Coverage);
		return FMath::Lerp(-38.0, Height, Land);
	}

	int32 FindRegionIndex(double X, double Y)
	{
		const TArray<FDBRealmRegion>& Regions = GetRegions();
		int32 Best = 0;
		double BestScore = -1.0;
		for (int32 Index = 0; Index < Regions.Num(); ++Index)
		{
			// Normalized distance: bigger regions reach further.
			const double Score = 1.0 - FVector2D::Distance(Warp(X, Y), Regions[Index].Center) / Regions[Index].Radius;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Index;
			}
		}
		return Best;
	}

	void SampleLayers(double X, double Y, double Height, double NormalZ, uint8 OutWeights[static_cast<int32>(EDBRealmLayer::Count)])
	{
		constexpr int32 Count = static_cast<int32>(EDBRealmLayer::Count);
		float Mix[Count] = {};
		double Sum = 0.0;
		bool bHot = false;
		for (const FDBRealmRegion& Region : GetRegions())
		{
			const double Weight = RegionWeight(Region, X, Y);
			if (Weight <= 1e-4)
			{
				continue;
			}
			float Layers[Count];
			BiomeLayers(Region.Biome, Layers);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Mix[Index] += static_cast<float>(Weight) * Layers[Index];
			}
			Sum += Weight;
			bHot |= Weight > 0.05 && (Region.Biome == EDBRealmBiome::FireMountains || Region.Biome == EDBRealmBiome::TheEnd
										 || Region.Biome == EDBRealmBiome::DemonWaste || Region.Biome == EDBRealmBiome::Desert);
		}
		if (Sum <= 0.0)
		{
			Mix[static_cast<int32>(EDBRealmLayer::Sand)] = 1.f;
			Sum = 1.0;
		}
		for (float& Value : Mix)
		{
			Value /= static_cast<float>(Sum);
		}
		// Variation inside a region (patches of the second layer instead of a uniform blend).
		const float Patch = static_cast<float>(Smooth(-0.25, 0.25, Fbm(X, Y, 180.0, 3, 31.0)));
		Mix[0] *= 0.6f + 0.8f * Patch;
		Mix[5] *= 1.4f - 0.8f * Patch;
		// Woods scattered through the green land (the map shows forest everywhere between the fields).
		const float Woods = static_cast<float>(Smooth(0.05, 0.3, Fbm(X, Y, 1100.0, 4, 33.0)));
		Mix[1] += Mix[0] * 0.9f * Woods;
		Mix[0] *= 1.f - 0.9f * Woods;
		// Lava streams through the volcanic rock.
		const float Streams = static_cast<float>(Smooth(0.86, 0.95, 1.0 - FMath::Abs(Fbm(X, Y, 700.0, 3, 34.0))));
		Mix[7] += Mix[2] * Streams * (Mix[7] > 0.02f ? 1.5f : 0.f);

		auto Override = [&](EDBRealmLayer Layer, double Amount)
		{
			const float A = static_cast<float>(FMath::Clamp(Amount, 0.0, 1.0));
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Mix[Index] *= 1.f - A;
			}
			Mix[static_cast<int32>(Layer)] += A;
		};
		Override(EDBRealmLayer::Rock, Smooth(0.82, 0.62, NormalZ));
		if (!bHot)
		{
			Override(EDBRealmLayer::Snow, Smooth(520.0, 680.0, Height + 60.0 * Fbm(X, Y, 250.0, 3, 32.0)) * Smooth(0.55, 0.75, NormalZ));
		}
		Override(EDBRealmLayer::Sand, Smooth(2.8, 0.8, Height) * (Height > -8.0 ? 1.0 : 0.6));

		float Total = 0.f;
		for (const float Value : Mix)
		{
			Total += FMath::Max(Value, 0.f);
		}
		int32 Assigned = 0;
		int32 Largest = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutWeights[Index] = static_cast<uint8>(FMath::RoundToInt(255.f * FMath::Max(Mix[Index], 0.f) / FMath::Max(Total, 1e-4f)));
			Assigned += OutWeights[Index];
			Largest = OutWeights[Index] > OutWeights[Largest] ? Index : Largest;
		}
		OutWeights[Largest] = static_cast<uint8>(FMath::Clamp(OutWeights[Largest] + (255 - Assigned), 0, 255));
	}

	const TCHAR* GetLayerName(EDBRealmLayer Layer)
	{
		static const TCHAR* Names[] = {TEXT("Meadow"), TEXT("Forest"), TEXT("Rock"), TEXT("Snow"), TEXT("Sand"), TEXT("Soil"), TEXT("Corrupt"), TEXT("Lava")};
		return Names[FMath::Clamp(static_cast<int32>(Layer), 0, 7)];
	}
}
