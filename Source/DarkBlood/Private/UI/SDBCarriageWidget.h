// Carriage window: every other settlement with fare and travel time; a click asks the server to travel there.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ADBCarriageStation;
class APlayerController;

class SDBCarriageWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBCarriageWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
		SLATE_ARGUMENT(TWeakObjectPtr<ADBCarriageStation>, Station)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	AActor* GetStation() const;

private:
	TWeakObjectPtr<APlayerController> Owner;
	TWeakObjectPtr<ADBCarriageStation> Station;
	FSimpleDelegate OnClose;
};
