// In-game HUD built with Slate in code (no widget assets needed): vitals, quest tracker, lock-on target,
// interaction prompt and notifications. A UMG/CommonUI skin can replace it later without gameplay changes.
#pragma once

#include "CoreMinimal.h"
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

	TWeakObjectPtr<APlayerController> Owner;
	FText Notification;
	double NotificationUntil = 0.0;
};
