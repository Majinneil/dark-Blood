// Spawns a group of enemies once its story condition is met (quest active / story flag set). Placed in the
// world by designers; the development slice spawns one in code. Server only.
#pragma once

#include "GameFramework/Actor.h"

#include "DBEncounterSpawner.generated.h"

class ADBEnemyCharacter;

UCLASS()
class DARKBLOOD_API ADBEncounterSpawner : public AActor
{
	GENERATED_BODY()

public:
	ADBEncounterSpawner();

	virtual void Tick(float DeltaSeconds) override;

	void Setup(TSubclassOf<ADBEnemyCharacter> InEnemyClass, int32 InCount, FName InRequiredActiveQuest, FName InRequiredStoryFlag);

	bool HasSpawned() const { return bSpawned; }

protected:
	bool IsConditionMet() const;
	void SpawnEncounter();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Encounter")
	TSubclassOf<ADBEnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Encounter", meta = (ClampMin = 1))
	int32 Count = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Encounter", meta = (Units = "cm"))
	float SpawnRadius = 400.f;

	/** Spawn once this quest is active in the shared (story) log. None = no quest condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Encounter")
	FName RequiredActiveQuest;

	/** Spawn once this story flag is set. None = no flag condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Encounter")
	FName RequiredStoryFlag;

private:
	bool bSpawned = false;
	float CheckTimer = 0.f;
};
