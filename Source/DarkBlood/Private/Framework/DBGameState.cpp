#include "Framework/DBGameState.h"

#include "Quest/DBQuestComponent.h"
#include "World/DBWorldStateComponent.h"

ADBGameState::ADBGameState()
{
	WorldState = CreateDefaultSubobject<UDBWorldStateComponent>(TEXT("WorldState"));
	SharedQuests = CreateDefaultSubobject<UDBQuestComponent>(TEXT("SharedQuests"));
}
