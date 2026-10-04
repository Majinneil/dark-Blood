#include "UI/SDBGameHudWidget.h"

#include "UI/DBUIStyle.h"

#include "AbilitySystemComponent.h"
#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBGameplayAbility.h"
#include "Core/DBGameplayTags.h"
#include "Abilities/DBAttributeSet.h"
#include "Character/DBPlayerCharacter.h"
#include "Combat/DBCombatStatics.h"
#include "Combat/DBLockOnComponent.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBQuestDefinition.h"
#include "Dialogue/DBDialogueComponent.h"
#include "Framework/DBGameState.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/DBInteractable.h"
#include "Interaction/DBInteractionComponent.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Quest/DBQuestComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DarkBloodHud"

namespace DBHudStyle = DBUIStyle;

void SDBGameHudWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");

	auto Attribute = [this](const FGameplayAttribute& Attr)
	{
		return [this, Attr]()
		{
			const UAbilitySystemComponent* ASC = GetPlayerAbilitySystem();
			return ASC ? ASC->GetNumericAttribute(Attr) : 0.f;
		};
	};

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)

		// Vitals (bottom left)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(32.f, 0.f, 0.f, 32.f)
		[
			SNew(SBorder).BorderImage(White).BorderBackgroundColor(DBHudStyle::Panel).Padding(12.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(STextBlock)
					.Font(DBHudStyle::Font(15, "Bold"))
					.ColorAndOpacity(DBHudStyle::Gold)
					.Text_Lambda([this]()
					{
						const APlayerController* PC = Owner.Get();
						const ADBPlayerState* PS = PC ? PC->GetPlayerState<ADBPlayerState>() : nullptr;
						return PS ? FText::FromString(PS->GetPlayerName()) : FText::GetEmpty();
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					MakeBar(LOCTEXT("Health", "Leben"), FLinearColor(0.7f, 0.08f, 0.08f), Attribute(UDBAttributeSet::GetHealthAttribute()),
						Attribute(UDBAttributeSet::GetMaxHealthAttribute()))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					MakeBar(LOCTEXT("Stamina", "Ausdauer"), FLinearColor(0.2f, 0.6f, 0.2f), Attribute(UDBAttributeSet::GetStaminaAttribute()),
						Attribute(UDBAttributeSet::GetMaxStaminaAttribute()))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					MakeBar(LOCTEXT("Mana", "Mana"), FLinearColor(0.15f, 0.3f, 0.8f), Attribute(UDBAttributeSet::GetManaAttribute()),
						Attribute(UDBAttributeSet::GetMaxManaAttribute()))
				]
			]
		]

		// Quest tracker (top right)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 32.f, 32.f, 0.f)
		[
			SNew(SBox).WidthOverride(400.f)
			[
				SNew(SBorder).BorderImage(White).BorderBackgroundColor(DBHudStyle::Panel).Padding(12.f)
				.Visibility_Lambda([this]() { return GetQuestTrackerText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				[
					SNew(STextBlock)
					.Font(DBHudStyle::Font(13))
					.AutoWrapText(true)
					.Text_Lambda([this]() { return GetQuestTrackerText(); })
				]
			]
		]

		// Lock-on target (top center)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 36.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([this]() { return GetLockTargetVisibility(); })
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(DBHudStyle::Font(16, "Bold"))
				.ColorAndOpacity(FLinearColor(0.95f, 0.45f, 0.4f))
				.Text_Lambda([this]() { return GetLockTargetName(); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(420.f).HeightOverride(12.f)
				[
					SNew(SProgressBar)
				.Style(DBUIStyle::FlatBar())
					.FillColorAndOpacity(FLinearColor(0.7f, 0.08f, 0.08f))
					.Percent_Lambda([this]() -> TOptional<float>
					{
						const UAbilitySystemComponent* ASC = GetLockTargetAbilitySystem();
						const float Max = ASC ? ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()) : 0.f;
						return Max > 0.f ? ASC->GetNumericAttribute(UDBAttributeSet::GetHealthAttribute()) / Max : 0.f;
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(420.f).HeightOverride(5.f)
				[
					SNew(SProgressBar)
				.Style(DBUIStyle::FlatBar())
					.FillColorAndOpacity(DBHudStyle::Gold)
					.Percent_Lambda([this]() -> TOptional<float>
					{
						const UAbilitySystemComponent* ASC = GetLockTargetAbilitySystem();
						const float Max = ASC ? ASC->GetNumericAttribute(UDBAttributeSet::GetMaxPoiseAttribute()) : 0.f;
						return Max > 0.f ? ASC->GetNumericAttribute(UDBAttributeSet::GetPoiseAttribute()) / Max : 0.f;
					})
				]
			]
		]

		// Ability bar (bottom center): class abilities on 1-4 with cooldowns
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 32.f)
		[
			SNew(SBorder).BorderImage(White).BorderBackgroundColor(DBHudStyle::Panel).Padding(FMargin(14.f, 8.f))
			.Visibility_Lambda([this]() { return GetAbilityBarText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(STextBlock)
				.Font(DBHudStyle::Font(14))
				.Text_Lambda([this]() { return GetAbilityBarText(); })
			]
		]

		// Interaction prompt (lower center)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 220.f)
		[
			SNew(SBorder).BorderImage(White).BorderBackgroundColor(DBHudStyle::Panel).Padding(FMargin(16.f, 8.f))
			.Visibility_Lambda([this]() { return GetInteractionPrompt().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(STextBlock)
				.Font(DBHudStyle::Font(16))
				.ColorAndOpacity(FLinearColor::White)
				.Text_Lambda([this]() { return GetInteractionPrompt(); })
			]
		]

		// Notifications (upper center)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 140.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return GetNotificationVisibility(); })
			.Font(DBHudStyle::Font(24, "Bold"))
			.ColorAndOpacity(DBHudStyle::Gold)
			.ShadowOffset(FVector2D(2.f, 2.f))
			.Text_Lambda([this]() { return Notification; })
		]
	];
}

TSharedRef<SWidget> SDBGameHudWidget::MakeBar(const FText& Label, const FLinearColor& Color, TFunction<float()> Current, TFunction<float()> Max) const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(80.f)
			[
				SNew(STextBlock).Font(DBHudStyle::Font(12)).Text(Label)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(260.f).HeightOverride(14.f)
			[
				SNew(SProgressBar)
				.Style(DBUIStyle::FlatBar())
				.FillColorAndOpacity(Color)
				.Percent_Lambda([Current, Max]() -> TOptional<float>
				{
					const float MaxValue = Max();
					return MaxValue > 0.f ? FMath::Clamp(Current() / MaxValue, 0.f, 1.f) : 0.f;
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Font(DBHudStyle::Font(12))
			.Text_Lambda([Current, Max]() { return FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Current(), Max())); })
		];
}

UAbilitySystemComponent* SDBGameHudWidget::GetPlayerAbilitySystem() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerState* PS = PC ? PC->GetPlayerState<ADBPlayerState>() : nullptr;
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

UAbilitySystemComponent* SDBGameHudWidget::GetLockTargetAbilitySystem() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerCharacter* Character = PC ? Cast<ADBPlayerCharacter>(PC->GetPawn()) : nullptr;
	const AActor* Target = Character ? Character->GetLockOn()->GetLockTarget() : nullptr;
	const IAbilitySystemInterface* WithASC = Cast<IAbilitySystemInterface>(Target);
	return WithASC ? WithASC->GetAbilitySystemComponent() : nullptr;
}

EVisibility SDBGameHudWidget::GetLockTargetVisibility() const
{
	return GetLockTargetAbilitySystem() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText SDBGameHudWidget::GetLockTargetName() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerCharacter* Character = PC ? Cast<ADBPlayerCharacter>(PC->GetPawn()) : nullptr;
	const AActor* Target = Character ? Character->GetLockOn()->GetLockTarget() : nullptr;
	return Target ? FText::FromString(DBCombat::GetCombatName(Target)) : FText::GetEmpty();
}

FText SDBGameHudWidget::GetInteractionPrompt() const
{
	const APlayerController* PC = Owner.Get();
	const ADBPlayerController* DBController = Cast<ADBPlayerController>(PC);
	if (!PC || (DBController && DBController->GetDialogue()->IsDialogueOpen()))
	{
		return FText::GetEmpty();
	}
	const APawn* Pawn = PC->GetPawn();
	const UDBInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UDBInteractionComponent>() : nullptr;
	const IDBInteractable* Interactable = Interaction ? Cast<IDBInteractable>(Interaction->GetFocusedInteractable()) : nullptr;
	return Interactable ? FText::Format(LOCTEXT("Prompt", "[E]  {0}"), Interactable->GetInteractionText()) : FText::GetEmpty();
}

FText SDBGameHudWidget::GetAbilityBarText() const
{
	const UDBAbilitySystemComponent* ASC = Cast<UDBAbilitySystemComponent>(GetPlayerAbilitySystem());
	if (!ASC)
	{
		return FText::GetEmpty();
	}
	const FGameplayTag Slots[] = {DBTags::Input_Ability1, DBTags::Input_Ability2, DBTags::Input_Ability3, DBTags::Input_Ability4};
	FString Out;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UDBGameplayAbility* Ability = Cast<UDBGameplayAbility>(Spec.Ability);
			if (!Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(Slots[Index]))
			{
				continue;
			}
			const float Cooldown = Ability->GetCooldownTag().IsValid() ? ASC->GetTimedTagRemaining(Ability->GetCooldownTag()) : 0.f;
			const FString State = Spec.IsActive() ? TEXT(" (aktiv)") : Cooldown > 0.f ? FString::Printf(TEXT(" (%.1f s)"), Cooldown) : FString();
			Out += FString::Printf(TEXT("%s[%d] %s%s"), Out.IsEmpty() ? TEXT("") : TEXT("      "), Index + 1, *Ability->GetDisplayName().ToString(), *State);
		}
	}
	return FText::FromString(Out);
}

