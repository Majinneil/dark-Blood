#include "DarkBloodRules/Items.h"

namespace DarkBlood::Rules
{
	bool FItemCatalog::Add(FItemDefinition Definition)
	{
		if (Definition.Id.empty() || Definition.MaxStack < 1)
		{
			return false;
		}
		if (Definition.bIsBag)
		{
			Definition.Category = EItemCategory::Bag;
			Definition.MaxStack = 1;
		}
		std::string Key = Definition.Id;
		return Definitions.emplace(std::move(Key), std::move(Definition)).second;
	}

	const FItemDefinition* FItemCatalog::Find(std::string_view Id) const
	{
		const auto It = Definitions.find(std::string(Id));
		return It != Definitions.end() ? &It->second : nullptr;
	}

	bool FItemStack::CanStackWith(const FItemStack& Other) const
	{
		return ItemId == Other.ItemId && InstanceId == 0 && Other.InstanceId == 0 && Durability == Other.Durability;
	}

	FItemStats& FItemStats::operator+=(const FItemStats& Other)
	{
		AttackPower += Other.AttackPower;
		SpellPower += Other.SpellPower;
		Armor += Other.Armor;
		MaxHealth += Other.MaxHealth;
		MaxStamina += Other.MaxStamina;
		MaxMana += Other.MaxMana;
		CritChance += Other.CritChance;
		for (int32 Index = 0; Index < static_cast<int32>(EDamageType::Count); ++Index)
		{
			Resistances[Index] += Other.Resistances[Index];
		}
		return *this;
	}

	bool FItemStats::IsZero() const
	{
		bool bZero = AttackPower == 0.f && SpellPower == 0.f && Armor == 0.f && MaxHealth == 0.f && MaxStamina == 0.f && MaxMana == 0.f &&
			CritChance == 0.f;
		for (const float Resistance : Resistances)
		{
			bZero = bZero && Resistance == 0.f;
		}
		return bZero;
	}
}
