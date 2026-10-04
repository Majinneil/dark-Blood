#include "DarkBloodRules/Damage.h"

#include <algorithm>

namespace DarkBlood::Rules
{
	FDamageResult ResolveDamage(const FDamageRequest& Request, const FDefenseSnapshot& Defense, const FDamageRules& Rules)
	{
		FDamageResult Result;
		if (Request.BaseDamage <= 0.f)
		{
			return Result;
		}

		if (Defense.State == EDefenseState::Invulnerable && !Request.bIgnoresInvulnerability)
		{
			Result.bImmune = true;
			return Result;
		}
		if (Defense.State == EDefenseState::PerfectParry && Request.bCanBeParried)
		{
			Result.bParried = true;
			return Result;
		}

		float Damage = Request.BaseDamage * (1.f + std::max(0.f, Request.AttackerPower) * Rules.PowerScaling);

		if (Request.CritRoll < Request.CritChance)
		{
			Result.bCritical = true;
			Damage *= std::max(1.f, Request.CritMultiplier);
		}

		if (Request.Type == EDamageType::Physical)
		{
			const float Armor = std::max(0.f, Defense.Armor);
			const float Constant = Rules.ArmorConstantPerLevel * static_cast<float>(std::max(1, Request.AttackerLevel));
			const float Mitigation = std::min(Rules.MaxArmorMitigation, Armor / (Armor + Constant));
			Damage *= 1.f - Mitigation;
		}
		else
		{
			const int32 TypeIndex = static_cast<int32>(Request.Type);
			const float Resistance = std::clamp(Defense.Resistances[TypeIndex], -1.f, Rules.MaxResistance);
			Damage *= 1.f - Resistance;
		}

		if (Defense.State == EDefenseState::Blocking && Request.bCanBeBlocked)
		{
			const float Efficiency = std::clamp(Defense.BlockEfficiency, 0.f, 1.f);
			Result.bBlocked = true;
			Result.BlockStaminaCost = Damage * Efficiency * Rules.BlockStaminaPerDamage * std::max(0.f, Defense.BlockStaminaMultiplier);
			Damage *= 1.f - Efficiency;
			Result.FinalDamage = std::max(0.f, Damage * std::clamp(Defense.DamageTakenMultiplier, 0.f, 2.f));
			return Result;
		}

		Damage *= std::clamp(Defense.DamageTakenMultiplier, 0.f, 2.f);
		Result.FinalDamage = std::max(Rules.MinimumDamage, Damage);
		return Result;
	}
}
