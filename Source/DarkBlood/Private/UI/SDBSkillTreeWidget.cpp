#include "UI/SDBSkillTreeWidget.h"

#include "Core/DBRulesBridge.h"
#include "Data/DBClassDefinition.h"
#include "Data/DBGameDataSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "UI/DBUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "DarkBloodRules/SkillTree.h"

#define LOCTEXT_NAMESPACE "DarkBloodSkillTree"

namespace R = DarkBlood::Rules;

void SDBSkillTreeWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	if (const UDBClassDefinition* Class = GetClassDefinition())
	{
		TArray<FDBSkillNode> Nodes = Class->SkillTree;
		Nodes.StableSort([](const FDBSkillNode& A, const FDBSkillNode& B) { return A.RequiredLevel < B.RequiredLevel; });
		for (const FDBSkillNode& Node : Nodes)
		{
			Rows->AddSlot().AutoHeight().Padding(0.f, 4.f)[MakeNodeRow(Node)];
		}
	}

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(900.f)
		.MaxDesiredHeight(760.f)
		[
			SNew(SBorder)
			.BorderImage(DBUIStyle::WhiteBrush())
			.BorderBackgroundColor(DBUIStyle::PanelDark)
			.Padding(24.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock)
						.Font(DBUIStyle::Font(24, "Bold"))
						.ColorAndOpacity(DBUIStyle::Gold)
						.Text_Lambda([this]()
						{
							const UDBClassDefinition* Class = GetClassDefinition();
							return FText::Format(LOCTEXT("Title", "Faehigkeiten - {0}"), Class ? Class->DisplayName : FText::GetEmpty());
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(DBUIStyle::Font(16, "Bold"))
						.Text_Lambda([this]()
						{
							const UDBProgressionComponent* Progression = GetProgression();
							return FText::Format(LOCTEXT("Points", "Stufe {0}   |   Punkte: {1}"), FText::AsNumber(Progression ? Progression->GetLevel() : 0),
								FText::AsNumber(Progression ? Progression->GetUnspentSkillPoints() : 0));
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 16.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(12))
					.ColorAndOpacity(FLinearColor(0.7f, 0.68f, 0.65f))
					.Text(LOCTEXT("Hint", "Knoten veraendern Mechaniken. Aktive Faehigkeiten liegen auf den Tasten 1 und 2.   [K] schliessen"))
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[Rows]
				]
			]
		]
	];
}

const UDBClassDefinition* SDBSkillTreeWidget::GetClassDefinition() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerState* PS = PC ? PC->GetPlayerState<ADBPlayerState>() : nullptr;
	const UDBGameDataSubsystem* Data = PC ? UDBGameDataSubsystem::Get(PC) : nullptr;
	return PS && Data ? Data->FindClass(PS->GetProfile().ClassId) : nullptr;
}

UDBProgressionComponent* SDBSkillTreeWidget::GetProgression() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerState* PS = PC ? PC->GetPlayerState<ADBPlayerState>() : nullptr;
	return PS ? PS->GetProgression() : nullptr;
}

FText SDBSkillTreeWidget::GetBlockReason(const FDBSkillNode& Node) const
{
	const UDBClassDefinition* Class = GetClassDefinition();
	const UDBProgressionComponent* Progression = GetProgression();
	if (!Class || !Progression)
	{
		return LOCTEXT("NoData", "-");
	}
	// Rebuild the rules view from the replicated values (display only; the server re-checks).
	R::FSkillTreeState Skills;
	for (const FDBSkillNode& Other : Class->SkillTree)
	{
		if (const int32 Rank = Progression->GetSkillRank(Other.NodeId); Rank > 0)
		{
			Skills.Ranks[DBBridge::ToStd(Other.NodeId)] = Rank;
		}
	}
	R::FProgressionState State;
	State.Level = Progression->GetLevel();
	State.UnspentSkillPoints = Progression->GetUnspentSkillPoints();

	switch (R::CanUnlockSkill(Class->BuildSkillTree(), Skills, State, DBBridge::ToStd(Node.NodeId)))
	{
	case R::ESkillUnlockResult::Ok: return FText::GetEmpty();
	case R::ESkillUnlockResult::MaxRankReached: return LOCTEXT("Max", "Gemeistert");
	case R::ESkillUnlockResult::NotEnoughPoints: return LOCTEXT("Points", "Keine Punkte");
	case R::ESkillUnlockResult::LevelTooLow: return FText::Format(LOCTEXT("Level", "Ab Stufe {0}"), FText::AsNumber(Node.RequiredLevel));
	case R::ESkillUnlockResult::MissingPrerequisite: return LOCTEXT("Prereq", "Voraussetzung fehlt");
	case R::ESkillUnlockResult::UnknownNode: return LOCTEXT("Unknown", "Unbekannt");
	}
	return FText::GetEmpty();
}

TSharedRef<SWidget> SDBSkillTreeWidget::MakeNodeRow(const FDBSkillNode& Node)
{
	const FName NodeId = Node.NodeId;
	const int32 MaxRank = Node.MaxRank;
	const FDBSkillNode NodeCopy = Node;

	return SNew(SBorder)
		.BorderImage(DBUIStyle::WhiteBrush())
		.BorderBackgroundColor_Lambda([this, NodeId]()
		{
			const UDBProgressionComponent* Progression = GetProgression();
			return Progression && Progression->GetSkillRank(NodeId) > 0 ? FLinearColor(0.2f, 0.07f, 0.05f, 0.9f) : FLinearColor(0.08f, 0.08f, 0.08f, 0.9f);
		})
		.Padding(12.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(16, "Bold"))
					.Text_Lambda([this, NodeCopy, MaxRank]()
					{
						const UDBProgressionComponent* Progression = GetProgression();
						const int32 Rank = Progression ? Progression->GetSkillRank(NodeCopy.NodeId) : 0;
						return FText::Format(LOCTEXT("NodeTitle", "{0}   {1}/{2}"), NodeCopy.DisplayName, FText::AsNumber(Rank), FText::AsNumber(MaxRank));
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 16.f, 0.f)
				[
					SNew(STextBlock).Font(DBUIStyle::Font(13)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.85f, 0.82f, 0.78f)).Text(Node.Description)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(170.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(10.f, 8.f))
					.ButtonColorAndOpacity(FLinearColor(0.55f, 0.1f, 0.08f))
					.IsEnabled_Lambda([this, NodeCopy]() { return GetBlockReason(NodeCopy).IsEmpty(); })
					.OnClicked_Lambda([this, NodeId]()
					{
						if (UDBProgressionComponent* Progression = GetProgression())
						{
							Progression->RequestUnlockSkill(NodeId);
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Font(DBUIStyle::Font(14, "Bold"))
						.Text_Lambda([this, NodeCopy]()
						{
							const FText Reason = GetBlockReason(NodeCopy);
							return Reason.IsEmpty() ? FText::Format(LOCTEXT("Learn", "Lernen ({0})"), FText::AsNumber(NodeCopy.CostPerRank)) : Reason;
						})
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
