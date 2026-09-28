// Placement helpers of the procedural art kit: spawns the local, non-replicated dressing actors (buildings, paths,
// lanterns, props, scatter, particles, fog) in a local frame. Used by the visual slice and the open-world settlements;
// everything it builds is deterministic, so every machine builds the same world from the same inputs.
#pragma once

#include "Art/DBArtBatcher.h"
#include "Art/DBArtMaterials.h"
#include "Art/DBModularBuilding.h"
#include "Art/DBModelLibrary.h"
#include "Art/DBSetDressing.h"
#include "Components/BoxComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

namespace DBArtBuild
{
	/** Actors whose lights burn day and night (demon lands); every other slice light only at dusk / night. */
	const FName AlwaysLitTag(TEXT("DBAlwaysLit"));

	/** Everything the slice spawns, in the slice's local frame (X = forward from the player start). */
	struct FArtBuilder
	{
		UWorld& World;
		FTransform Origin;
		TArray<TObjectPtr<AActor>>& Out;

		FTransform At(const FVector& Local, float Yaw = 0.f) const
		{
			return FTransform(FRotator(0.f, Yaw, 0.f), Local) * Origin;
		}

		template <typename T>
		T* Begin(const FVector& Local, float Yaw = 0.f)
		{
			FActorSpawnParameters Params;
			Params.bDeferConstruction = true;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			T* Actor = World.SpawnActor<T>(T::StaticClass(), At(Local, Yaw), Params);
			if (Actor)
			{
				Actor->SetReplicates(false);
				Out.Add(Actor);
			}
			return Actor;
		}

		template <typename T>
		T* Finish(T* Actor, const FVector& Local, float Yaw = 0.f)
		{
			if (Actor)
			{
				Actor->FinishSpawning(At(Local, Yaw));
			}
			return Actor;
		}

		ADBModularBuilding* Building(const FVector& Local, float Yaw, EDBBuildingType Type, int32 Seed, FName Region, float Wealth, int32 X, int32 Y,
			int32 Floors = 1, float Damage = 0.f)
		{
			ADBModularBuilding* Actor = Begin<ADBModularBuilding>(Local, Yaw);
			if (Actor)
			{
				Actor->Configure(Type, Seed, Region, Wealth, X, Y, Floors, Damage);
			}
			return Finish(Actor, Local, Yaw);
		}

		ADBGroundPatch* Ground(const FVector& Local, const FVector2D& Size, EDBArtMaterial Material, float Layer, float TileSize = 0.f, bool bRound = false,
			float Yaw = 0.f)
		{
			ADBGroundPatch* Actor = Begin<ADBGroundPatch>(Local, Yaw);
			if (Actor)
			{
				Actor->Size = Size;
				Actor->Material = Material;
				Actor->Layer = Layer;
				Actor->TileSize = TileSize;
				Actor->bRound = bRound;
				Actor->Seed = FMath::RoundToInt(Local.X + Local.Y);
			}
			return Finish(Actor, Local, Yaw);
		}

		ADBSplineDressing* Spline(EDBSplineDressing Type, const TArray<FVector>& Points, float Width, int32 Seed)
		{
			ADBSplineDressing* Actor = Begin<ADBSplineDressing>(Points[0]);
			if (Actor)
			{
				Actor->Type = Type;
				Actor->Width = Width;
				Actor->Seed = Seed;
				TArray<FVector> Relative;
				for (const FVector& Point : Points)
				{
					Relative.Add(Point - Points[0]);
				}
				Actor->GetSpline()->SetSplinePoints(Relative, ESplineCoordinateSpace::Local, true);
			}
			return Finish(Actor, Points[0]);
		}

		ADBLantern* Lantern(const FVector& Local, EDBLanternStyle Style, float Yaw = 0.f)
		{
			ADBLantern* Actor = Begin<ADBLantern>(Local, Yaw);
			if (Actor)
			{
				Actor->Style = Style;
				Actor->StoneMaterial = Style == EDBLanternStyle::Stone ? EDBArtMaterial::StoneTemple : EDBArtMaterial::StoneMossy;
			}
			return Finish(Actor, Local, Yaw);
		}

		/** Free authored model from the library (DBModelLibrary) at its library size, or Height; null when not imported. */
		ADBPropActor* Model(const FVector& Local, float Yaw, const TCHAR* Key, float Height = 0.f, bool bCollision = true, float Lumens = 0.f,
			const FVector& LightOffset = FVector::ZeroVector)
		{
			const DBModels::FModelInfo* Info = DBModels::Find(Key);
			const TArray<FString>& Parts = DBModels::GetParts(Key);
			if (!Info || Parts.IsEmpty())
			{
				return nullptr;
			}
			TArray<const TCHAR*> PartPtrs;
			for (const FString& Part : Parts)
			{
				PartPtrs.Add(*Part);
			}
			const float Size = Height > 0.f ? Height : Info->Height;
			return Prop(Local - FVector(0.f, 0.f, Size * Info->Sink), Yaw, PartPtrs, Size, FRotator(0.f, Info->Yaw, 0.f), Lumens, LightOffset, bCollision);
		}

