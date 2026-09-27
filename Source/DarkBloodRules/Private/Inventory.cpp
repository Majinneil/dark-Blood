#include "DarkBloodRules/Inventory.h"

#include <algorithm>
#include <utility>

namespace DarkBlood::Rules
{
	const char* ToString(EInventoryResult Result)
	{
		switch (Result)
		{
		case EInventoryResult::Ok: return "Ok";
		case EInventoryResult::InvalidSlot: return "InvalidSlot";
		case EInventoryResult::EmptySlot: return "EmptySlot";
		case EInventoryResult::InvalidCount: return "InvalidCount";
		case EInventoryResult::UnknownItem: return "UnknownItem";
		case EInventoryResult::NotABag: return "NotABag";
		case EInventoryResult::NotEquippable: return "NotEquippable";
		case EInventoryResult::WrongEquipSlot: return "WrongEquipSlot";
		case EInventoryResult::LevelTooLow: return "LevelTooLow";
		case EInventoryResult::ClassNotAllowed: return "ClassNotAllowed";
		case EInventoryResult::CategoryNotAccepted: return "CategoryNotAccepted";
		case EInventoryResult::InsufficientSpace: return "InsufficientSpace";
		case EInventoryResult::NotEnoughItems: return "NotEnoughItems";
		case EInventoryResult::SlotOccupied: return "SlotOccupied";
		}
		return "Unknown";
	}

	FInventory::FInventory(int32 BasePouchCapacity)
	{
		Sections.resize(1 + static_cast<size_t>(EBagKind::Count));

		FInventorySection& Base = Sections[BasePouchSection];
		Base.bIsBasePouch = true;
		Base.Kind = EBagKind::General;
		Base.Slots.resize(static_cast<size_t>(std::max(0, BasePouchCapacity)));

		for (int32 KindIndex = 0; KindIndex < static_cast<int32>(EBagKind::Count); ++KindIndex)
		{
			Sections[SectionForBag(static_cast<EBagKind>(KindIndex))].Kind = static_cast<EBagKind>(KindIndex);
		}
	}

	FItemDefinition FInventory::ResolveDefinition(const FItemCatalog& Catalog, const FItemStack& Stack) const
	{
		if (const FItemDefinition* Definition = Catalog.Find(Stack.ItemId))
		{
			return *Definition;
		}
		// Unknown ids (e.g. content removed from the game) are still relocated, never destroyed.
		FItemDefinition Fallback;
		Fallback.Id = Stack.ItemId;
		Fallback.Category = EItemCategory::Misc;
		Fallback.MaxStack = std::max(1, Stack.Count);
		return Fallback;
	}

	bool FInventory::IsValidRef(FSlotRef Ref) const
	{
		return Ref.Section >= 0 && Ref.Section < NumSections() && Ref.Index >= 0 &&
			Ref.Index < Sections[Ref.Section].Capacity();
	}

	const FItemStack* FInventory::GetSlot(FSlotRef Ref) const
	{
		return IsValidRef(Ref) ? &Sections[Ref.Section].Slots[Ref.Index] : nullptr;
	}

	int32 FInventory::TotalCapacity() const
	{
		int32 Total = 0;
		for (const FInventorySection& Section : Sections)
		{
			Total += Section.Capacity();
		}
		return Total;
	}

	int32 FInventory::FreeSlots() const
	{
		int32 Free = 0;
		for (const FInventorySection& Section : Sections)
		{
			for (const FItemStack& Slot : Section.Slots)
			{
				Free += Slot.IsEmpty() ? 1 : 0;
			}
		}
		return Free;
	}

	int32 FInventory::CountItem(std::string_view ItemId) const
	{
		int32 Total = 0;
		for (const FInventorySection& Section : Sections)
		{
			for (const FItemStack& Slot : Section.Slots)
			{
				if (!Slot.IsEmpty() && Slot.ItemId == ItemId)
				{
					Total += Slot.Count;
				}
			}
		}
		for (const FItemStack& Quest : QuestItems)
		{
			if (Quest.ItemId == ItemId)
			{
				Total += Quest.Count;
			}
		}
		return Total;
	}

