#include "UI/SDBSettingsWidget.h"

#include "Audio/DBAudioSubsystem.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Settings/DBGameUserSettings.h"
#include "UI/DBUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DarkBloodSettings"

namespace
{
	const float FrameRates[] = {0.f, 30.f, 60.f, 120.f, 144.f};

	FText QualityName(int32 Level)
	{
		static const FText Names[] = {LOCTEXT("Low", "Niedrig"), LOCTEXT("Medium", "Mittel"), LOCTEXT("High", "Hoch"), LOCTEXT("Epic", "Episch"),
			LOCTEXT("Cinematic", "Kino")};
		return Names[FMath::Clamp(Level, 0, 4)];
	}

	FText WindowModeName(int32 Mode)
	{
		static const FText Names[] = {LOCTEXT("Fullscreen", "Vollbild"), LOCTEXT("Borderless", "Fenster-Vollbild"), LOCTEXT("Windowed", "Fenster")};
		return Names[FMath::Clamp(Mode, 0, 2)];
	}

	FText UpscalingName(int32 Mode)
	{
		return StaticEnum<EDBUpscaling>()->GetDisplayNameTextByIndex(FMath::Clamp(Mode, 0, 4));
	}

	FText Percent(int32 Tenths)
	{
		return FText::Format(LOCTEXT("Percent", "{0} %"), FText::AsNumber(Tenths * 10));
	}

	int32 Wrap(int32 Value, int32 Count)
	{
		return Count > 0 ? (Value % Count + Count) % Count : 0;
	}
}

