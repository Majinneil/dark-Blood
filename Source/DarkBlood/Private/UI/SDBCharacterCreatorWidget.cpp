#include "UI/SDBCharacterCreatorWidget.h"

#include "UI/DBUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DarkBloodCreator"

namespace
{
	const TArray<FText> HairStyles = {LOCTEXT("HairTopknot", "Chonmage (Knoten)"), LOCTEXT("HairShort", "Kurz"), LOCTEXT("HairLong", "Lang, offen"),
		LOCTEXT("HairTail", "Pferdeschwanz"), LOCTEXT("HairWild", "Wild"), LOCTEXT("HairBald", "Geschoren")};
	const TArray<FText> HairColors = {LOCTEXT("HairBlack", "Schwarz"), LOCTEXT("HairBrown", "Dunkelbraun"), LOCTEXT("HairGrey", "Grau"),
		LOCTEXT("HairWhite", "Weiss"), LOCTEXT("HairBlood", "Blutrot")};
	const TArray<FColor> HairColorValues = {FColor(22, 20, 20), FColor(52, 34, 24), FColor(110, 106, 102), FColor(220, 216, 208), FColor(110, 14, 16)};
	const TArray<FText> EyeColors = {LOCTEXT("EyeBrown", "Braun"), LOCTEXT("EyeDark", "Dunkelbraun"), LOCTEXT("EyeGrey", "Grau"),
		LOCTEXT("EyeAmber", "Bernstein"), LOCTEXT("EyeRed", "Rot (Dunkles Blut)")};
	const TArray<FColor> EyeColorValues = {FColor(74, 48, 30), FColor(40, 26, 18), FColor(120, 124, 128), FColor(170, 110, 30), FColor(170, 20, 18)};
	const TArray<FText> SkinTones = {LOCTEXT("SkinPale", "Blass"), LOCTEXT("SkinLight", "Hell"), LOCTEXT("SkinWarm", "Warm"),
		LOCTEXT("SkinOlive", "Oliv"), LOCTEXT("SkinTan", "Gebraeunt"), LOCTEXT("SkinDark", "Dunkel")};
	const TArray<FText> Scars = {LOCTEXT("ScarNone", "Keine"), LOCTEXT("ScarEye", "Ueber dem Auge"), LOCTEXT("ScarCheek", "Wange"),
		LOCTEXT("ScarNeck", "Hals"), LOCTEXT("ScarCross", "Kreuznarbe")};
}

TSharedRef<SWidget> SDBCharacterCreatorWidget::MakeChoiceRow(const FText& Label, const TArray<FText>* Options, int32* Value)
{
	auto Arrow = [Options, Value](const TCHAR* Glyph, int32 Step)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(10.f, 2.f))
			.ButtonColorAndOpacity(FLinearColor(0.12f, 0.1f, 0.1f))
			.OnClicked_Lambda([Options, Value, Step]()
			{
				*Value = (*Value + Step + Options->Num()) % Options->Num();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(DBUIStyle::Font(14, "Bold")).Text(FText::FromString(Glyph))
			];
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(DBUIStyle::Font(13)).ColorAndOpacity(FLinearColor(0.75f, 0.72f, 0.68f)).Text(Label)
		]
		+ SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT("<"), -1)]
		+ SHorizontalBox::Slot().FillWidth(0.58f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(DBUIStyle::Font(14)).Text_Lambda([Options, Value]() { return (*Options)[*Value]; })
		]
		+ SHorizontalBox::Slot().AutoWidth()[Arrow(TEXT(">"), 1)];
}

