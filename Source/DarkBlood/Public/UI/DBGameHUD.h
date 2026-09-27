// Game HUD: owns the code-built Slate UI (HUD, dialogue, character creator) and switches the input mode.
// Inherits the canvas debug overlay (DBToggleDebugHUD).
#pragma once

#include "Debug/DBDebugHUD.h"

#include "DBGameHUD.generated.h"

class SDBCharacterCreatorWidget;
class SDBDialogueWidget;
class SDBGameHudWidget;
class SWidget;

UCLASS()
class DARKBLOOD_API ADBGameHUD : public ADBDebugHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ShowNotification(const FText& Text);
	void ShowCharacterCreator();
	void HideCharacterCreator();
	bool IsCharacterCreatorVisible() const { return CreatorRoot.IsValid(); }

private:
	void OnDialogueChanged();
	void UpdateInputMode();

	TSharedPtr<SDBGameHudWidget> HudWidget;
	TSharedPtr<SDBDialogueWidget> DialogueWidget;
	TSharedPtr<SDBCharacterCreatorWidget> CreatorWidget;
	TSharedPtr<SWidget> CreatorRoot;
	FDelegateHandle DialogueHandle;
	bool bUIReady = false;
};
