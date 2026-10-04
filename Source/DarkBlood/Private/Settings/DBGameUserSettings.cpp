#include "Settings/DBGameUserSettings.h"

#include "DarkBlood.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "RenderUtils.h"

UDBGameUserSettings* UDBGameUserSettings::Get()
{
	return GEngine ? Cast<UDBGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UDBGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	Upscaling = EDBUpscaling::Quality;
	bHardwareRayTracing = true;
}

float UDBGameUserSettings::GetScreenPercentage(EDBUpscaling Mode)
{
	switch (Mode)
	{
	case EDBUpscaling::Native: return 100.f;
	case EDBUpscaling::Quality: return 66.7f;
	case EDBUpscaling::Balanced: return 58.f;
	case EDBUpscaling::Performance: return 50.f;
	case EDBUpscaling::UltraPerformance: return 33.3f;
	default: return 100.f;
	}
}

bool UDBGameUserSettings::IsHardwareRayTracingAvailable()
{
	return GRHISupportsRayTracing && IsRayTracingEnabled();
}

void UDBGameUserSettings::ApplyNonResolutionSettings()
{
	// The upscaler input is part of the scalability state the engine applies below.
	ScalabilityQuality.ResolutionQuality = GetScreenPercentage(Upscaling);
	Super::ApplyNonResolutionSettings();

	// SetByCode: these outrank the project defaults in DefaultEngine.ini (a game setting would not).
	if (IConsoleVariable* AntiAliasing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
	{
		AntiAliasing->Set(4, ECVF_SetByCode); // TSR: temporal upscaling to the output resolution
	}
	if (IConsoleVariable* HardwareRayTracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing")))
	{
		HardwareRayTracing->Set(bHardwareRayTracing && IsHardwareRayTracingAvailable() ? 1 : 0, ECVF_SetByCode);
	}
}

void UDBGameUserSettings::ApplyRecommendedDefaults()
{
	// Output resolution: the desktop. The upscaler keeps the internal resolution near 1440p on 4K screens.
	const FIntPoint Desktop = GetDesktopResolution();
	SetScreenResolution(Desktop);
	SetFullscreenMode(EWindowMode::WindowedFullscreen);
	Upscaling = Desktop.Y >= 1400 ? EDBUpscaling::Quality : EDBUpscaling::Native;
	const bool bRayTracing = IsHardwareRayTracingAvailable();
	bHardwareRayTracing = bRayTracing;
	// Ray tracing capable GPUs handle the epic preset; everything else starts on high.
	SetOverallScalabilityLevel(bRayTracing ? 3 : 2);
	SetFrameRateLimit(0.f);
	SetVSyncEnabled(true);
	bRecommendedDefaultsApplied = true;
	UE_LOG(LogDarkBlood, Log, TEXT("Graphics: recommended defaults for %dx%d -> %s"), Desktop.X, Desktop.Y, *Describe());
}

FString UDBGameUserSettings::Describe() const
{
	const FIntPoint Resolution = GetScreenResolution();
	static const TCHAR* Presets[] = {TEXT("Niedrig"), TEXT("Mittel"), TEXT("Hoch"), TEXT("Episch"), TEXT("Kino")};
	const int32 Level = GetOverallScalabilityLevel();
	return FString::Printf(TEXT("%dx%d %s, Qualitaet %s, TSR %.0f %%, Raytracing %s, FPS-Limit %s, VSync %s"), Resolution.X, Resolution.Y,
		GetFullscreenMode() == EWindowMode::Fullscreen ? TEXT("Vollbild") : (GetFullscreenMode() == EWindowMode::WindowedFullscreen ? TEXT("Fenster-Vollbild") : TEXT("Fenster")),
		Level >= 0 && Level <= 4 ? Presets[Level] : TEXT("Benutzerdefiniert"), GetScreenPercentage(Upscaling),
		bHardwareRayTracing && IsHardwareRayTracingAvailable() ? TEXT("an") : (IsHardwareRayTracingAvailable() ? TEXT("aus") : TEXT("nicht verfuegbar")),
		GetFrameRateLimit() > 0.f ? *FString::Printf(TEXT("%.0f"), GetFrameRateLimit()) : TEXT("aus"), IsVSyncEnabled() ? TEXT("an") : TEXT("aus"));
}