void SDBCharacterCreatorWidget::Construct(const FArguments& InArgs)
{
	Classes = InArgs._Classes;
	OnCreate = InArgs._OnCreate;
	SelectedClass = Classes.IsEmpty() ? NAME_None : Classes[0].ClassId;

	TSharedRef<SHorizontalBox> ClassRow = SNew(SHorizontalBox);
	for (const FDBCreatorClassOption& Option : Classes)
	{
		const FName ClassId = Option.ClassId;
		ClassRow->AddSlot().FillWidth(1.f).Padding(4.f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(10.f, 12.f))
			.HAlign(HAlign_Center)
			.ButtonColorAndOpacity_Lambda([this, ClassId]()
			{
				return SelectedClass == ClassId ? FLinearColor(0.55f, 0.1f, 0.08f) : FLinearColor(0.12f, 0.1f, 0.1f);
			})
			.OnClicked_Lambda([this, ClassId]()
			{
				SelectedClass = ClassId;
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(DBUIStyle::Font(14, "Bold")).Text(Option.DisplayName)
			]
		];
	}

	auto BodyButton = [this](EDBBodyType Type, const FText& Label)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(16.f, 8.f))
			.ButtonColorAndOpacity_Lambda([this, Type]()
			{
				return BodyType == Type ? FLinearColor(0.55f, 0.1f, 0.08f) : FLinearColor(0.12f, 0.1f, 0.1f);
			})
			.OnClicked_Lambda([this, Type]()
			{
				BodyType = Type;
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(DBUIStyle::Font(14)).Text(Label)
			];
	};

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(DBUIStyle::WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.01f, 0.f, 0.f, 0.92f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(980.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(DBUIStyle::Font(34, "Bold")).ColorAndOpacity(DBUIStyle::Blood).Text(LOCTEXT("Title", "DARK BLOOD"))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f, 0.f, 28.f)
				[
					SNew(STextBlock).Font(DBUIStyle::Font(16)).ColorAndOpacity(DBUIStyle::Gold).Text(LOCTEXT("Subtitle", "Erschaffe deinen Charakter"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(DBUIStyle::Font(14)).Text(LOCTEXT("Name", "Name (frei waehlbar, z. B. \"Jin Akagi\")"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 20.f)
				[
					SAssignNew(NameBox, SEditableTextBox)
					.Font(DBUIStyle::Font(18))
					.HintText(LOCTEXT("NameHint", "Charaktername"))
					.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Type)
					{
						if (Type == ETextCommit::OnEnter)
						{
							Submit();
						}
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(DBUIStyle::Font(14)).Text(LOCTEXT("Class", "Klasse"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(-4.f, 6.f, -4.f, 6.f)
				[
					ClassRow
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 20.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(13))
					.ColorAndOpacity(FLinearColor(0.75f, 0.72f, 0.68f))
					.AutoWrapText(true)
					.Text_Lambda([this]()
					{
						const FDBCreatorClassOption* Option = Classes.FindByPredicate([this](const FDBCreatorClassOption& O) { return O.ClassId == SelectedClass; });
						return Option ? Option->Description : FText::GetEmpty();
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(DBUIStyle::Font(14)).Text(LOCTEXT("Body", "Koerper"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 14.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)[BodyButton(EDBBodyType::TypeA, LOCTEXT("BodyA", "Typ A"))]
					+ SHorizontalBox::Slot().AutoWidth()[BodyButton(EDBBodyType::TypeB, LOCTEXT("BodyB", "Typ B"))]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(DBUIStyle::Font(14)).Text(LOCTEXT("Looks", "Aussehen"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 4.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 24.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[MakeChoiceRow(LOCTEXT("RowHair", "Frisur"), &HairStyles, &HairStyle)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[MakeChoiceRow(LOCTEXT("RowHairColor", "Haarfarbe"), &HairColors, &HairColor)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[MakeChoiceRow(LOCTEXT("RowEyes", "Augenfarbe"), &EyeColors, &EyeColor)]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[MakeChoiceRow(LOCTEXT("RowSkin", "Hautton"), &SkinTones, &SkinTone)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[MakeChoiceRow(LOCTEXT("RowScars", "Narben"), &Scars, &Scar)]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 20.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(11))
					.ColorAndOpacity(FLinearColor(0.55f, 0.52f, 0.5f))
					.Text(LOCTEXT("LooksHint", "Wird gespeichert und im Koop uebertragen. Gesicht und Haare werden sichtbar, sobald die Charakter-Art (MetaHuman) eingebunden ist."))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 10.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(14))
					.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.3f))
					.Text_Lambda([this]() { return Error; })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SButton)
					.ContentPadding(FMargin(40.f, 12.f))
					.ButtonColorAndOpacity(FLinearColor(0.55f, 0.1f, 0.08f))
					.OnClicked(this, &SDBCharacterCreatorWidget::Submit)
					[
						SNew(STextBlock).Font(DBUIStyle::Font(18, "Bold")).Text(LOCTEXT("Create", "Erschaffen"))
					]
				]
			]
		]
	];
}

TSharedPtr<SWidget> SDBCharacterCreatorWidget::GetFocusTarget() const
{
	return NameBox;
}

FReply SDBCharacterCreatorWidget::Submit()
{
	FDBAppearance Appearance;
	Appearance.BodyType = BodyType;
	Appearance.HairStyle = HairStyle;
	Appearance.HairColor = HairColorValues[HairColor];
	Appearance.EyeColor = EyeColorValues[EyeColor];
	Appearance.SkinTone = SkinTone;
	if (Scar > 0)
	{
		Appearance.Scars = {Scar};
	}
	const FString Name = NameBox.IsValid() ? NameBox->GetText().ToString() : FString();
	Error = OnCreate.IsBound() ? OnCreate.Execute(Name, SelectedClass, Appearance) : FText::GetEmpty();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
