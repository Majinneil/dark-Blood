// Server-side damage execution. All math is delegated to DarkBlood::Rules::ResolveDamage.
// Usage: a gameplay effect with this execution + SetByCaller.Damage magnitude + a Damage.Type.* asset tag.
#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "DBDamageExecution.generated.h"

UCLASS()
class DARKBLOOD_API UDBDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UDBDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
