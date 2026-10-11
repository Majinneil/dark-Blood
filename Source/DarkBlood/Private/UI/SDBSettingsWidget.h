// Graphics settings panel ([F10]): quality preset, resolution, window mode, TSR upscaling, ray tracing, frame rate
// limit and VSync. Edits a pending copy; "Uebernehmen" applies and saves (UDBGameUserSettings).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

class SDBSettingsWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDBSettingsWidget) {}
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FPending
	{
		int32 Quality = 3;
		int32 ResolutionIndex = 0;
		int32 WindowMode = 1;
		int32 Upscaling = 1;
		bool bRayTracing = true;
		int32 FrameRateIndex = 0;
		bool bVSync = true;
		/** Volumes in percent steps of 10. */
		int32 Master = 10;
		int32 Music = 8;
		int32 Effects = 10;
		int32 Voice = 10;
	};

	void LoadFromSettings();
	void Apply();
	void RestoreRecommended();

	/** One row: label, current value, "<" and ">" to cycle. */
	TSharedRef<SWidget> MakeRow(const FText& Label, TFunction<FText()> Value, TFunction<void(int32)> Step, TFunction<bool()> IsEnabled = nullptr);

	FPending Pending;
	TArray<FIntPoint> Resolutions;
	FSimpleDelegate OnClose;
	FText StatusText;
};
