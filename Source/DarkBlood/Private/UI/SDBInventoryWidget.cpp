#include "UI/SDBInventoryWidget.h"

#include "AbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Core/DBRulesBridge.h"
#include "Data/DBEconomyDefinitions.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBItemDefinition.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/DBInventoryComponent.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "UI/DBUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "World/DBEconomyActors.h"

#include "DarkBloodRules/Crafting.h"

#define LOCTEXT_NAMESPACE "DarkBloodInventory"

namespace R = DarkBlood::Rules;

namespace
{
	const UDBItemDefinition* FindItem(const APlayerController* PC, FName ItemId)
	{
		const UDBGameDataSubsystem* Data = PC ? UDBGameDataSubsystem::Get(PC) : nullptr;
		return Data ? Data->FindItem(ItemId) : nullptr;
	}

	FLinearColor RarityColor(EDBItemRarity Rarity)
	{
		switch (Rarity)
		{
		case EDBItemRarity::Uncommon: return FLinearColor(0.4f, 0.9f, 0.4f);
		case EDBItemRarity::Rare: return FLinearColor(0.35f, 0.6f, 1.f);
		case EDBItemRarity::Epic: return FLinearColor(0.75f, 0.4f, 1.f);
		case EDBItemRarity::Legendary: return FLinearColor(1.f, 0.65f, 0.15f);
		case EDBItemRarity::Demonic: return FLinearColor(0.95f, 0.15f, 0.15f);
		default: return FLinearColor(0.9f, 0.88f, 0.84f);
		}
	}

	FText SlotName(EDBEquipSlot Slot)
	{
		switch (Slot)
		{
		case EDBEquipSlot::MainHand: return LOCTEXT("MainHand", "Waffe");
		case EDBEquipSlot::OffHand: return LOCTEXT("OffHand", "Nebenhand");
		case EDBEquipSlot::Head: return LOCTEXT("Head", "Kopf");
		case EDBEquipSlot::Chest: return LOCTEXT("Chest", "Brust");
		case EDBEquipSlot::Hands: return LOCTEXT("Hands", "Haende");
		case EDBEquipSlot::Legs: return LOCTEXT("Legs", "Beine");
		case EDBEquipSlot::Feet: return LOCTEXT("Feet", "Fuesse");
		case EDBEquipSlot::Accessory1: return LOCTEXT("Acc1", "Schmuck 1");
		case EDBEquipSlot::Accessory2: return LOCTEXT("Acc2", "Schmuck 2");
		default: return FText::GetEmpty();
		}
	}

	FString ItemLabel(const APlayerController* PC, const FDBItemStackView& Stack)
	{
		const UDBItemDefinition* Definition = FindItem(PC, Stack.ItemId);
		FString Label = Definition ? Definition->DisplayName.ToString() : Stack.ItemId.ToString();
		if (Stack.Count > 1)
		{
			Label = FString::Printf(TEXT("%dx %s"), Stack.Count, *Label);
		}
		if (Stack.Durability >= 0 && Definition && Definition->MaxDurability > 0)
		{
			Label += FString::Printf(TEXT("   (%d/%d)"), Stack.Durability, Definition->MaxDurability);
		}
		return Label;
	}

	TSharedRef<SWidget> SmallButton(const FText& Label, TFunction<void()> OnClick)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(8.f, 2.f))
			.ButtonColorAndOpacity(FLinearColor(0.35f, 0.08f, 0.06f))
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Font(DBUIStyle::Font(12, "Bold")).Text(Label)
			];
	}
}

// ==== Inventory ======================================================================================

void SDBInventoryWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(1150.f)
		.HeightOverride(720.f)
		[
			SNew(SBorder)
			.BorderImage(DBUIStyle::WhiteBrush())
			.BorderBackgroundColor(DBUIStyle::PanelDark)
			.Padding(22.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Font(DBUIStyle::Font(24, "Bold")).ColorAndOpacity(DBUIStyle::Gold).Text(LOCTEXT("Title", "Inventar"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(DBUIStyle::Font(16, "Bold"))
						.Text_Lambda([this]()
						{
							const UDBInventoryComponent* Inventory = GetInventory();
							return FText::Format(LOCTEXT("Currency", "{0} Mon    [I] schliessen"), FText::AsNumber(Inventory ? Inventory->GetCurrency() : 0));
						})
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SHorizontalBox)
					// Equipment + stats
					+ SHorizontalBox::Slot().FillWidth(0.42f).Padding(0.f, 0.f, 16.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(DBUIStyle::Font(16, "Bold")).Text(LOCTEXT("Equipment", "Ausruestung"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
						[
							SAssignNew(EquipmentBox, SVerticalBox)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
						[
							SNew(STextBlock).Font(DBUIStyle::Font(16, "Bold")).Text(LOCTEXT("Stats", "Werte"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
						[
							SNew(STextBlock).Font(DBUIStyle::Font(13)).Text_Lambda([this]() { return GetStatsText(); })
						]
					]
					// Bags and items
					+ SHorizontalBox::Slot().FillWidth(0.58f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(ItemsBox, SVerticalBox)
						]
					]
				]
			]
		]
	];
	Rebuild();
}

UDBInventoryComponent* SDBInventoryWidget::GetInventory() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerState* PS = PC ? PC->GetPlayerState<ADBPlayerState>() : nullptr;
	return PS ? PS->GetInventory() : nullptr;
}

FString SDBInventoryWidget::ComputeSignature() const
{
	const UDBInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return FString();
	}
	FString Signature;
	for (const FDBInventoryEntry& Entry : Inventory->GetEntries())
	{
		Signature += FString::Printf(TEXT("%d.%d:%s:%d:%d|"), Entry.Section, Entry.SlotIndex, *Entry.Stack.ItemId.ToString(), Entry.Stack.Count,
			Entry.Stack.Durability);
	}
	for (int32 Slot = 1; Slot <= static_cast<int32>(EDBEquipSlot::Accessory2); ++Slot)
	{
		const FDBItemStackView Equipped = Inventory->GetEquipped(static_cast<EDBEquipSlot>(Slot));
		Signature += FString::Printf(TEXT("E%d:%s:%d|"), Slot, *Equipped.ItemId.ToString(), Equipped.Durability);
	}
	for (const FDBInventorySectionView& Section : Inventory->GetSections())
	{
		Signature += FString::Printf(TEXT("S%s:%d|"), *Section.BagItemId.ToString(), Section.Capacity);
	}
	return Signature;
}

void SDBInventoryWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (ComputeSignature() != LastSignature)
	{
		Rebuild();
	}
}