	std::vector<int32> FInventory::PlacementOrder(EItemCategory Category, int32 PreferredSection) const
	{
		std::vector<int32> Order;
		Order.reserve(Sections.size());
		auto Push = [this, Category, &Order](int32 Section)
		{
			if (Section >= 0 && Section < NumSections() && Sections[Section].Accepts(Category) &&
				std::find(Order.begin(), Order.end(), Section) == Order.end())
			{
				Order.push_back(Section);
			}
		};

		Push(PreferredSection);
		for (int32 KindIndex = 0; KindIndex < static_cast<int32>(EBagKind::Count); ++KindIndex)
		{
			if (static_cast<EBagKind>(KindIndex) != EBagKind::General)
			{
				Push(SectionForBag(static_cast<EBagKind>(KindIndex)));
			}
		}
		Push(BasePouchSection);
		Push(SectionForBag(EBagKind::General));
		return Order;
	}

	int32 FInventory::MergeIntoSection(int32 Section, FItemStack& Stack, int32 MaxStack)
	{
		int32 Moved = 0;
		for (FItemStack& Slot : Sections[Section].Slots)
		{
			if (Stack.Count <= 0)
			{
				break;
			}
			if (Slot.IsEmpty() || !Slot.CanStackWith(Stack))
			{
				continue;
			}
			const int32 Space = std::max(0, MaxStack - Slot.Count);
			const int32 Amount = std::min(Space, Stack.Count);
			Slot.Count += Amount;
			Stack.Count -= Amount;
			Moved += Amount;
		}
		return Moved;
	}

	int32 FInventory::FillEmptyInSection(int32 Section, FItemStack& Stack, int32 MaxStack)
	{
		int32 Moved = 0;
		for (FItemStack& Slot : Sections[Section].Slots)
		{
			if (Stack.Count <= 0)
			{
				break;
			}
			if (!Slot.IsEmpty())
			{
				continue;
			}
			const int32 Amount = std::min(MaxStack, Stack.Count);
			Slot = Stack;
			Slot.Count = Amount;
			Stack.Count -= Amount;
			Moved += Amount;
		}
		return Moved;
	}

	int32 FInventory::Place(const FItemCatalog& Catalog, FItemStack Stack, int32 PreferredSection)
	{
		if (Stack.IsEmpty())
		{
			return 0;
		}

		const FItemDefinition Definition = ResolveDefinition(Catalog, Stack);
		if (Definition.Category == EItemCategory::Quest)
		{
			for (FItemStack& Existing : QuestItems)
			{
				if (Existing.CanStackWith(Stack))
				{
					Existing.Count += Stack.Count;
					return 0;
				}
			}
			QuestItems.push_back(std::move(Stack));
			return 0;
		}

		const int32 MaxStack = Stack.InstanceId != 0 ? 1 : std::max(1, Definition.MaxStack);
		const std::vector<int32> Order = PlacementOrder(Definition.Category, PreferredSection);

		if (Stack.InstanceId == 0)
		{
			for (const int32 Section : Order)
			{
				MergeIntoSection(Section, Stack, MaxStack);
				if (Stack.Count <= 0)
				{
					return 0;
				}
			}
		}
		for (const int32 Section : Order)
		{
			FillEmptyInSection(Section, Stack, MaxStack);
			if (Stack.Count <= 0)
			{
				return 0;
			}
		}
		return Stack.Count;
	}

	FAddResult FInventory::Add(const FItemCatalog& Catalog, const FItemStack& Stack, EAddMode Mode)
	{
		FAddResult Result;
		if (Stack.Count <= 0 || (Stack.InstanceId != 0 && Stack.Count != 1))
		{
			Result.Result = EInventoryResult::InvalidCount;
			Result.Remaining = std::max(0, Stack.Count);
			return Result;
		}
		if (Catalog.Find(Stack.ItemId) == nullptr)
		{
			Result.Result = EInventoryResult::UnknownItem;
			Result.Remaining = Stack.Count;
			return Result;
		}

		if (Mode == EAddMode::AllOrNothing)
		{
			FInventory Next = *this;
			const int32 Remaining = Next.Place(Catalog, Stack, -1);
			if (Remaining > 0)
			{
				Result.Result = EInventoryResult::InsufficientSpace;
				Result.Remaining = Stack.Count;
				return Result;
			}
			*this = std::move(Next);
			Result.Added = Stack.Count;
			return Result;
		}

		Result.Remaining = Place(Catalog, Stack, -1);
		Result.Added = Stack.Count - Result.Remaining;
		Result.Result = Result.Remaining > 0 ? EInventoryResult::InsufficientSpace : EInventoryResult::Ok;
		return Result;
	}

