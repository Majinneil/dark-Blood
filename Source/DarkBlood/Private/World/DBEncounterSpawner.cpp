#include "World/DBEncounterSpawner.h"

#include "Character/DBEnemyCharacter.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "Framework/DBGameState.h"
#include "Quest/DBQuestComponent.h"
#include "World/DBWorldStateComponent.h"

ADBEncounterSpawner::ADBEncounterSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	bReplicates = false; // server-side logic only; the spawned enemies replicate themselves
}

void ADBEncounterSpawner::Setup(TSubclassOf<ADBEnemyCharacter> InEnemyClass, int32 InCount, FName InRequiredActiveQuest, FName InRequiredStoryFlag)
{
	EnemyClass = InEnemyClass;
	Count = FMath::Max(1, InCount);
	RequiredActiveQuest = InRequiredActiveQuest;
	RequiredStoryFlag = InRequiredStoryFlag;
}

bool ADBEncounterSpawner::IsConditionMet() const
{
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	if (!GameState)
	{
		return false;
	}
	if (!RequiredActiveQuest.IsNone())
	{
		const UDBQuestComponent* Shared = GameState->GetSharedQuests();
		if (!Shared || Shared->GetQuestStatus(RequiredActiveQuest) != EDBQuestStatus::Active)
		{
			return false;
		}
	}
	if (!RequiredStoryFlag.IsNone())
	{
		const UDBWorldStateComponent* WorldState = GameState->GetWorldState();
		if (!WorldState || !WorldState->HasStoryFlag(RequiredStoryFlag))
		{
			return false;
		}
	}
	return true;
}

void ADBEncounterSpawner::SpawnEncounter()
{
	bSpawned = true;
	if (!EnemyClass)
	{
		return;
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(Count);
		const FVector Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * SpawnRadius;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		GetWorld()->SpawnActor<ADBEnemyCharacter>(EnemyClass, Location, GetActorRotation(), Params);
	}
	UE_LOG(LogDBCombat, Log, TEXT("Encounter %s: spawned %d x %s"), *GetName(), Count, *EnemyClass->GetName());
}

void ADBEncounterSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bSpawned)
	{
		return;
	}
	CheckTimer -= DeltaSeconds;
	if (CheckTimer <= 0.f)
	{
		CheckTimer = 0.5f;
		if (IsConditionMet())
		{
			SpawnEncounter();
		}
	}
}
