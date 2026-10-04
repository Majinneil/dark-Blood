// Inventory window ([I]): equipment, bags with their items (use / equip), currency and character stats.
// Rebuilds itself when the replicated inventory changes; every action is a validated server request.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class SVerticalBox;
class UDBInventoryComponent;

class SDBInventoryWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBInventoryWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	UDBInventoryComponent* GetInventory() const;
	FString ComputeSignature() const;
	void Rebuild();
	FText GetStatsText() const;

	TWeakObjectPtr<APlayerController> Owner;
	TSharedPtr<SVerticalBox> EquipmentBox;
	TSharedPtr<SVerticalBox> ItemsBox;
	FString LastSignature;
};

/** Crafting window of a station: recipes with ingredient counts, craft buttons and repair. */
class SDBCraftingWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBCraftingWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
		SLATE_ARGUMENT(TWeakObjectPtr<AActor>, Station)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	AActor* GetStation() const { return Station.Get(); }

private:
	UDBInventoryComponent* GetInventory() const;
	int32 GetPlayerLevel() const;
	int64 GetRepairCost() const;

	TWeakObjectPtr<APlayerController> Owner;
	TWeakObjectPtr<AActor> Station;
	FSimpleDelegate OnClose;
};
