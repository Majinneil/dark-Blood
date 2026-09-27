// DEVELOPMENT overlay drawn with the canvas (no UMG assets needed). Toggle with the console command DBToggleDebugHUD.
// The real game UI (UMG/CommonUI) replaces this in Phase 3; the overlay stays available for testing.
#pragma once

#include "GameFramework/HUD.h"

#include "DBDebugHUD.generated.h"

UCLASS()
class DARKBLOOD_API ADBDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	void ToggleDebugOverlay() { bShowOverlay = !bShowOverlay; }

private:
	void DrawLine(const FString& Text, float& Y, const FLinearColor& Color = FLinearColor::White);

	bool bShowOverlay = true;
};
