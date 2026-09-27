#include "UI/SDBDialogueWidget.h"

#include "Dialogue/DBDialogueComponent.h"
#include "UI/DBUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DarkBloodDialogue"

void SDBDialogueWidget::Construct(const FArguments& InArgs)
{
	Dialogue = InArgs._Dialogue;

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(0.f, 0.f, 0.f, 48.f)
	[
		SNew(SBox)
		.WidthOverride(900.f)
		.Visibility_Lambda([this]()
		{
			return Dialogue.IsValid() && Dialogue->IsDialogueOpen() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SBorder)
			.BorderImage(DBUIStyle::WhiteBrush())
			.BorderBackgroundColor(DBUIStyle::PanelDark)
			.Padding(FMargin(28.f, 20.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(18, "Bold"))
					.ColorAndOpacity(DBUIStyle::Gold)
					.Text_Lambda([this]() { return Dialogue.IsValid() ? Dialogue->GetView().Speaker : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 14.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(16))
					.AutoWrapText(true)
					.Text_Lambda([this]() { return Dialogue.IsValid() ? Dialogue->GetView().Text : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(ChoiceBox, SVerticalBox)
				]
			]
		]
	];
	Refresh();
}

void SDBDialogueWidget::Refresh()
{
	if (!ChoiceBox.IsValid())
	{
		return;
	}
	ChoiceBox->ClearChildren();
	if (!Dialogue.IsValid() || !Dialogue->IsDialogueOpen())
	{
		return;
	}

	const FDBDialogueView& View = Dialogue->GetView();
	auto AddOption = [this](const FText& Label, int32 Index)
	{
		ChoiceBox->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(FLinearColor(0.15f, 0.08f, 0.06f))
			.ContentPadding(FMargin(12.f, 6.f))
			.OnClicked_Lambda([this, Index]()
			{
				if (Dialogue.IsValid())
				{
					Dialogue->Choose(Index);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(DBUIStyle::Font(15)).ColorAndOpacity(FLinearColor(0.92f, 0.88f, 0.8f)).Text(Label)
			]
		];
	};

	if (View.bEnds)
	{
		AddOption(LOCTEXT("Continue", "[E]  Weiter"), 0);
		return;
	}
	for (int32 Index = 0; Index < View.Choices.Num(); ++Index)
	{
		AddOption(FText::Format(LOCTEXT("Choice", "{0}.  {1}"), FText::AsNumber(Index + 1), View.Choices[Index]), Index);
	}
}

#undef LOCTEXT_NAMESPACE
