// Inventory, bags, equipment and currency of one character (on the PlayerState, so it survives death).
//
// Authority model: the server owns a DarkBlood::Rules::FInventory/FEquipment and performs every
// mutation through the rules core. Clients only see a replicated view and send requests via RPCs.
#pragma once

#include "Components/ActorComponent.h"
#include "Core/DBTypes.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "DarkBloodRules/Crafting.h"
#include "DarkBloodRules/Equipment.h"

#include "DBInventoryComponent.generated.h"

class UDBInventoryComponent;

USTRUCT(BlueprintType)
struct FDBItemStackView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Count = 0;

	/** Unique instance id (0 for stackables). int64 for Blueprint compatibility. */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int64 InstanceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Durability = -1;

	bool operator==(const FDBItemStackView& Other) const
	{
		return ItemId == Other.ItemId && Count == Other.Count && InstanceId == Other.InstanceId && Durability == Other.Durability;
	}
};

USTRUCT(BlueprintType)
struct FDBInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Section index; -1 = quest items. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Section = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FDBItemStackView Stack;

	void PostReplicatedAdd(const struct FDBInventoryList& InArraySerializer);
	void PostReplicatedChange(const struct FDBInventoryList& InArraySerializer);
	void PreReplicatedRemove(const struct FDBInventoryList& InArraySerializer);
};

USTRUCT()
struct FDBInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDBInventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UDBInventoryComponent> Owner = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FDBInventoryEntry, FDBInventoryList>(Entries, DeltaParms, *this);
	}
};

template <>
struct TStructOpsTypeTraits<FDBInventoryList> : public TStructOpsTypeTraitsBase2<FDBInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

