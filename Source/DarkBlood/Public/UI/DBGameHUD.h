// Game HUD: owns the code-built Slate UI (HUD, dialogue, character creator) and switches the input mode.
// Inherits the canvas debug overlay (DBToggleDebugHUD).
#pragma once

#include "Debug/DBDebugHUD.h"

#include "DBGameHUD.generated.h"

class SDBCharacterCreatorWidget;
class SDBDialogueWidget;
class SDBCraftingWidget;
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

	void ToggleSkillTree();
	bool IsSkillTreeVisible() const { return SkillTreeRoot.IsValid(); }

	void ToggleInventory();
	void ShowCrafting(AActor* Station);
	void HideCrafting();

	virtual void DrawHUD() override;

private:
	void OnDialogueChanged();
	void UpdateInputMode();

	TSharedPtr<SDBGameHudWidget> HudWidget;
	TSharedPtr<SDBDialogueWidget> DialogueWidget;
	TSharedPtr<SDBCharacterCreatorWidget> CreatorWidget;
	TSharedPtr<SWidget> CreatorRoot;
	TSharedPtr<SWidget> SkillTreeRoot;
	TSharedPtr<SWidget> InventoryRoot;
	TSharedPtr<SDBCraftingWidget> CraftingWidget;
	FDelegateHandle DialogueHandle;
	bool bUIReady = false;
};
