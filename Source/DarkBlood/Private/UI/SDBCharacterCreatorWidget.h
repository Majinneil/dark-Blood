// Character creation screen: free name (rules-core validation), class, body type and appearance
// (hairstyle, hair / eye color, skin tone, scars). Only ids are stored and replicated (FDBAppearance);
// the visual profile maps them to meshes / material parameters once character art (MetaHuman) exists.
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
	TSharedRef<SWidget> MakeChoiceRow(const FText& Label, const TArray<FText>* Options, int32* Value);

	FName SelectedClass;
	EDBBodyType BodyType = EDBBodyType::TypeA;
	int32 HairStyle = 0;
	int32 HairColor = 0;
	int32 EyeColor = 0;
	int32 SkinTone = 1;
	int32 Scar = 0;
	FText Error;
};
