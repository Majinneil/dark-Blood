// In-game HUD built with Slate in code (no widget assets needed): vitals, quest tracker, lock-on target,
// interaction prompt and notifications. A UMG/CommonUI skin can replace it later without gameplay changes.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class UAbilitySystemComponent;

class SDBGameHudWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBGameHudWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void ShowNotification(const FText& Text, float Seconds = 4.f);

private:
	UAbilitySystemComponent* GetPlayerAbilitySystem() const;
	UAbilitySystemComponent* GetLockTargetAbilitySystem() const;

	TSharedRef<SWidget> MakeBar(const FText& Label, const FLinearColor& Color, TFunction<float()> Current, TFunction<float()> Max) const;

	FText GetQuestTrackerText() const;
	FText GetAbilityBarText() const;
	FText GetInteractionPrompt() const;
	FText GetLockTargetName() const;
	EVisibility GetLockTargetVisibility() const;
	EVisibility GetNotificationVisibility() const;
	/** Nearest living boss within 60 m of the local player (the boss bar). */
	const class ADBBossCharacter* FindBoss() const;
	FText GetBossTitle() const;
	/** Portrait of that boss (loaded once per boss), null without one. */
	const FSlateBrush* GetBossPortrait() const;

	TWeakObjectPtr<APlayerController> Owner;
	FText Notification;
	double NotificationUntil = 0.0;
	mutable FSlateBrush BossPortraitBrush;
	mutable FName BossPortraitId;
	mutable TStrongObjectPtr<class UTexture2D> BossPortraitTexture;
};
