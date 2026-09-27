// Character creation screen: free name (rules-core validation), class, body type. Appearance details
// (face, hair, colors) arrive with character art; the data model (FDBAppearance) already holds them.
#pragma once

#include "CoreMinimal.h"
#include "Core/DBTypes.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;

DECLARE_DELEGATE_RetVal_ThreeParams(FText, FDBOnCreateCharacter, const FString& /*Name*/, FName /*ClassId*/, const FDBAppearance& /*Appearance*/);

struct FDBCreatorClassOption
{
	FName ClassId;
	FText DisplayName;
	FText Description;
};

class SDBCharacterCreatorWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBCharacterCreatorWidget) {}
		SLATE_ARGUMENT(TArray<FDBCreatorClassOption>, Classes)
		/** Returns an error text (empty on success). */
		SLATE_EVENT(FDBOnCreateCharacter, OnCreate)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedPtr<SWidget> GetFocusTarget() const;

private:
	FReply Submit();

	TArray<FDBCreatorClassOption> Classes;
	FDBOnCreateCharacter OnCreate;
	TSharedPtr<SEditableTextBox> NameBox;
	FName SelectedClass;
	EDBBodyType BodyType = EDBBodyType::TypeA;
	FText Error;
};