void SDBSettingsWidget::Construct(const FArguments& InArgs)
{
	OnClose = InArgs._OnClose;

	// Resolutions the display supports, plus the common 16:9 steps up to 4K for windowed play.
	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	for (const FIntPoint& Common : {FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160)})
	{
		Supported.AddUnique(Common);
	}
	for (const FIntPoint& Resolution : Supported)
	{
		if (Resolution.X >= 1280 && Resolution.Y >= 720)
		{
			Resolutions.AddUnique(Resolution);
		}
	}
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
	LoadFromSettings();

	const bool bRayTracingAvailable = UDBGameUserSettings::IsHardwareRayTracingAvailable();
	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(760.f)
		[
			SNew(SBorder)
			.BorderImage(DBUIStyle::WhiteBrush())
			.BorderBackgroundColor(DBUIStyle::PanelDark)
			.Padding(24.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
				[
					SNew(STextBlock).Font(DBUIStyle::Font(24, "Bold")).ColorAndOpacity(DBUIStyle::Gold).Text(LOCTEXT("Title", "Grafik und Klang"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Quality", "Grafikqualitaet"), [this]() { return QualityName(Pending.Quality); },
						[this](int32 Delta) { Pending.Quality = Wrap(Pending.Quality + Delta, 5); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Resolution", "Aufloesung"),
						[this]()
						{
							const FIntPoint R = Resolutions.IsValidIndex(Pending.ResolutionIndex) ? Resolutions[Pending.ResolutionIndex] : FIntPoint::ZeroValue;
							return FText::FromString(FString::Printf(TEXT("%d x %d%s"), R.X, R.Y, R.Y >= 2160 ? TEXT("  (4K)") : TEXT("")));
						},
						[this](int32 Delta) { Pending.ResolutionIndex = Wrap(Pending.ResolutionIndex + Delta, Resolutions.Num()); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("WindowMode", "Anzeigemodus"), [this]() { return WindowModeName(Pending.WindowMode); },
						[this](int32 Delta) { Pending.WindowMode = Wrap(Pending.WindowMode + Delta, 3); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Upscaling", "Hochskalierung (TSR)"), [this]() { return UpscalingName(Pending.Upscaling); },
						[this](int32 Delta) { Pending.Upscaling = Wrap(Pending.Upscaling + Delta, 5); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("RayTracing", "Raytracing (Licht und Spiegelungen)"),
						[this, bRayTracingAvailable]()
						{
							return !bRayTracingAvailable ? LOCTEXT("NotAvailable", "nicht verfuegbar")
														 : (Pending.bRayTracing ? LOCTEXT("On", "An") : LOCTEXT("Off", "Aus"));
						},
						[this](int32) { Pending.bRayTracing = !Pending.bRayTracing; }, [bRayTracingAvailable]() { return bRayTracingAvailable; })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("FrameRate", "Bildrate begrenzen"),
						[this]()
						{
							const float Limit = FrameRates[FMath::Clamp(Pending.FrameRateIndex, 0, 4)];
							return Limit > 0.f ? FText::FromString(FString::Printf(TEXT("%.0f FPS"), Limit)) : LOCTEXT("Unlimited", "Unbegrenzt");
						},
						[this](int32 Delta) { Pending.FrameRateIndex = Wrap(Pending.FrameRateIndex + Delta, UE_ARRAY_COUNT(FrameRates)); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("VSync", "VSync"), [this]() { return Pending.bVSync ? LOCTEXT("On", "An") : LOCTEXT("Off", "Aus"); },
						[this](int32) { Pending.bVSync = !Pending.bVSync; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					MakeRow(LOCTEXT("Master", "Gesamtlautstaerke"), [this]() { return Percent(Pending.Master); },
						[this](int32 Step) { Pending.Master = FMath::Clamp(Pending.Master + Step, 0, 10); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Music", "Musik"), [this]() { return Percent(Pending.Music); },
						[this](int32 Step) { Pending.Music = FMath::Clamp(Pending.Music + Step, 0, 10); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Effects", "Effekte und Umgebung"), [this]() { return Percent(Pending.Effects); },
						[this](int32 Step) { Pending.Effects = FMath::Clamp(Pending.Effects + Step, 0, 10); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Voice", "Stimmen"), [this]() { return Percent(Pending.Voice); },
						[this](int32 Step) { Pending.Voice = FMath::Clamp(Pending.Voice + Step, 0, 10); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(12))
					.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
					.AutoWrapText(true)
					.Text(LOCTEXT("Hint", "4K fluessig: Aufloesung 3840 x 2160 mit Hochskalierung \"Qualitaet\" oder \"Leistung\" - "
										 "das Spiel rechnet intern kleiner und TSR schaerft auf 4K hoch."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(STextBlock).Font(DBUIStyle::Font(12)).ColorAndOpacity(DBUIStyle::Gold).Text_Lambda([this]() { return StatusText; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f).HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Recommended", "Empfohlen"))
						.OnClicked_Lambda([this]() { RestoreRecommended(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Apply", "Uebernehmen"))
						.OnClicked_Lambda([this]() { Apply(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Close", "Schliessen"))
						.OnClicked_Lambda([this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); })
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SDBSettingsWidget::MakeRow(const FText& Label, TFunction<FText()> Value, TFunction<void(int32)> Step, TFunction<bool()> IsEnabled)
{
	auto Enabled = [IsEnabled]() { return !IsEnabled || IsEnabled(); };
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 4.f)
		[
			SNew(STextBlock).Font(DBUIStyle::Font(15)).Text(Label)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton).Text(FText::FromString(TEXT("<"))).IsEnabled_Lambda(Enabled).OnClicked_Lambda([Step]() { Step(-1); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(250.f)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(DBUIStyle::Font(15, "Bold")).Text_Lambda([Value]() { return Value(); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton).Text(FText::FromString(TEXT(">"))).IsEnabled_Lambda(Enabled).OnClicked_Lambda([Step]() { Step(1); return FReply::Handled(); })
		];
}

void SDBSettingsWidget::LoadFromSettings()
{
	const UDBGameUserSettings* Settings = UDBGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	Pending.Quality = FMath::Clamp(Settings->GetOverallScalabilityLevel(), 0, 4);
	const FIntPoint Current = Settings->GetScreenResolution();
	Resolutions.AddUnique(Current);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
	Pending.ResolutionIndex = Resolutions.IndexOfByKey(Current);
	Pending.WindowMode = static_cast<int32>(Settings->GetFullscreenMode());
	Pending.Upscaling = static_cast<int32>(Settings->Upscaling);
	Pending.bRayTracing = Settings->bHardwareRayTracing;
	Pending.FrameRateIndex = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameRates); ++Index)
	{
		if (FMath::IsNearlyEqual(FrameRates[Index], Settings->GetFrameRateLimit()))
		{
			Pending.FrameRateIndex = Index;
		}
	}
	Pending.bVSync = Settings->IsVSyncEnabled();
	Pending.Master = FMath::RoundToInt(Settings->MasterVolume * 10.f);
	Pending.Music = FMath::RoundToInt(Settings->MusicVolume * 10.f);
	Pending.Effects = FMath::RoundToInt(Settings->EffectsVolume * 10.f);
	Pending.Voice = FMath::RoundToInt(Settings->VoiceVolume * 10.f);
}

void SDBSettingsWidget::Apply()
{
	UDBGameUserSettings* Settings = UDBGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	Settings->SetOverallScalabilityLevel(Pending.Quality);
	if (Resolutions.IsValidIndex(Pending.ResolutionIndex))
	{
		Settings->SetScreenResolution(Resolutions[Pending.ResolutionIndex]);
	}
	Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(Pending.WindowMode));
	Settings->Upscaling = static_cast<EDBUpscaling>(Pending.Upscaling);
	Settings->bHardwareRayTracing = Pending.bRayTracing;
	Settings->SetFrameRateLimit(FrameRates[FMath::Clamp(Pending.FrameRateIndex, 0, 4)]);
	Settings->SetVSyncEnabled(Pending.bVSync);
	Settings->MasterVolume = Pending.Master / 10.f;
	Settings->MusicVolume = Pending.Music / 10.f;
	Settings->EffectsVolume = Pending.Effects / 10.f;
	Settings->VoiceVolume = Pending.Voice / 10.f;
	Settings->ApplySettings(false);
	Settings->SaveSettings();
	// Volumes take effect at once in every running world.
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UDBAudioSubsystem* Audio = Context.World() ? Context.World()->GetSubsystem<UDBAudioSubsystem>() : nullptr)
		{
			Audio->ApplyVolumes();
		}
	}
	StatusText = FText::FromString(TEXT("Gespeichert: ") + Settings->Describe());
}

void SDBSettingsWidget::RestoreRecommended()
{
	if (UDBGameUserSettings* Settings = UDBGameUserSettings::Get())
	{
		Settings->ApplyRecommendedDefaults();
		LoadFromSettings();
		StatusText = LOCTEXT("RecommendedLoaded", "Empfohlene Werte geladen - \"Uebernehmen\" zum Speichern.");
	}
}

#undef LOCTEXT_NAMESPACE