FText SDBGameHudWidget::GetQuestTrackerText() const
{
	const APlayerController* PC = Owner.Get();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	const UDBGameDataSubsystem* Data = PC ? UDBGameDataSubsystem::Get(PC) : nullptr;
	if (!World || !Data)
	{
		return FText::GetEmpty();
	}
	TArray<FDBQuestProgressView> Views;
	if (const ADBGameState* GameState = World->GetGameState<ADBGameState>(); GameState && GameState->GetSharedQuests())
	{
		Views.Append(GameState->GetSharedQuests()->GetQuests());
	}
	if (const ADBPlayerState* PS = PC->GetPlayerState<ADBPlayerState>(); PS && PS->GetPersonalQuests())
	{
		Views.Append(PS->GetPersonalQuests()->GetQuests());
	}

	FString Out;
	for (const FDBQuestProgressView& Quest : Views)
	{
		const UDBQuestDefinition* Definition = Data->FindQuest(Quest.QuestId);
		if (!Definition || (Quest.Status != EDBQuestStatus::Active && Quest.Status != EDBQuestStatus::ReadyToTurnIn))
		{
			continue;
		}
		Out += FString::Printf(TEXT("%s\n"), *Definition->Title.ToString());
		if (Quest.Status == EDBQuestStatus::ReadyToTurnIn)
		{
			Out += FString::Printf(TEXT("   >  %s\n"), *LOCTEXT("TurnIn", "Abgeben").ToString());
			continue;
		}
		for (int32 Index = 0; Index < Definition->Objectives.Num(); ++Index)
		{
			const FDBQuestObjective& Objective = Definition->Objectives[Index];
			const int32 Count = Quest.ObjectiveCounts.IsValidIndex(Index) ? Quest.ObjectiveCounts[Index] : 0;
			const bool bDone = Count >= Objective.Required;
			const FString Label = Objective.Description.IsEmpty() ? Objective.ObjectiveId.ToString() : Objective.Description.ToString();
			Out += Objective.Required > 1 ? FString::Printf(TEXT("   %s  %s  %d/%d\n"), bDone ? TEXT("[x]") : TEXT("[ ]"), *Label, Count, Objective.Required)
										  : FString::Printf(TEXT("   %s  %s\n"), bDone ? TEXT("[x]") : TEXT("[ ]"), *Label);
			if (!bDone && Definition->bSequential)
			{
				break; // only the current step of sequential quests
			}
		}
	}
	return FText::FromString(Out.TrimEnd());
}

void SDBGameHudWidget::ShowNotification(const FText& Text, float Seconds)
{
	Notification = Text;
	NotificationUntil = FPlatformTime::Seconds() + Seconds;
}

EVisibility SDBGameHudWidget::GetNotificationVisibility() const
{
	return FPlatformTime::Seconds() < NotificationUntil ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
