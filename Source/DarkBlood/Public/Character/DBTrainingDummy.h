// DEVELOPMENT training dummy: stationary, respawns in place, counts for "Kill TrainingDummy" objectives.
// Can swing at the nearest player on demand (or periodically) to practise block, parry and dodge.
#pragma once

#include "Character/DBEnemyCharacter.h"

#include "DBTrainingDummy.generated.h"

UCLASS()
class DARKBLOOD_API ADBTrainingDummy : public ADBEnemyCharacter
{
	GENERATED_BODY()

public:
	ADBTrainingDummy(const FObjectInitializer& ObjectInitializer);

	/** Server: swing at the nearest player in reach. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Training")
	bool SwingAtNearestPlayer();

	/** Server: swing every Interval seconds while a player is in reach (0 = off). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Training")
	void SetAutoAttack(float IntervalSeconds);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Training", meta = (ClampMin = 0, Units = "s"))
	float AutoAttackInterval = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Training", meta = (ClampMin = 0, Units = "cm"))
	float AttackReach = 260.f;

private:
	FTimerHandle AutoAttackTimer;
};
