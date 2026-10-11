// Graphics settings on top of the engine's user settings (GameUserSettings.ini): quality preset (engine scalability
// incl. cinematic), resolution / window mode, TSR upscaling, Lumen hardware ray tracing, frame rate limit, VSync.
// Presentation only - nothing in gameplay reads these values.
#pragma once

#include "GameFramework/GameUserSettings.h"

#include "DBGameUserSettings.generated.h"

/** TSR input resolution relative to the output (4K output + Performance = 1080p rendering). */
UENUM(BlueprintType)
enum class EDBUpscaling : uint8
{
	Native UMETA(DisplayName = "Nativ (100 %)"),
	Quality UMETA(DisplayName = "Qualitaet (67 %)"),
	Balanced UMETA(DisplayName = "Ausgewogen (58 %)"),
	Performance UMETA(DisplayName = "Leistung (50 %)"),
	UltraPerformance UMETA(DisplayName = "Ultra-Leistung (33 %)"),
};

UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class DARKBLOOD_API UDBGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UDBGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	/** First start on this machine: preset, upscaling and ray tracing picked from the display and the GPU. */
	void ApplyRecommendedDefaults();

	static float GetScreenPercentage(EDBUpscaling Mode);
	/** The GPU and RHI can run Lumen with hardware ray tracing (project built with ray tracing support). */
	static bool IsHardwareRayTracingAvailable();

	/** Human-readable summary for logs and the performance snapshot. */
	FString Describe() const;

	UPROPERTY(Config)
	EDBUpscaling Upscaling = EDBUpscaling::Quality;

	UPROPERTY(Config)
	bool bHardwareRayTracing = true;

	UPROPERTY(Config)
	bool bRecommendedDefaultsApplied = false;

	/** Volumes 0..1 (Phase 18): everything, music, effects (combat, steps, ambience, interface), voices. */
	UPROPERTY(Config)
	float MasterVolume = 1.f;

	UPROPERTY(Config)
	float MusicVolume = 0.8f;

	UPROPERTY(Config)
	float EffectsVolume = 1.f;

	UPROPERTY(Config)
	float VoiceVolume = 1.f;
};
