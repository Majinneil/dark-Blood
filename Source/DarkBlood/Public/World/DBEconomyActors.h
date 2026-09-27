// World objects of the item economy: crafting stations (forge ...) and loot chests. Both are interactables.
#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/DBInteractable.h"

#include "DBEconomyActors.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Opens the crafting window for recipes of StationId (and repairs gear). */
UCLASS()
class DARKBLOOD_API ADBCraftingStation : public AActor, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBCraftingStation();

	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override { return User != nullptr; }
	virtual void Interact(APlayerController* User) override;

	FName GetStationId() const { return StationId; }
	FText GetDisplayName() const { return DisplayName; }

	/** Development: configure a spawned station. */
	void Setup(FName InStationId, const FText& InDisplayName);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Identity();

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Identity, BlueprintReadOnly, Category = "Dark Blood|Crafting")
	FName StationId = TEXT("Forge");

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Identity, BlueprintReadOnly, Category = "Dark Blood|Crafting")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;
};

/** Each player can open it once; rolls the loot table for that player (personal loot). */
UCLASS()
class DARKBLOOD_API ADBLootChest : public AActor, public IDBInteractable
{
	GENERATED_BODY()

public:
	ADBLootChest();

	virtual FText GetInteractionText() const override;
	virtual bool CanInteract(const APawn* User) const override;
	virtual void Interact(APlayerController* User) override;

	void Setup(FName InLootTableId);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dark Blood|Loot")
	FName LootTableId;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

private:
	/** Character ids that already opened this chest (server). */
	TSet<FGuid> OpenedBy;
};