	EInventoryResult FInventory::Remove(std::string_view ItemId, int32 Count)
	{
		if (Count <= 0)
		{
			return EInventoryResult::InvalidCount;
		}
		if (CountItem(ItemId) < Count)
		{
			return EInventoryResult::NotEnoughItems;
		}

		int32 Remaining = Count;
		auto Consume = [&Remaining, ItemId](FItemStack& Stack)
		{
			if (Remaining <= 0 || Stack.IsEmpty() || Stack.ItemId != ItemId)
			{
				return;
			}
			const int32 Amount = std::min(Remaining, Stack.Count);
			Stack.Count -= Amount;
			Remaining -= Amount;
			if (Stack.Count <= 0)
			{
				Stack = FItemStack();
			}
		};

		for (FItemStack& Quest : QuestItems)
		{
			Consume(Quest);
		}
		QuestItems.erase(std::remove_if(QuestItems.begin(), QuestItems.end(),
			[](const FItemStack& Stack) { return Stack.IsEmpty(); }), QuestItems.end());

		// Consume from the base pouch last so bag content is used first.
		for (int32 Section = NumSections() - 1; Section >= 0 && Remaining > 0; --Section)
		{
			for (FItemStack& Slot : Sections[Section].Slots)
			{
				Consume(Slot);
			}
		}
		return EInventoryResult::Ok;
	}

	EInventoryResult FInventory::RemoveAt(FSlotRef Ref, int32 Count, FItemStack* OutRemoved)
	{
		if (!IsValidRef(Ref))
		{
			return EInventoryResult::InvalidSlot;
		}
		FItemStack& Slot = Sections[Ref.Section].Slots[Ref.Index];
		if (Slot.IsEmpty())
		{
			return EInventoryResult::EmptySlot;
		}
		const int32 Amount = Count <= 0 ? Slot.Count : Count;
		if (Amount > Slot.Count)
		{
			return EInventoryResult::InvalidCount;
		}
		if (OutRemoved)
		{
			*OutRemoved = Slot;
			OutRemoved->Count = Amount;
		}
		Slot.Count -= Amount;
		if (Slot.Count <= 0)
		{
			Slot = FItemStack();
		}
		return EInventoryResult::Ok;
	}

	EInventoryResult FInventory::Move(const FItemCatalog& Catalog, FSlotRef From, FSlotRef To, int32 Count)
	{
		if (!IsValidRef(From) || !IsValidRef(To))
		{
			return EInventoryResult::InvalidSlot;
		}
		if (From.Section == To.Section && From.Index == To.Index)
		{
			return EInventoryResult::Ok;
		}

		FItemStack& Source = Sections[From.Section].Slots[From.Index];
		FItemStack& Target = Sections[To.Section].Slots[To.Index];
		if (Source.IsEmpty())
		{
			return EInventoryResult::EmptySlot;
		}

		const int32 Amount = Count <= 0 ? Source.Count : Count;
		if (Amount > Source.Count)
		{
			return EInventoryResult::InvalidCount;
		}

		const FItemDefinition SourceDefinition = ResolveDefinition(Catalog, Source);
		if (!Sections[To.Section].Accepts(SourceDefinition.Category))
		{
			return EInventoryResult::CategoryNotAccepted;
		}

		if (Target.IsEmpty())
		{
			Target = Source;
			Target.Count = Amount;
			Source.Count -= Amount;
			if (Source.Count <= 0)
			{
				Source = FItemStack();
			}
			return EInventoryResult::Ok;
		}

		if (Target.CanStackWith(Source))
		{
			const int32 Space = std::max(0, SourceDefinition.MaxStack - Target.Count);
			if (Space == 0)
			{
				return EInventoryResult::SlotOccupied;
			}
			const int32 Moved = std::min(Space, Amount);
			Target.Count += Moved;
			Source.Count -= Moved;
			if (Source.Count <= 0)
			{
				Source = FItemStack();
			}
			return EInventoryResult::Ok;
		}

		// Different items: only whole-stack swaps are allowed.
		if (Amount != Source.Count)
		{
			return EInventoryResult::SlotOccupied;
		}
		const FItemDefinition TargetDefinition = ResolveDefinition(Catalog, Target);
		if (!Sections[From.Section].Accepts(TargetDefinition.Category))
		{
			return EInventoryResult::CategoryNotAccepted;
		}
		std::swap(Source, Target);
		return EInventoryResult::Ok;
	}

