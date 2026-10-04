// DARK BLOOD - Rules Core: damage resolution.
// Evaluated exclusively on the server (GAS execution calculation calls into this).
// Randomness is injected by the caller so results are deterministic and testable.
#pragma once

#include "DarkBloodRules/RulesCore.h"

namespace DarkBlood::Rules
{
	enum class EDamageType : uint8
	{
		Physical,
		Fire,
		Frost,
		Lightning,
		Shadow,
		Poison,
		Spirit,
		DarkBlood,
		Count
	};

	enum class EDefenseState : uint8
	{
		None,
		Blocking,
		PerfectParry, // inside the perfect-parry window
		Invulnerable, // dodge i-frames, cinematic, etc.
	};

	struct FDamageRequest
	{
		float BaseDamage = 0.f;
		/** Attack power (physical) or spell power (magic) of the attacker. */
		float AttackerPower = 0.f;
		int32 AttackerLevel = 1;
		EDamageType Type = EDamageType::Physical;
		float CritChance = 0.f;
		float CritMultiplier = 1.5f;
		/** Uniform [0,1) roll supplied by the server RNG. */
		float CritRoll = 1.f;
		bool bCanBeBlocked = true;
		bool bCanBeParried = true;
		/** Unblockable/unparryable boss attacks still respect i-frames unless this is set. */
		bool bIgnoresInvulnerability = false;
	};

	struct FDefenseSnapshot
	{
		float Armor = 0.f;
		/** Fractional resistance per damage type, clamped to [-1, MaxResistance]. */
		float Resistances[static_cast<int32>(EDamageType::Count)] = {};
		EDefenseState State = EDefenseState::None;
		/** Fraction of damage negated when blocking. */
		float BlockEfficiency = 0.7f;
		/** Scales the stamina a block costs (e.g. 0.5 in a defensive stance). */
		float BlockStaminaMultiplier = 1.f;
		/** Final multiplier on damage taken (protective circles, buffs); clamped to [0, 2]. */
		float DamageTakenMultiplier = 1.f;
	};

	struct FDamageRules
	{
		float ArmorConstantPerLevel = 50.f;
		float MaxArmorMitigation = 0.75f;
		float MaxResistance = 0.8f;
		float PowerScaling = 0.01f; // +1% damage per point of attacker power
		float BlockStaminaPerDamage = 0.5f;
		float MinimumDamage = 1.f;
	};

	struct FDamageResult
	{
		float FinalDamage = 0.f;
		float BlockStaminaCost = 0.f;
		bool bCritical = false;
		bool bBlocked = false;
		bool bParried = false;
		bool bImmune = false;
	};

	DARKBLOODRULES_API FDamageResult ResolveDamage(const FDamageRequest& Request, const FDefenseSnapshot& Defense,
		const FDamageRules& Rules = FDamageRules());
}
