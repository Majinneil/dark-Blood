#include "Inventory/DBInventoryComponent.h"

#include "Core/DBGameSettings.h"
#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBGameDataSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"

namespace R = DarkBlood::Rules;

namespace
{
	FDBItemStackView ToView(const R::FItemStack& Stack)
	{
		FDBItemStackView View;
		if (!Stack.IsEmpty())
		{
			View.ItemId = DBBridge::ToFName(Stack.ItemId);
			View.Count = Stack.Count;
			View.InstanceId = static_cast<int64>(Stack.InstanceId);
			View.Durability = Stack.Durability;
		}
		return View;
	}
}

void FDBInventoryEntry::PostReplicatedAdd(const FDBInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->BroadcastChanged();
	}
}

void FDBInventoryEntry::PostReplicatedChange(const FDBInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->BroadcastChanged();
	}
}

void FDBInventoryEntry::PreReplicatedRemove(const FDBInventoryList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->BroadcastChanged();
	}
}

UDBInventoryComponent::UDBInventoryComponent()
	: Inventory(UDBGameSettings::Get().BasePouchCapacity)
{
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;
	ReplicatedItems.Owner = this;
	Equipped.SetNum(R::FEquipment::NumSlots);
}

void UDBInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();
	ReplicatedItems.Owner = this;
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SyncReplicatedView();
	}
}

void UDBInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UDBInventoryComponent, ReplicatedItems, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDBInventoryComponent, Sections, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDBInventoryComponent, Currency, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDBInventoryComponent, PendingDeliveryCount, COND_OwnerOnly);
	DOREPLIFETIME(UDBInventoryComponent, Equipped);
}

const R::FItemCatalog* UDBInventoryComponent::GetCatalog() const
{
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	return Data ? &Data->GetItemCatalog() : nullptr;
}

R::FEquipContext UDBInventoryComponent::MakeEquipContext() const
{
	R::FEquipContext Context;
	if (const ADBPlayerState* PlayerState = Cast<ADBPlayerState>(GetOwner()))
	{
		Context.ClassId = DBBridge::ToStd(PlayerState->GetProfile().ClassId);
		if (const UDBProgressionComponent* Progression = PlayerState->GetProgression())
		{
			Context.CharacterLevel = Progression->GetState().Level;
		}
	}
	return Context;
}

int32 UDBInventoryComponent::AddItem(FName ItemId, int32 Count, bool bAllowPartial)
{
	const R::FItemCatalog* Catalog = GetCatalog();
	if (!GetOwner()->HasAuthority() || !Catalog || Count <= 0)
	{
		return 0;
	}
	const R::FItemDefinition* Definition = Catalog->Find(DBBridge::ToStd(ItemId));
	if (!Definition)
	{
		UE_LOG(LogDarkBlood, Warning, TEXT("AddItem: unknown item %s"), *ItemId.ToString());
		return 0;
	}

	int32 Added = 0;
	if (Definition->MaxDurability > 0)
	{
		// Items with durability are unique instances and never stack.
		R::FInventory Working = Inventory;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const R::FItemStack Unique = DBBridge::MakeStack(ItemId, 1, DBBridge::NewInstanceId(), Definition->MaxDurability);
			if (Working.Add(*Catalog, Unique).Result != R::EInventoryResult::Ok)
			{
				break;
			}
			++Added;
		}
		if (Added == Count || (bAllowPartial && Added > 0))
		{
			Inventory = MoveTemp(Working);
		}
		else
		{
			Added = 0;
		}
	}
	else
	{
		const R::FAddResult Result = Inventory.Add(*Catalog, DBBridge::MakeStack(ItemId, Count),
			bAllowPartial ? R::EAddMode::Partial : R::EAddMode::AllOrNothing);
		Added = Result.Added;
	}

	if (Added > 0)
	{
		SyncReplicatedView();
	}
	return Added;
}

bool UDBInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	const bool bRemoved = Inventory.Remove(DBBridge::ToStd(ItemId), Count) == R::EInventoryResult::Ok;
	if (bRemoved)
	{
		TryDeliverPending();
		SyncReplicatedView();
	}
	return bRemoved;
}

void UDBInventoryComponent::DeliverItem(FName ItemId, int32 Count)
{
	const R::FItemCatalog* Catalog = GetCatalog();
	if (!GetOwner()->HasAuthority() || !Catalog || Count <= 0)
	{
		return;
	}
	const int32 Added = AddItem(ItemId, Count, true);
	if (Added < Count)
	{
		PendingDeliveries.push_back(DBBridge::MakeStack(ItemId, Count - Added));
		UE_LOG(LogDarkBlood, Log, TEXT("Inventory full: %d x %s queued for later delivery"), Count - Added, *ItemId.ToString());
		SyncReplicatedView();
	}
}

