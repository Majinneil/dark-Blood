// Shared look of the code-built Slate UI (fonts, colors). Replace with a proper style set in the UI pass.
#pragma once

#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace DBUIStyle
{
	inline FSlateFontInfo Font(int32 Size, const char* Weight = "Regular")
	{
		return FCoreStyle::GetDefaultFontStyle(Weight, Size);
	}

	inline const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	inline const FLinearColor Gold(0.95f, 0.78f, 0.4f);
	inline const FLinearColor Blood(0.7f, 0.08f, 0.08f);
	inline const FLinearColor Panel(0.f, 0.f, 0.f, 0.55f);
	inline const FLinearColor PanelDark(0.02f, 0.01f, 0.01f, 0.85f);

	/** Flat bar: white fill tinted by FillColorAndOpacity, dark background. */
	inline const FProgressBarStyle* FlatBar()
	{
		static const FProgressBarStyle Style = FProgressBarStyle()
			.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.03f, 0.03f, 0.03f, 0.9f)))
			.SetFillImage(FSlateColorBrush(FLinearColor::White))
			.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
		return &Style;
	}
}
