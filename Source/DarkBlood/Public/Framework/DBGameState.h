// Replicated game state: shared world state and shared (story) quest log.
#pragma once

#include "GameFramework/GameStateBase.h"

#include "DBGameState.generated.h"

class UDBQuestComponent;
class UDBWorldStateComponent;

UCLASS()
class DARKBLOOD_API ADBGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADBGameState();

	UFUNCTION(BlueprintPure, Category = "Dark Blood")
	UDBWorldStateComponent* GetWorldState() const { return WorldState; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood")
	UDBQuestComponent* GetSharedQuests() const { return SharedQuests; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Dark Blood")
	TObjectPtr<UDBWorldStateComponent> WorldState;

	UPROPERTY(VisibleAnywhere, Category = "Dark Blood")
	TObjectPtr<UDBQuestComponent> SharedQuests;
};