void SDBInventoryWidget::Rebuild()
{
	LastSignature = ComputeSignature();
	UDBInventoryComponent* Inventory = GetInventory();
	const APlayerController* PC = Owner.Get();
	if (!EquipmentBox.IsValid() || !ItemsBox.IsValid() || !Inventory)
	{
		return;
	}
	const TWeakObjectPtr<UDBInventoryComponent> WeakInventory = Inventory;

	EquipmentBox->ClearChildren();
	for (int32 SlotIndex = 1; SlotIndex <= static_cast<int32>(EDBEquipSlot::Accessory2); ++SlotIndex)
	{
		const EDBEquipSlot Slot = static_cast<EDBEquipSlot>(SlotIndex);
		const FDBItemStackView Equipped = Inventory->GetEquipped(Slot);
		const UDBItemDefinition* Definition = FindItem(PC, Equipped.ItemId);
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(90.f)[SNew(STextBlock).Font(DBUIStyle::Font(13)).ColorAndOpacity(FLinearColor(0.7f, 0.68f, 0.65f)).Text(SlotName(Slot))]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(DBUIStyle::Font(13))
				.ColorAndOpacity(Definition ? RarityColor(Definition->Rarity) : FLinearColor(0.4f, 0.4f, 0.4f))
				.Text(FText::FromString(Equipped.ItemId.IsNone() ? TEXT("-") : ItemLabel(PC, Equipped)))
			];
		if (!Equipped.ItemId.IsNone())
		{
			Row->AddSlot().AutoWidth()[SmallButton(LOCTEXT("Unequip", "Ablegen"), [WeakInventory, Slot]()
			{
				if (WeakInventory.IsValid())
				{
					WeakInventory->RequestUnequipItem(Slot);
				}
			})];
		}
		EquipmentBox->AddSlot().AutoHeight().Padding(0.f, 2.f)[Row];
	}

	ItemsBox->ClearChildren();
	const TArray<FDBInventorySectionView> Sections = Inventory->GetSections();
	const TArray<FDBInventoryEntry> Entries = Inventory->GetEntries();
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		const FDBInventorySectionView& Section = Sections[SectionIndex];
		if (Section.Capacity <= 0)
		{
			continue;
		}
		int32 Used = 0;
		for (const FDBInventoryEntry& Entry : Entries)
		{
			Used += Entry.Section == SectionIndex ? 1 : 0;
		}
		const UDBItemDefinition* Bag = FindItem(PC, Section.BagItemId);
		const FString Title = SectionIndex == 0 ? TEXT("Grundbeutel") : (Bag ? Bag->DisplayName.ToString() : TEXT("Tasche"));
		ItemsBox->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 4.f)
		[
			SNew(STextBlock).Font(DBUIStyle::Font(15, "Bold")).ColorAndOpacity(DBUIStyle::Gold)
			.Text(FText::FromString(FString::Printf(TEXT("%s  (%d/%d)"), *Title, Used, Section.Capacity)))
		];
		for (const FDBInventoryEntry& Entry : Entries)
		{
			if (Entry.Section != SectionIndex)
			{
				continue;
			}
			const UDBItemDefinition* Definition = FindItem(PC, Entry.Stack.ItemId);
			FDBSlotRef Ref;
			Ref.Section = Entry.Section;
			Ref.Index = Entry.SlotIndex;
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(13))
					.ColorAndOpacity(Definition ? RarityColor(Definition->Rarity) : FLinearColor::White)
					.Text(FText::FromString(ItemLabel(PC, Entry.Stack)))
					.ToolTipText(Definition ? Definition->Description : FText::GetEmpty())
				];
			if (Definition && (Definition->HealAmount > 0.f || Definition->StaminaAmount > 0.f || Definition->ManaAmount > 0.f))
			{
				Row->AddSlot().AutoWidth().Padding(4.f, 0.f)[SmallButton(LOCTEXT("Use", "Benutzen"), [WeakInventory, Ref]()
				{
					if (WeakInventory.IsValid())
					{
						WeakInventory->RequestUseItem(Ref);
					}
				})];
			}
			if (Definition && Definition->EquipSlot != EDBEquipSlot::None)
			{
				EDBEquipSlot Target = Definition->EquipSlot;
				if (Target == EDBEquipSlot::Accessory1 && !Inventory->GetEquipped(EDBEquipSlot::Accessory1).ItemId.IsNone())
				{
					Target = EDBEquipSlot::Accessory2;
				}
				Row->AddSlot().AutoWidth().Padding(4.f, 0.f)[SmallButton(LOCTEXT("Equip", "Anlegen"), [WeakInventory, Ref, Target]()
				{
					if (WeakInventory.IsValid())
					{
						WeakInventory->RequestEquipItem(Ref, Target);
					}
				})];
			}
			if (Definition && Definition->bIsBag)
			{
				Row->AddSlot().AutoWidth().Padding(4.f, 0.f)[SmallButton(LOCTEXT("EquipBag", "Tasche anlegen"), [WeakInventory, Ref]()
				{
					if (WeakInventory.IsValid())
					{
						WeakInventory->RequestEquipBag(Ref);
					}
				})];
			}
			ItemsBox->AddSlot().AutoHeight().Padding(8.f, 2.f)[Row];
		}
	}
}