		/** Authored (Fab CC BY) model; returns null when it is not imported so callers can fall back. */
		ADBPropActor* Prop(const FVector& Local, float Yaw, const TArray<const TCHAR*>& Parts, float Height, const FRotator& ModelRotation = FRotator::ZeroRotator,
			float Lumens = 0.f, const FVector& LightOffset = FVector::ZeroVector, bool bCollision = true)
		{
			ADBPropActor* Actor = Begin<ADBPropActor>(Local, Yaw);
			if (!Actor)
			{
				return nullptr;
			}
			for (const TCHAR* Part : Parts)
			{
				const FString Path(Part);
				Actor->Parts.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path + TEXT(".") + FPaths::GetBaseFilename(Path))));
			}
			Actor->TargetHeight = Height;
			Actor->ModelRotation = ModelRotation;
			Actor->LightLumens = Lumens;
			Actor->LightOffset = LightOffset;
			Actor->bCollision = bCollision;
			Finish(Actor, Local, Yaw);
			if (!Actor->HasModel())
			{
				Out.Remove(Actor);
				Actor->Destroy();
				return nullptr;
			}
			return Actor;
		}

		ADBScatterVolume* Scatter(const FVector& Local, const FVector& Extent, EDBBiome Biome, float Density, float Undergrowth, int32 Seed, float Grass = 0.f)
		{
			ADBScatterVolume* Actor = Begin<ADBScatterVolume>(Local);
			if (Actor)
			{
				Actor->Biome = Biome;
				Actor->Density = Density;
				Actor->UndergrowthDensity = Undergrowth;
				Actor->GrassDensity = Grass;
				Actor->Seed = Seed;
				Actor->GetArea()->SetBoxExtent(Extent);
			}
			return Finish(Actor, Local);
		}

		ADBAmbientFx* Fx(EDBAmbientFx Kind, const FVector& Local, const FVector& Extent, int32 Count, int32 Seed, bool bNightOnly = false)
		{
			ADBAmbientFx* Actor = Begin<ADBAmbientFx>(Local);
			if (Actor)
			{
				Actor->Kind = Kind;
				Actor->Extent = Extent;
				Actor->Count = Count;
				Actor->Seed = Seed;
				Actor->bNightOnly = bNightOnly;
			}
			return Finish(Actor, Local);
		}

		/** Authored model with every material replaced (e.g. vein-lit corrupted wood) and an optional colored light. */
		ADBPropActor* CorruptedProp(const FVector& Local, float Yaw, const TCHAR* Part, float Height, EDBArtMaterial Material, float Lumens = 0.f,
			const FRotator& ModelRotation = FRotator::ZeroRotator)
		{
			ADBPropActor* Actor = Begin<ADBPropActor>(Local, Yaw);
			if (!Actor)
			{
				return nullptr;
			}
			const FString Path(Part);
			Actor->Parts.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path + TEXT(".") + FPaths::GetBaseFilename(Path))));
			Actor->TargetHeight = Height;
			Actor->ModelRotation = ModelRotation;
			Actor->MaterialOverride = Material;
			Actor->LightLumens = Lumens;
			Actor->LightOffset = FVector(0.f, 0.f, Height * 0.25f);
			Actor->LightColor = FLinearColor(1.f, 0.06f, 0.03f);
			Actor->Tags.Add(AlwaysLitTag);
			return Finish(Actor, Local, Yaw);
		}

		/** Local fog volume (sphere), e.g. the red haze over the demon lands. */
		void Fog(const FVector& Local, float Radius, float Density, const FLinearColor& Albedo, const FLinearColor& Emissive)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			const float Scale = Radius / ULocalFogVolumeComponent::GetBaseVolumeSize();
			ALocalFogVolume* Volume = World.SpawnActor<ALocalFogVolume>(ALocalFogVolume::StaticClass(), At(Local), Params);
			if (!Volume)
			{
				return;
			}
			Volume->SetReplicates(false);
			Volume->SetActorScale3D(FVector(Scale, Scale, Scale * 0.35f));
			ULocalFogVolumeComponent* Component = Volume->GetComponent();
			Component->SetRadialFogExtinction(Density);
			Component->SetHeightFogExtinction(Density);
			Component->SetHeightFogFalloff(600.f);
			Component->SetFogAlbedo(Albedo);
			Component->SetFogEmissive(Emissive);
			Out.Add(Volume);
		}

		/** Generated horizon terrain (Tools/UE58/generate_backdrop_terrain.py), placed as authored around Center. */
		void Backdrop(const FVector& Center, const TCHAR* MeshName)
		{
			if (ADBPropActor* Actor = Begin<ADBPropActor>(Center))
			{
				const FString Path = FString::Printf(TEXT("/Game/DarkBlood/Art/Environment/Backdrop/%s.%s"), MeshName, MeshName);
				Actor->Parts.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path)));
				Actor->bKeepPivot = true;
				Actor->bCollision = false;
				Finish(Actor, Center);
			}
		}
	};

}
