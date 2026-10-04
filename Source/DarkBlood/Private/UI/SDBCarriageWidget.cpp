#include "UI/SDBCarriageWidget.h"

#include "Inventory/DBInventoryComponent.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "UI/DBUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "World/DBCarriageStation.h"
#include "World/DBRealmLayout.h"

#define LOCTEXT_NAMESPACE "DarkBloodCarriageWidget"

void SDBCarriageWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	Station = InArgs._Station;
	OnClose = InArgs._OnClose;

	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	const int32 From = Station.IsValid() ? Station->GetSiteIndex() : INDEX_NONE;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (int32 Index = 0; Index < Settlements.Num(); ++Index)
	{
		if (Index == From)
		{
			continue;
		}
		const int64 Fare = ADBCarriageStation::GetFare(From, Index);
		const float Hours = ADBCarriageStation::GetTravelHours(From, Index);
		FNumberFormattingOptions OneDecimal;
		OneDecimal.MaximumFractionalDigits = 1;
		const FText Label = FText::Format(LOCTEXT("Destination", "{0}   -   {1} Mon   -   {2} Std."), FText::FromString(Settlements[Index].Name),
			FText::AsNumber(Fare), FText::AsNumber(Hours, &OneDecimal));
		Rows->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SButton)
			.OnClicked_Lambda([this, Index]()
			{
				if (ADBPlayerController* Controller = Cast<ADBPlayerController>(Owner.Get()); Controller && Station.IsValid())
				{
					Controller->ServerTravelByCarriage(Station.Get(), Index);
				}
				OnClose.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(DBUIStyle::Font(14)).Text(Label)
			]
		];
	}

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(620.f)
		.MaxDesiredHeight(720.f)
		[
			SNew(SBorder)
			.BorderImage(DBUIStyle::WhiteBrush())
			.BorderBackgroundColor(DBUIStyle::PanelDark)
			.Padding(20.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(22, "Bold"))
					.ColorAndOpacity(DBUIStyle::Gold)
					.Text(FText::Format(LOCTEXT("Title", "Kutsche ab {0}"),
						FText::FromString(Settlements.IsValidIndex(From) ? Settlements[From].Name : TEXT("?"))))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 12.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(14))
					.Text_Lambda([this]()
					{
						const ADBPlayerState* PlayerState = Owner.IsValid() ? Owner->GetPlayerState<ADBPlayerState>() : nullptr;
						const int64 Mon = PlayerState && PlayerState->GetInventory() ? PlayerState->GetInventory()->GetCurrency() : 0;
						return FText::Format(LOCTEXT("Purse", "Deine Boerse: {0} Mon. Allein vergeht die Reisezeit."), FText::AsNumber(Mon));
					})
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[Rows]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 12.f, 0.f, 0.f)
				[
					SNew(SButton)
					.OnClicked_Lambda([this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); })
					[
						SNew(STextBlock).Font(DBUIStyle::Font(14, "Bold")).Text(LOCTEXT("Close", "Schliessen"))
					]
				]
			]
		]
	];
}

AActor* SDBCarriageWidget::GetStation() const
{
	return Station.Get();
}

#undef LOCTEXT_NAMESPACE
