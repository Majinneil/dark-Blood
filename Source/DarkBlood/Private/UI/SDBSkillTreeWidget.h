// Skill tree panel ([K]): nodes of the player's class with rank, description, requirements and a learn
// button. Availability is checked with the rules core on the client for display; the server decides.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class UDBClassDefinition;
class UDBProgressionComponent;
struct FDBSkillNode;

class SDBSkillTreeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBSkillTreeWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	const UDBClassDefinition* GetClassDefinition() const;
	UDBProgressionComponent* GetProgression() const;

	/** Empty when the node can be learned; otherwise a short reason. */
	FText GetBlockReason(const FDBSkillNode& Node) const;

	TSharedRef<SWidget> MakeNodeRow(const FDBSkillNode& Node);

	TWeakObjectPtr<APlayerController> Owner;
};