FText SDBInventoryWidget::GetStatsText() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerState* PS = PC ? PC->GetPlayerState<ADBPlayerState>() : nullptr;
	const UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return FText::GetEmpty();
	}
	auto Get = [ASC](const FGameplayAttribute& Attribute) { return ASC->GetNumericAttribute(Attribute); };
	return FText::FromString(FString::Printf(
		TEXT("Stufe %d   Staerke %d\nLeben %.0f   Ausdauer %.0f   Mana %.0f\nAngriffskraft %.0f   Zauberkraft %.0f\nRuestung %.0f   Krit %.0f %%\n"
			 "Resistenz: Feuer %.0f %%  Frost %.0f %%  Blitz %.0f %%\n    Schatten %.0f %%  Gift %.0f %%  Geist %.0f %%"),
		PS->GetProgression()->GetLevel(), PS->GetProgression()->GetPowerRating(), Get(UDBAttributeSet::GetMaxHealthAttribute()),
		Get(UDBAttributeSet::GetMaxStaminaAttribute()), Get(UDBAttributeSet::GetMaxManaAttribute()), Get(UDBAttributeSet::GetAttackPowerAttribute()),
		Get(UDBAttributeSet::GetSpellPowerAttribute()), Get(UDBAttributeSet::GetArmorAttribute()), Get(UDBAttributeSet::GetCritChanceAttribute()) * 100.f,
		Get(UDBAttributeSet::GetResistFireAttribute()) * 100.f, Get(UDBAttributeSet::GetResistFrostAttribute()) * 100.f,
		Get(UDBAttributeSet::GetResistLightningAttribute()) * 100.f, Get(UDBAttributeSet::GetResistShadowAttribute()) * 100.f,
		Get(UDBAttributeSet::GetResistPoisonAttribute()) * 100.f, Get(UDBAttributeSet::GetResistSpiritAttribute()) * 100.f));
}

// ==== Crafting =======================================================================================

void SDBCraftingWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	Station = InArgs._Station;
	OnClose = InArgs._OnClose;

	const APlayerController* PC = Owner.Get();
	const UDBGameDataSubsystem* Data = PC ? UDBGameDataSubsystem::Get(PC) : nullptr;
	const ADBCraftingStation* Crafting = Cast<ADBCraftingStation>(Station.Get());
	const FName StationId = Crafting ? Crafting->GetStationId() : NAME_None;

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (const UDBRecipeDefinition* Recipe : Data ? Data->GetAllRecipes() : TArray<UDBRecipeDefinition*>())
	{
		if (!Recipe->StationId.IsNone() && Recipe->StationId != StationId)
		{
			continue;
		}
		const TWeakObjectPtr<const UDBRecipeDefinition> WeakRecipe = Recipe;
		const UDBItemDefinition* Output = Data->FindItem(Recipe->OutputItemId);
		Rows->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			SNew(SBorder)
			.BorderImage(DBUIStyle::WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.07f, 0.9f))
			.Padding(10.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(DBUIStyle::Font(15, "Bold"))
						.ColorAndOpacity(Output ? RarityColor(Output->Rarity) : FLinearColor::White)
						.Text(FText::FromString(FString::Printf(TEXT("%s%s   (ab Stufe %d)"), Recipe->OutputCount > 1 ? *FString::Printf(TEXT("%dx "), Recipe->OutputCount) : TEXT(""),
							Output ? *Output->DisplayName.ToString() : *Recipe->OutputItemId.ToString(), Recipe->RequiredLevel)))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(DBUIStyle::Font(12))
						.Text_Lambda([this, WeakRecipe, PC]()
						{
							const UDBInventoryComponent* Inventory = GetInventory();
							const UDBRecipeDefinition* Current = WeakRecipe.Get();
							if (!Inventory || !Current)
							{
								return FText::GetEmpty();
							}
							TArray<FString> Parts;
							for (const FDBRecipeIngredient& Input : Current->Inputs)
							{
								const UDBItemDefinition* Item = FindItem(PC, Input.ItemId);
								Parts.Add(FString::Printf(TEXT("%s %d/%d"), Item ? *Item->DisplayName.ToString() : *Input.ItemId.ToString(),
									Inventory->CountItem(Input.ItemId), Input.Count));
							}
							if (Current->CurrencyCost > 0)
							{
								Parts.Add(FString::Printf(TEXT("%lld Mon"), Current->CurrencyCost));
							}
							return FText::FromString(FString::Join(Parts, TEXT(",   ")));
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ContentPadding(FMargin(14.f, 6.f))
					.ButtonColorAndOpacity(FLinearColor(0.55f, 0.1f, 0.08f))
					.IsEnabled_Lambda([this, WeakRecipe, StationId]()
					{
						const UDBInventoryComponent* Inventory = GetInventory();
						const UDBRecipeDefinition* Current = WeakRecipe.Get();
						if (!Inventory || !Current || GetPlayerLevel() < Current->RequiredLevel || Inventory->GetCurrency() < Current->CurrencyCost)
						{
							return false;
						}
						for (const FDBRecipeIngredient& Input : Current->Inputs)
						{
							if (Inventory->CountItem(Input.ItemId) < Input.Count)
							{
								return false;
							}
						}
						return true;
					})
					.OnClicked_Lambda([this, WeakRecipe]()
					{
						if (UDBInventoryComponent* Inventory = GetInventory(); Inventory && WeakRecipe.IsValid())
						{
							Inventory->RequestCraft(WeakRecipe->RecipeId, Station.Get());
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(DBUIStyle::Font(14, "Bold")).Text(LOCTEXT("Craft", "Herstellen"))
					]
				]
			]
		];
	}

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(900.f)
		.MaxDesiredHeight(740.f)
		[
			SNew(SBorder)
			.BorderImage(DBUIStyle::WhiteBrush())
			.BorderBackgroundColor(DBUIStyle::PanelDark)
			.Padding(22.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Font(DBUIStyle::Font(24, "Bold")).ColorAndOpacity(DBUIStyle::Gold)
						.Text(Crafting ? Crafting->GetDisplayName() : LOCTEXT("Crafting", "Herstellung"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Font(DBUIStyle::Font(15, "Bold")).Text_Lambda([this]()
						{
							const UDBInventoryComponent* Inventory = GetInventory();
							return FText::Format(LOCTEXT("Mon", "{0} Mon"), FText::AsNumber(Inventory ? Inventory->GetCurrency() : 0));
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 12.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ContentPadding(FMargin(14.f, 6.f))
						.ButtonColorAndOpacity(FLinearColor(0.25f, 0.2f, 0.12f))
						.IsEnabled_Lambda([this]() { const int64 Cost = GetRepairCost(); const UDBInventoryComponent* I = GetInventory(); return Cost > 0 && I && I->GetCurrency() > 0; })
						.OnClicked_Lambda([this]()
						{
							if (UDBInventoryComponent* Inventory = GetInventory())
							{
								Inventory->RequestRepairAll(Station.Get());
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Font(DBUIStyle::Font(14, "Bold")).Text_Lambda([this]()
							{
								const int64 Cost = GetRepairCost();
								return Cost > 0 ? FText::Format(LOCTEXT("Repair", "Alles reparieren ({0} Mon)"), FText::AsNumber(Cost))
												: LOCTEXT("NoRepair", "Nichts zu reparieren");
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ContentPadding(FMargin(14.f, 6.f))
						.OnClicked_Lambda([this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); })
						[
							SNew(STextBlock).Font(DBUIStyle::Font(14)).Text(LOCTEXT("Close", "Schliessen"))
						]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[Rows]
				]
			]
		]
	];
}

UDBInventoryComponent* SDBCraftingWidget::GetInventory() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerState* PS = PC ? PC->GetPlayerState<ADBPlayerState>() : nullptr;
	return PS ? PS->GetInventory() : nullptr;
}

int32 SDBCraftingWidget::GetPlayerLevel() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerState* PS = PC ? PC->GetPlayerState<ADBPlayerState>() : nullptr;
	return PS ? PS->GetProgression()->GetLevel() : 1;
}

int64 SDBCraftingWidget::GetRepairCost() const
{
	const UDBInventoryComponent* Inventory = GetInventory();
	const APlayerController* PC = Owner.Get();
	if (!Inventory)
	{
		return 0;
	}
	int64 Total = 0;
	for (int32 Slot = 1; Slot <= static_cast<int32>(EDBEquipSlot::Accessory2); ++Slot)
	{
		const FDBItemStackView Equipped = Inventory->GetEquipped(static_cast<EDBEquipSlot>(Slot));
		if (const UDBItemDefinition* Definition = FindItem(PC, Equipped.ItemId))
		{
			Total += R::RepairCost(Definition->ToRules(), Equipped.Durability);
		}
	}
	return Total;
}

#undef LOCTEXT_NAMESPACE