	EInventoryResult FInventory::EquipBag(const FItemCatalog& Catalog, FSlotRef BagSlot)
	{
		const FItemStack* Slot = GetSlot(BagSlot);
		if (!Slot)
		{
			return EInventoryResult::InvalidSlot;
		}
		if (Slot->IsEmpty())
		{
			return EInventoryResult::EmptySlot;
		}
		const FItemDefinition* Definition = Catalog.Find(Slot->ItemId);
		if (!Definition)
		{
			return EInventoryResult::UnknownItem;
		}
		if (!Definition->bIsBag)
		{
			return EInventoryResult::NotABag;
		}

		FInventory Next = *this;
		FItemStack NewBag;
		Next.RemoveAt(BagSlot, 1, &NewBag);

		const int32 SectionIndex = SectionForBag(Definition->Bag.Kind);
		FInventorySection& Section = Next.Sections[SectionIndex];
		FItemStack OldBag = std::move(Section.BagItem);

		std::vector<FItemStack> Displaced;
		for (FItemStack& Item : Section.Slots)
		{
			if (!Item.IsEmpty())
			{
				Displaced.push_back(std::move(Item));
			}
		}

		Section.BagItem = std::move(NewBag);
		Section.AcceptedCategories =
			Definition->Bag.Kind == EBagKind::General ? AllCategories : Definition->Bag.AcceptedCategories;
		Section.Slots.assign(static_cast<size_t>(std::max(0, Definition->Bag.Capacity)), FItemStack());

		for (FItemStack& Item : Displaced)
		{
			if (Next.Place(Catalog, std::move(Item), SectionIndex) > 0)
			{
				return EInventoryResult::InsufficientSpace;
			}
		}
		if (!OldBag.IsEmpty() && Next.Place(Catalog, std::move(OldBag), -1) > 0)
		{
			return EInventoryResult::InsufficientSpace;
		}

		*this = std::move(Next);
		return EInventoryResult::Ok;
	}

	EInventoryResult FInventory::UnequipBag(const FItemCatalog& Catalog, EBagKind Kind)
	{
		const int32 SectionIndex = SectionForBag(Kind);
		if (Sections[SectionIndex].BagItem.IsEmpty())
		{
			return EInventoryResult::EmptySlot;
		}

		FInventory Next = *this;
		FInventorySection& Section = Next.Sections[SectionIndex];
		FItemStack OldBag = std::move(Section.BagItem);
		Section.BagItem = FItemStack();

		std::vector<FItemStack> Displaced;
		for (FItemStack& Item : Section.Slots)
		{
			if (!Item.IsEmpty())
			{
				Displaced.push_back(std::move(Item));
			}
		}
		Section.Slots.clear();
		Section.AcceptedCategories = AllCategories;

		for (FItemStack& Item : Displaced)
		{
			if (Next.Place(Catalog, std::move(Item), -1) > 0)
			{
				return EInventoryResult::InsufficientSpace;
			}
		}
		if (Next.Place(Catalog, std::move(OldBag), -1) > 0)
		{
			return EInventoryResult::InsufficientSpace;
		}

		*this = std::move(Next);
		return EInventoryResult::Ok;
	}

	std::vector<FItemStack> FInventory::CollectAllItems() const
	{
		std::vector<FItemStack> Items;
		for (const FInventorySection& Section : Sections)
		{
			if (!Section.BagItem.IsEmpty())
			{
				Items.push_back(Section.BagItem);
			}
			for (const FItemStack& Slot : Section.Slots)
			{
				if (!Slot.IsEmpty())
				{
					Items.push_back(Slot);
				}
			}
		}
		Items.insert(Items.end(), QuestItems.begin(), QuestItems.end());
		return Items;
	}

	bool FInventory::RestoreRaw(std::vector<FInventorySection> InSections, std::vector<FItemStack> InQuestItems)
	{
		if (InSections.size() != 1 + static_cast<size_t>(EBagKind::Count))
		{
			return false;
		}
		for (size_t Index = 0; Index < InSections.size(); ++Index)
		{
			FInventorySection& Section = InSections[Index];
			Section.bIsBasePouch = Index == BasePouchSection;
			Section.Kind = Index == BasePouchSection ? EBagKind::General : static_cast<EBagKind>(Index - 1);
			if (!Section.bIsBasePouch && Section.BagItem.IsEmpty() && !Section.Slots.empty())
			{
				return false; // slots without a bag providing them
			}
			for (FItemStack& Slot : Section.Slots)
			{
				if (Slot.Count < 0)
				{
					return false;
				}
				if (Slot.IsEmpty())
				{
					Slot = FItemStack();
				}
			}
		}
		Sections = std::move(InSections);
		QuestItems = std::move(InQuestItems);
		return true;
	}
}