void UDBInventoryComponent::TryDeliverPending()
{
	const R::FItemCatalog* Catalog = GetCatalog();
	if (!Catalog || PendingDeliveries.empty() || !GetOwner()->HasAuthority())
	{
		return;
	}
	std::vector<R::FItemStack> StillPending;
	for (R::FItemStack& Pending : PendingDeliveries)
	{
		const R::FItemDefinition* Definition = Catalog->Find(Pending.ItemId);
		if (!Definition)
		{
			StillPending.push_back(Pending); // keep unknown content untouched
			continue;
		}
		const FName ItemId = DBBridge::ToFName(Pending.ItemId);
		const int32 Added = AddItem(ItemId, Pending.Count, true);
		if (Added < Pending.Count)
		{
			Pending.Count -= Added;
			StillPending.push_back(Pending);
		}
	}
	PendingDeliveries = MoveTemp(StillPending);
}

void UDBInventoryComponent::AddCurrency(int64 Amount)
{
	if (GetOwner()->HasAuthority() && Amount > 0)
	{
		ServerCurrency = FMath::Min<int64>(ServerCurrency + Amount, 999'999'999);
		SyncReplicatedView();
	}
}

bool UDBInventoryComponent::SpendCurrency(int64 Amount)
{
	if (!GetOwner()->HasAuthority() || Amount < 0 || ServerCurrency < Amount)
	{
		return false;
	}
	ServerCurrency -= Amount;
	SyncReplicatedView();
	return true;
}

int64 UDBInventoryComponent::ApplyDeathPenalty()
{
	const R::FItemCatalog* Catalog = GetCatalog();
	if (!GetOwner()->HasAuthority() || !Catalog)
	{
		return 0;
	}
	const R::FDeathPenaltyResult Result = R::ApplyDeathPenalty(ServerCurrency, Equipment, *Catalog);
	SyncReplicatedView();
	return Result.CurrencyLost;
}

void UDBInventoryComponent::RestoreFromRecord(const R::FInventory& InInventory, const R::FEquipment& InEquipment, int64 InCurrency,
	const std::vector<R::FItemStack>& InPendingDeliveries)
{
	Inventory = InInventory;
	Equipment = InEquipment;
	ServerCurrency = InCurrency;
	PendingDeliveries = InPendingDeliveries;
	TryDeliverPending();
	SyncReplicatedView();
}

void UDBInventoryComponent::ServerMoveItem_Implementation(FDBSlotRef From, FDBSlotRef To, int32 Count)
{
	const R::FItemCatalog* Catalog = GetCatalog();
	const R::EInventoryResult Result =
		Catalog ? Inventory.Move(*Catalog, DBBridge::ToRules(From), DBBridge::ToRules(To), Count) : R::EInventoryResult::UnknownItem;
	FinishRequest(TEXT("MoveItem"), Result, false);
}

void UDBInventoryComponent::ServerEquipBag_Implementation(FDBSlotRef BagSlot)
{
	const R::FItemCatalog* Catalog = GetCatalog();
	const R::EInventoryResult Result = Catalog ? Inventory.EquipBag(*Catalog, DBBridge::ToRules(BagSlot)) : R::EInventoryResult::UnknownItem;
	FinishRequest(TEXT("EquipBag"), Result, false);
}

void UDBInventoryComponent::ServerUnequipBag_Implementation(EDBBagKind Kind)
{
	const R::FItemCatalog* Catalog = GetCatalog();
	const R::EInventoryResult Result =
		Catalog ? Inventory.UnequipBag(*Catalog, DBBridge::CastEnum<R::EBagKind>(Kind)) : R::EInventoryResult::UnknownItem;
	FinishRequest(TEXT("UnequipBag"), Result, false);
}

void UDBInventoryComponent::ServerEquipItem_Implementation(FDBSlotRef From, EDBEquipSlot Slot)
{
	const R::FItemCatalog* Catalog = GetCatalog();
	const R::EInventoryResult Result = Catalog
		? R::EquipFromInventory(Inventory, Equipment, *Catalog, DBBridge::ToRules(From), DBBridge::CastEnum<R::EEquipSlot>(Slot), MakeEquipContext())
		: R::EInventoryResult::UnknownItem;
	FinishRequest(TEXT("EquipItem"), Result, true);
}

void UDBInventoryComponent::ServerUnequipItem_Implementation(EDBEquipSlot Slot)
{
	const R::FItemCatalog* Catalog = GetCatalog();
	const R::EInventoryResult Result =
		Catalog ? R::UnequipToInventory(Inventory, Equipment, *Catalog, DBBridge::CastEnum<R::EEquipSlot>(Slot)) : R::EInventoryResult::UnknownItem;
	FinishRequest(TEXT("UnequipItem"), Result, true);
}

void UDBInventoryComponent::ClientRequestFailed_Implementation(FName Operation, const FString& Reason)
{
	OnRequestFailed.Broadcast(Operation, Reason);
}

void UDBInventoryComponent::FinishRequest(FName Operation, R::EInventoryResult Result, bool bGearChanged)
{
	if (Result != R::EInventoryResult::Ok)
	{
		ClientRequestFailed(Operation, FString(ANSI_TO_TCHAR(R::ToString(Result))));
		return;
	}
	TryDeliverPending();
	SyncReplicatedView();
	if (bGearChanged)
	{
		if (const ADBPlayerState* PlayerState = Cast<ADBPlayerState>(GetOwner()))
		{
			if (UDBProgressionComponent* Progression = PlayerState->GetProgression())
			{
				Progression->RecalculateAttributes(false);
			}
		}
	}
}

