// Game HUD: owns the code-built Slate UI (HUD, dialogue, character creator) and switches the input mode.
// Inherits the canvas debug overlay (DBToggleDebugHUD).
#pragma once

#include "Debug/DBDebugHUD.h"

#include "DBGameHUD.generated.h"

class SDBCharacterCreatorWidget;
class SDBDialogueWidget;
class SDBCarriageWidget;
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

	/** World map [M]. */
	void ToggleMap();

	/** Graphics settings [F10]. */
	void ToggleSettings();
	void ShowCrafting(AActor* Station);
	void HideCrafting();
	void ShowCarriage(AActor* Station);
	void HideCarriage();

	/** The end of the story (Phase 15): finale text and credits over the world for half a minute. */
	void ShowFinale();

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
	TSharedPtr<SWidget> SettingsRoot;
	TSharedPtr<SWidget> MapRoot;
	TSharedPtr<SWidget> FinaleRoot;
	FTimerHandle FinaleTimer;
	TSharedPtr<SDBCraftingWidget> CraftingWidget;
	TSharedPtr<SDBCarriageWidget> CarriageWidget;
	FDelegateHandle DialogueHandle;
	bool bUIReady = false;
	/** Open menu panels last time (UI open / close sounds). */
	int32 OpenPanels = 0;
};
