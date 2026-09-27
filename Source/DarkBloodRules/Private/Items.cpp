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
}
