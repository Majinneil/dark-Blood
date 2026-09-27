// Conversation panel (bottom of the screen): speaker, line and numbered options. Keys 1-4 or clicks pick an
// option; [E] continues when the line has no options.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class UDBDialogueComponent;

class SDBDialogueWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBDialogueWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UDBDialogueComponent>, Dialogue)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Rebuilds the options from the dialogue component's current view. */
	void Refresh();

private:
	TWeakObjectPtr<UDBDialogueComponent> Dialogue;
	TSharedPtr<SVerticalBox> ChoiceBox;
};
