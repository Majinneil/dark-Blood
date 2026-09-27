#include "Abilities/DBDamageExecution.h"

#include "Abilities/DBAttributeSet.h"
#include "Core/DBGameplayTags.h"

#include "DarkBloodRules/Damage.h"

namespace
{
	struct FDBDamageStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
		DECLARE_ATTRIBUTE_CAPTUREDEF(SpellPower);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance);
		DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);

		FDBDamageStatics()
		{
			DEFINE_ATTRIBUTE_CAPTUREDEF(UDBAttributeSet, AttackPower, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UDBAttributeSet, SpellPower, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UDBAttributeSet, CritChance, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UDBAttributeSet, Armor, Target, false);
		}
	};

	const FDBDamageStatics& DamageStatics()
	{
		static FDBDamageStatics Statics;
		return Statics;
	}

	DarkBlood::Rules::EDamageType ResolveDamageType(const FGameplayTagContainer& Tags)
	{
		using DarkBlood::Rules::EDamageType;
		struct FMapping
		{
			FGameplayTag Tag;
			EDamageType Type;
		};
		const FMapping Mappings[] = {
			{DBTags::Damage_Type_Fire, EDamageType::Fire},
			{DBTags::Damage_Type_Frost, EDamageType::Frost},
			{DBTags::Damage_Type_Lightning, EDamageType::Lightning},
			{DBTags::Damage_Type_Shadow, EDamageType::Shadow},
			{DBTags::Damage_Type_Poison, EDamageType::Poison},
			{DBTags::Damage_Type_Spirit, EDamageType::Spirit},
			{DBTags::Damage_Type_DarkBlood, EDamageType::DarkBlood},
		};
		for (const FMapping& Mapping : Mappings)
		{
			if (Tags.HasTagExact(Mapping.Tag))
			{
				return Mapping.Type;
			}
		}
		return EDamageType::Physical;
	}
}

UDBDamageExecution::UDBDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().SpellPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
}

void UDBDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	namespace R = DarkBlood::Rules;

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	FGameplayTagContainer SpecTags;
	Spec.GetAllAssetTags(SpecTags);

	R::FDamageRequest Request;
	Request.BaseDamage = Spec.GetSetByCallerMagnitude(DBTags::SetByCaller_Damage, false, 0.f);
	Request.Type = ResolveDamageType(SpecTags);
	Request.AttackerLevel = FMath::Max(1, FMath::RoundToInt(Spec.GetLevel()));
	Request.bCanBeBlocked = !SpecTags.HasTagExact(DBTags::Damage_Unblockable);
	Request.bCanBeParried = !SpecTags.HasTagExact(DBTags::Damage_Unparryable);
	// Executions only run with authority, so the roll is server-side.
	Request.CritRoll = FMath::FRand();

	float Power = 0.f;
	const FGameplayEffectAttributeCaptureDefinition& PowerDef =
		Request.Type == R::EDamageType::Physical ? DamageStatics().AttackPowerDef : DamageStatics().SpellPowerDef;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(PowerDef, EvaluationParameters, Power);
	Request.AttackerPower = Power;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritChanceDef, EvaluationParameters, Request.CritChance);

	R::FDefenseSnapshot Defense;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvaluationParameters, Defense.Armor);
	if (TargetTags)
	{
		if (TargetTags->HasTagExact(DBTags::State_Invulnerable))
		{
			Defense.State = R::EDefenseState::Invulnerable;
		}
		else if (TargetTags->HasTagExact(DBTags::State_ParryWindow))
		{
			Defense.State = R::EDefenseState::PerfectParry;
		}
		else if (TargetTags->HasTagExact(DBTags::State_Blocking))
		{
			Defense.State = R::EDefenseState::Blocking;
		}
	}
	// Elemental resistances become attributes in Phase 5 (gear); until then they are zero.

	const R::FDamageResult Result = R::ResolveDamage(Request, Defense);
	if (Result.FinalDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(UDBAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Result.FinalDamage));
	}
	if (Result.BlockStaminaCost > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(UDBAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, -Result.BlockStaminaCost));
	}
	// Parry/immune reactions (counter windows, VFX) are driven by gameplay events in Phase 2.
}
