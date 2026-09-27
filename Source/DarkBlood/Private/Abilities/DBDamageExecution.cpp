#include "Abilities/DBDamageExecution.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Combat/DBCombatStatics.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"

#include "DarkBloodRules/Damage.h"

namespace
{
	/** How long a perfect parry opens the counter window. */
	constexpr float CounterWindowSeconds = 1.f;

	// Built from the public attribute getters: the attribute properties are private,
	// so DEFINE_ATTRIBUTE_CAPTUREDEF (which names the member directly) cannot be used.
	struct FDBDamageStatics
	{
		FGameplayEffectAttributeCaptureDefinition AttackPowerDef;
		FGameplayEffectAttributeCaptureDefinition SpellPowerDef;
		FGameplayEffectAttributeCaptureDefinition CritChanceDef;
		FGameplayEffectAttributeCaptureDefinition ArmorDef;

		FDBDamageStatics()
			: AttackPowerDef(UDBAttributeSet::GetAttackPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true)
			, SpellPowerDef(UDBAttributeSet::GetSpellPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true)
			, CritChanceDef(UDBAttributeSet::GetCritChanceAttribute(), EGameplayEffectAttributeCaptureSource::Source, true)
			, ArmorDef(UDBAttributeSet::GetArmorAttribute(), EGameplayEffectAttributeCaptureSource::Target, false)
		{
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
	for (int32 Index = 1; Index < static_cast<int32>(DarkBlood::Rules::EDamageType::Count); ++Index)
	{
		RelevantAttributesToCapture.Add(
			FGameplayEffectAttributeCaptureDefinition(UDBAttributeSet::GetResistanceAttribute(Index), EGameplayEffectAttributeCaptureSource::Target, false));
	}
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
	bool bCounterStance = false;
	if (TargetTags)
	{
		if (TargetTags->HasTagExact(DBTags::State_Warded))
		{
			Defense.DamageTakenMultiplier = 0.7f;
		}
		if (TargetTags->HasTagExact(DBTags::State_IronStance))
		{
			Defense.BlockStaminaMultiplier = 0.5f;
		}
		bCounterStance = TargetTags->HasTagExact(DBTags::State_CounterStance);
		if (TargetTags->HasTagExact(DBTags::State_Invulnerable))
		{
			Defense.State = R::EDefenseState::Invulnerable;
		}
		else if (TargetTags->HasTagExact(DBTags::State_ParryWindow) || bCounterStance)
		{
			Defense.State = R::EDefenseState::PerfectParry;
		}
		else if (TargetTags->HasTagExact(DBTags::State_Blocking))
		{
			Defense.State = R::EDefenseState::Blocking;
		}
	}
	// Elemental resistances (gear, effects) of the target.
	for (int32 Index = 1; Index < static_cast<int32>(R::EDamageType::Count); ++Index)
	{
		const FGameplayAttribute Attribute = UDBAttributeSet::GetResistanceAttribute(Index);
		if (Attribute.IsValid())
		{
			ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
				FGameplayEffectAttributeCaptureDefinition(Attribute, EGameplayEffectAttributeCaptureSource::Target, false), EvaluationParameters,
				Defense.Resistances[Index]);
		}
	}

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

	// Poise: nothing on parry/immunity, a block absorbs the same share as for health.
	float PoiseDamage = 0.f;
	if (!Result.bParried && !Result.bImmune)
	{
		PoiseDamage = Spec.GetSetByCallerMagnitude(DBTags::SetByCaller_PoiseDamage, false, 0.f);
		if (Result.bBlocked)
		{
			PoiseDamage *= 1.f - Defense.BlockEfficiency;
		}
		if (PoiseDamage > 0.f || SpecTags.HasTagExact(DBTags::Damage_Knockdown))
		{
			OutExecutionOutput.AddOutputModifier(
				FGameplayModifierEvaluatedData(UDBAttributeSet::GetIncomingPoiseDamageAttribute(), EGameplayModOp::Additive, PoiseDamage));
		}
	}

	UDBAbilitySystemComponent* SourceASC = Cast<UDBAbilitySystemComponent>(ExecutionParams.GetSourceAbilitySystemComponent());
	UDBAbilitySystemComponent* TargetASC = Cast<UDBAbilitySystemComponent>(ExecutionParams.GetTargetAbilitySystemComponent());
	const AActor* SourceAvatar = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	const AActor* TargetAvatar = TargetASC ? TargetASC->GetAvatarActor() : nullptr;

	if (Result.bParried && TargetASC)
	{
		// Perfect parry: the defender gets a counter window, the attacker bounces off.
		TargetASC->AddTimedLooseTag(DBTags::State_CounterWindow, CounterWindowSeconds);
		FGameplayEventData Payload;
		Payload.Instigator = SourceAvatar;
		Payload.Target = TargetAvatar;
		TargetASC->SendGameplayEventDeferred(DBTags::Event_Combat_ParrySuccess, Payload);
		DBCombat::SendHitReact(SourceASC, R::EHitReaction::ParriedStagger, TargetAvatar);
		if (bCounterStance)
		{
			// The monk's counter stance answers the caught hit (see UDBAbility_CounterStance).
			FGameplayEventData Counter;
			Counter.Instigator = SourceAvatar;
			Counter.Target = TargetAvatar;
			TargetASC->SendGameplayEventDeferred(DBTags::Event_Combat_CounterTriggered, Counter);
		}
	}

	// Presentation hooks; without GameplayCue notify assets these are no-ops.
	if (TargetASC)
	{
		const FGameplayTag Cue = Result.bParried  ? DBTags::GameplayCue_Combat_Parried
							   : Result.bBlocked  ? DBTags::GameplayCue_Combat_Blocked
							   : Result.bImmune   ? FGameplayTag()
												  : DBTags::GameplayCue_Combat_Hit;
		if (Cue.IsValid())
		{
			TargetASC->ExecuteGameplayCue(Cue, Spec.GetEffectContext());
		}
	}

	UE_LOG(LogDBCombat, Log, TEXT("%s -> %s: %.1f damage%s%s%s%s, poise %.1f"), *DBCombat::GetCombatName(SourceAvatar),
		*DBCombat::GetCombatName(TargetAvatar), Result.FinalDamage, Result.bCritical ? TEXT(" CRIT") : TEXT(""),
		Result.bBlocked ? TEXT(" BLOCKED") : TEXT(""), Result.bParried ? TEXT(" PARRIED") : TEXT(""), Result.bImmune ? TEXT(" IMMUNE") : TEXT(""),
		PoiseDamage);
}
