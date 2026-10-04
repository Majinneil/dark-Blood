// DEVELOPMENT enemy: a lesser demon with the simple melee AI. First real opponent of the combat slice.
#pragma once

#include "Character/DBEnemyCharacter.h"

#include "DBLesserDemon.generated.h"

class UDBMeleeAIComponent;

UCLASS()
class DARKBLOOD_API ADBLesserDemon : public ADBEnemyCharacter
{
	GENERATED_BODY()

public:
	ADBLesserDemon(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dark Blood|AI")
	TObjectPtr<UDBMeleeAIComponent> MeleeAI;
};