void UDBInventoryComponent::SyncReplicatedView()
{
	// Desired state keyed by (section, slot); quest items use section -1.
	TMap<FIntPoint, FDBItemStackView> Desired;
	for (int32 SectionIndex = 0; SectionIndex < Inventory.NumSections(); ++SectionIndex)
	{
		const R::FInventorySection& Section = Inventory.GetSection(SectionIndex);
		for (int32 SlotIndex = 0; SlotIndex < Section.Capacity(); ++SlotIndex)
		{
			if (!Section.Slots[SlotIndex].IsEmpty())
			{
				Desired.Add(FIntPoint(SectionIndex, SlotIndex), ToView(Section.Slots[SlotIndex]));
			}
		}
	}
	const std::vector<R::FItemStack>& QuestItems = Inventory.GetQuestItems();
	for (int32 Index = 0; Index < static_cast<int32>(QuestItems.size()); ++Index)
	{
		Desired.Add(FIntPoint(-1, Index), ToView(QuestItems[Index]));
	}

	bool bRemovedAny = false;
	for (int32 EntryIndex = ReplicatedItems.Entries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		FDBInventoryEntry& Entry = ReplicatedItems.Entries[EntryIndex];
		const FIntPoint Key(Entry.Section, Entry.SlotIndex);
		if (const FDBItemStackView* Wanted = Desired.Find(Key))
		{
			if (!(Entry.Stack == *Wanted))
			{
				Entry.Stack = *Wanted;
				ReplicatedItems.MarkItemDirty(Entry);
			}
			Desired.Remove(Key);
		}
		else
		{
			ReplicatedItems.Entries.RemoveAtSwap(EntryIndex);
			bRemovedAny = true;
		}
	}
	if (bRemovedAny)
	{
		ReplicatedItems.MarkArrayDirty();
	}
	for (const TPair<FIntPoint, FDBItemStackView>& Pair : Desired)
	{
		FDBInventoryEntry& Entry = ReplicatedItems.Entries.AddDefaulted_GetRef();
		Entry.Section = Pair.Key.X;
		Entry.SlotIndex = Pair.Key.Y;
		Entry.Stack = Pair.Value;
		ReplicatedItems.MarkItemDirty(Entry);
	}

	Sections.SetNum(Inventory.NumSections());
	for (int32 SectionIndex = 0; SectionIndex < Inventory.NumSections(); ++SectionIndex)
	{
		const R::FInventorySection& Section = Inventory.GetSection(SectionIndex);
		Sections[SectionIndex].Capacity = Section.Capacity();
		Sections[SectionIndex].BagItemId = DBBridge::ToFName(Section.BagItem.ItemId);
	}

	Equipped.SetNum(R::FEquipment::NumSlots);
	for (int32 SlotIndex = 0; SlotIndex < R::FEquipment::NumSlots; ++SlotIndex)
	{
		Equipped[SlotIndex] = ToView(Equipment.Get(static_cast<R::EEquipSlot>(SlotIndex)));
	}

	Currency = ServerCurrency;
	PendingDeliveryCount = 0;
	for (const R::FItemStack& Pending : PendingDeliveries)
	{
		PendingDeliveryCount += Pending.Count;
	}
	OnInventoryChanged.Broadcast();
}

void UDBInventoryComponent::OnRep_View()
{
	OnInventoryChanged.Broadcast();
}

FDBItemStackView UDBInventoryComponent::GetEquipped(EDBEquipSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return Equipped.IsValidIndex(Index) ? Equipped[Index] : FDBItemStackView();
}

int32 UDBInventoryComponent::CountItem(FName ItemId) const
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return Inventory.CountItem(DBBridge::ToStd(ItemId));
	}
	int32 Total = 0;
	for (const FDBInventoryEntry& Entry : ReplicatedItems.Entries)
	{
		Total += Entry.Stack.ItemId == ItemId ? Entry.Stack.Count : 0;
	}
	return Total;
}

float UDBInventoryComponent::ComputeGearScore() const
{
	const R::FItemCatalog* Catalog = GetCatalog();
	if (!Catalog)
	{
		return 0.f;
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return Equipment.ComputeGearScore(*Catalog);
	}
	// Clients rebuild a rules-core equipment from the replicated view to use the identical formula.
	R::FEquipment View;
	for (int32 SlotIndex = 0; SlotIndex < Equipped.Num() && SlotIndex < R::FEquipment::NumSlots; ++SlotIndex)
	{
		View.GetMutable(static_cast<R::EEquipSlot>(SlotIndex)) = DBBridge::MakeStack(Equipped[SlotIndex].ItemId, Equipped[SlotIndex].Count);
	}
	return View.ComputeGearScore(*Catalog);
}