USTRUCT(BlueprintType)
struct FDBInventorySectionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Capacity = 0;

	/** Bag providing the section (None for the base pouch / empty bag slot). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName BagItemId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDBOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDBOnInventoryRequestFailed, FName, Operation, FString, Reason);

UCLASS(ClassGroup = (DarkBlood), meta = (BlueprintSpawnableComponent))
class DARKBLOOD_API UDBInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDBInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void InitializeComponent() override;

	// ---- Server API ---------------------------------------------------------------------------

	/** Adds items (creates unique instances for items with durability). Returns the amount added. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Inventory")
	int32 AddItem(FName ItemId, int32 Count, bool bAllowPartial = false);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Inventory")
	bool RemoveItem(FName ItemId, int32 Count);

	/** Adds a reward; whatever does not fit is queued as a pending delivery instead of being lost. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Inventory")
	void DeliverItem(FName ItemId, int32 Count);

	/** Moves pending deliveries into the inventory as far as space allows. */
	void TryDeliverPending();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Inventory")
	void AddCurrency(int64 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dark Blood|Inventory")
	bool SpendCurrency(int64 Amount);

	/** Applies the moderate death penalty. Inventory items are never lost. Returns the currency lost. */
	int64 ApplyDeathPenalty();

	/** Server: rolls a loot table for this player and delivers it (full bags -> pending deliveries). */
	void GrantLootTable(FName LootTableId, const FString& SourceName);

	/** Server: delivers already rolled loot and tells the player what they got. */
	void GrantLoot(const DarkBlood::Rules::FLootResult& Loot, const FString& SourceName);

	void RestoreFromRecord(const DarkBlood::Rules::FInventory& InInventory, const DarkBlood::Rules::FEquipment& InEquipment, int64 InCurrency,
		const std::vector<DarkBlood::Rules::FItemStack>& InPendingDeliveries);
	const std::vector<DarkBlood::Rules::FItemStack>& GetPendingDeliveries() const { return PendingDeliveries; }
	const DarkBlood::Rules::FInventory& GetRulesInventory() const { return Inventory; }
	const DarkBlood::Rules::FEquipment& GetRulesEquipment() const { return Equipment; }
	int64 GetServerCurrency() const { return ServerCurrency; }

	// ---- Client requests (validated on the server) ---------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Inventory")
	void RequestMoveItem(FDBSlotRef From, FDBSlotRef To, int32 Count = 0) { ServerMoveItem(From, To, Count); }

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Inventory")
	void RequestEquipBag(FDBSlotRef BagSlot) { ServerEquipBag(BagSlot); }

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Inventory")
	void RequestUnequipBag(EDBBagKind Kind) { ServerUnequipBag(Kind); }

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Inventory")
	void RequestEquipItem(FDBSlotRef From, EDBEquipSlot Slot) { ServerEquipItem(From, Slot); }

	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Inventory")
	void RequestUnequipItem(EDBEquipSlot Slot) { ServerUnequipItem(Slot); }

	/** Eat / drink a consumable. */
	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Inventory")
	void RequestUseItem(FDBSlotRef Slot) { ServerUseItem(Slot); }

	/** Craft at a station (the server checks distance and station id). */
	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Inventory")
	void RequestCraft(FName RecipeId, AActor* Station) { ServerCraft(RecipeId, Station); }

	/** Repair all equipped gear at a station (as far as the Mon last). */
	UFUNCTION(BlueprintCallable, Category = "Dark Blood|Inventory")
	void RequestRepairAll(AActor* Station) { ServerRepairAll(Station); }

	// ---- Replicated view ----------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Inventory")
	TArray<FDBInventoryEntry> GetEntries() const { return ReplicatedItems.Entries; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Inventory")
	TArray<FDBInventorySectionView> GetSections() const { return Sections; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Inventory")
	FDBItemStackView GetEquipped(EDBEquipSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Inventory")
	int64 GetCurrency() const { return Currency; }

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Inventory")
	int32 CountItem(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Dark Blood|Inventory")
	int32 GetPendingDeliveryCount() const { return PendingDeliveryCount; }

	/** Average item level of equipped gear (works on server and clients). */
	float ComputeGearScore() const;

	/** Player level (for recipe requirements). */
	int32 GetOwnerLevel() const;

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|Inventory")
	FDBOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Dark Blood|Inventory")
	FDBOnInventoryRequestFailed OnRequestFailed;

	void BroadcastChanged() { OnInventoryChanged.Broadcast(); }

private:
	UFUNCTION(Server, Reliable) void ServerMoveItem(FDBSlotRef From, FDBSlotRef To, int32 Count);
	UFUNCTION(Server, Reliable) void ServerEquipBag(FDBSlotRef BagSlot);
	UFUNCTION(Server, Reliable) void ServerUnequipBag(EDBBagKind Kind);
	UFUNCTION(Server, Reliable) void ServerEquipItem(FDBSlotRef From, EDBEquipSlot Slot);
	UFUNCTION(Server, Reliable) void ServerUnequipItem(EDBEquipSlot Slot);
	UFUNCTION(Server, Reliable) void ServerUseItem(FDBSlotRef Slot);
	UFUNCTION(Server, Reliable) void ServerCraft(FName RecipeId, AActor* Station);
	UFUNCTION(Server, Reliable) void ServerRepairAll(AActor* Station);

	/** Station id if Station is a crafting station within reach of the owner's pawn; None otherwise. */
	FName ValidateStation(AActor* Station) const;
	void NotifyOwner(const FText& Text) const;
	void RecalculateOwnerStats() const;

	UFUNCTION(Client, Reliable) void ClientRequestFailed(FName Operation, const FString& Reason);

	UFUNCTION() void OnRep_View();

	const DarkBlood::Rules::FItemCatalog* GetCatalog() const;
	DarkBlood::Rules::FEquipContext MakeEquipContext() const;
	/** Common tail of every server mutation: report result, refresh view, update dependent stats. */
	void FinishRequest(FName Operation, DarkBlood::Rules::EInventoryResult Result, bool bGearChanged);
	void SyncReplicatedView();

	DarkBlood::Rules::FInventory Inventory;
	DarkBlood::Rules::FEquipment Equipment;
	int64 ServerCurrency = 0;
	std::vector<DarkBlood::Rules::FItemStack> PendingDeliveries;

	UPROPERTY(Replicated)
	FDBInventoryList ReplicatedItems;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	TArray<FDBInventorySectionView> Sections;

	/** Visible to everyone (character visuals). Index = EDBEquipSlot. */
	UPROPERTY(ReplicatedUsing = OnRep_View)
	TArray<FDBItemStackView> Equipped;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	int64 Currency = 0;

	UPROPERTY(ReplicatedUsing = OnRep_View)
	int32 PendingDeliveryCount = 0;
};
