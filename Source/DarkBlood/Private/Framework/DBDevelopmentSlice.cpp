#include "Framework/DBDevelopmentSlice.h"

#include "Character/DBLesserDemon.h"
#include "Character/DBNpcCharacter.h"
#include "Character/DBTrainingDummy.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/DBEncounterSpawner.h"

namespace DBDevelopmentSlice
{
	namespace
	{
		template <typename T>
		T* SpawnAt(UWorld* World, const FTransform& Origin, const FVector& Offset, bool bFaceOrigin = true)
		{
			const FVector Location = Origin.TransformPosition(Offset);
			const FRotator Rotation = bFaceOrigin ? FRotator(0.f, (Origin.GetLocation() - Location).Rotation().Yaw, 0.f) : Origin.Rotator();
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			return World->SpawnActor<T>(T::StaticClass(), Location, Rotation, Params);
		}
	}

	void Spawn(UWorld* World, const FTransform& Origin)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return;
		}
		for (TActorIterator<ADBNpcCharacter> It(World); It; ++It)
		{
			if (It->GetNpcId() == TEXT("NPC_King"))
			{
				return; // already set up
			}
		}

		// Castle courtyard: the king and three training dummies.
		if (ADBNpcCharacter* King = SpawnAt<ADBNpcCharacter>(World, Origin, FVector(450.f, 0.f, 0.f)))
		{
			King->Setup(TEXT("NPC_King"), FText::FromString(TEXT("Koenig Aoki [DEV]")), TEXT("Dlg_King"));
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			SpawnAt<ADBTrainingDummy>(World, Origin, FVector(350.f, 550.f + 200.f * static_cast<float>(Index), 0.f));
		}

		// East gate: the captain and the first demon encounter (appears once MQ02 is active).
		if (ADBNpcCharacter* Captain = SpawnAt<ADBNpcCharacter>(World, Origin, FVector(2200.f, -700.f, 0.f)))
		{
			Captain->Setup(TEXT("NPC_Captain"), FText::FromString(TEXT("Hauptmann Kenji [DEV]")), TEXT("Dlg_Captain"));
		}
		if (ADBEncounterSpawner* Spawner = SpawnAt<ADBEncounterSpawner>(World, Origin, FVector(3000.f, -700.f, 0.f)))
		{
			Spawner->Setup(ADBLesserDemon::StaticClass(), 3, TEXT("MQ02_EastGate"), TEXT("Story.CaptainBriefed"));
		}
		UE_LOG(LogDarkBlood, Log, TEXT("DEVELOPMENT story slice spawned"));
	}
}
