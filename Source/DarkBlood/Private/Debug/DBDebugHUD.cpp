#include "Debug/DBDebugHUD.h"

#include "Abilities/DBAttributeSet.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBRegionDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Framework/DBGameState.h"
#include "Inventory/DBInventoryComponent.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "Quest/DBQuestComponent.h"
#include "World/DBWorldStateComponent.h"

void ADBDebugHUD::DrawLine(const FString& Text, float& Y, const FLinearColor& Color)
{
	DrawText(Text, Color, 24.f, Y, GEngine ? GEngine->GetSmallFont() : nullptr, 1.1f);
	Y += 16.f;
}

void ADBDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!bShowOverlay || !Canvas)
	{
		return;
	}

	float Y = 24.f;
	const FLinearColor Title(0.85f, 0.1f, 0.1f);
	const FLinearColor Dim(0.7f, 0.7f, 0.7f);
	DrawLine(TEXT("DARK BLOOD - Entwicklungsansicht (DBToggleDebugHUD)"), Y, Title);

	const ADBPlayerState* PlayerState = GetOwningPlayerController() ? GetOwningPlayerController()->GetPlayerState<ADBPlayerState>() : nullptr;
	if (!PlayerState || !PlayerState->IsCharacterReady())
	{
		DrawLine(TEXT("Charakter wird geladen ..."), Y, Dim);
		return;
	}

	const UDBProgressionComponent* Progression = PlayerState->GetProgression();
	const UDBAttributeSet* Attributes = PlayerState->GetAttributeSet();
	const UDBInventoryComponent* Inventory = PlayerState->GetInventory();

	DrawLine(FString::Printf(TEXT("%s  |  Klasse: %s  |  Stufe %d  |  Staerke %d"), *PlayerState->GetPlayerName(),
		*PlayerState->GetProfile().ClassId.ToString(), Progression->GetLevel(), Progression->GetPowerRating()), Y);
	DrawLine(FString::Printf(TEXT("XP %lld / %lld  |  Faehigkeitspunkte %d"), Progression->GetXpIntoLevel(), Progression->GetXpToNextLevel(),
		Progression->GetUnspentSkillPoints()), Y);
	if (Attributes)
	{
		DrawLine(FString::Printf(TEXT("Leben %.0f/%.0f  Ausdauer %.0f/%.0f  Mana %.0f/%.0f"), Attributes->GetHealth(), Attributes->GetMaxHealth(),
			Attributes->GetStamina(), Attributes->GetMaxStamina(), Attributes->GetMana(), Attributes->GetMaxMana()), Y, FLinearColor(0.9f, 0.85f, 0.7f));
	}
	int32 Capacity = 0;
	for (const FDBInventorySectionView& Section : Inventory->GetSections())
	{
		Capacity += Section.Capacity;
	}
	DrawLine(FString::Printf(TEXT("Mon %lld  |  Inventar %d Stapel / %d Plaetze  |  Nachlieferung %d"), Inventory->GetCurrency(),
		Inventory->GetEntries().Num(), Capacity, Inventory->GetPendingDeliveryCount()), Y);

	EDBDangerTier Tier;
	int32 Min = 0;
	int32 Max = 0;
	int32 Power = 0;
	if (PlayerState->GetCurrentDanger(Tier, Min, Max, Power))
	{
		const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
		const UDBRegionDefinition* Region = Data ? Data->FindRegion(PlayerState->GetCurrentRegionId()) : nullptr;
		const FString TierName = StaticEnum<EDBDangerTier>()->GetDisplayNameTextByValue(static_cast<int64>(Tier)).ToString().ToUpper();
		const FLinearColor TierColor = Tier >= EDBDangerTier::Dangerous ? FLinearColor(1.f, 0.2f, 0.1f) : FLinearColor(0.6f, 0.9f, 0.6f);
		DrawLine(FString::Printf(TEXT("Region: %s  |  GEFAHRENSTUFE: %s  |  Empfohlene Staerke: %d-%d  |  Deine Staerke: %d"),
			Region ? *Region->DisplayName.ToString() : TEXT("?"), *TierName, Min, Max, Power), Y, TierColor);
	}
	else
	{
		DrawLine(TEXT("Region: (keine)"), Y, Dim);
	}

	if (const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>())
	{
		const UDBWorldStateComponent* World = GameState->GetWorldState();
		const float Hour = World->GetTimeOfDay();
		DrawLine(FString::Printf(TEXT("Tag %d, %02d:%02d %s  |  Vasallen besiegt: %d/14%s"), World->GetDay(), FMath::FloorToInt(Hour),
			FMath::FloorToInt(FMath::Fmod(Hour, 1.f) * 60.f), World->IsNight() ? TEXT("(Nacht)") : TEXT("(Tag)"), World->GetDefeatedVassalCount(),
			World->IsFinalRegionOpen() ? TEXT("  |  DAS ENDE ist offen") : TEXT("")), Y);

		for (const FDBQuestProgressView& Quest : GameState->GetSharedQuests()->GetQuests())
		{
			if (Quest.Status == EDBQuestStatus::Active || Quest.Status == EDBQuestStatus::ReadyToTurnIn)
			{
				FString Counts;
				for (const int32 Count : Quest.ObjectiveCounts)
				{
					Counts += FString::Printf(TEXT(" %d"), Count);
				}
				DrawLine(FString::Printf(TEXT("Hauptquest: %s [%s]"), *Quest.QuestId.ToString(), *Counts), Y, FLinearColor(1.f, 0.8f, 0.3f));
			}
		}
	}

	if (const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this); Data && Data->IsUsingDevelopmentContent())
	{
		DrawLine(TEXT("Hinweis: Entwicklungs-Platzhalterdaten aktiv (fehlende Assets)"), Y, Dim);
	}
}
