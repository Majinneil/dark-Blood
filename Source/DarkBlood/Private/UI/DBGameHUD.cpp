#include "UI/DBGameHUD.h"

#include "Data/DBClassDefinition.h"
#include "Data/DBGameDataSubsystem.h"
#include "Dialogue/DBDialogueComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Player/DBPlayerController.h"
#include "UI/SDBCharacterCreatorWidget.h"
#include "UI/SDBDialogueWidget.h"
#include "UI/SDBGameHudWidget.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Settings/DBGameUserSettings.h"
#include "UI/SDBInventoryWidget.h"
#include "UI/SDBSettingsWidget.h"
#include "UI/SDBSkillTreeWidget.h"
#include "UI/SDBWorldMapWidget.h"
#include "Widgets/SWeakWidget.h"

void ADBGameHUD::BeginPlay()
{
	Super::BeginPlay();

	ADBPlayerController* Controller = Cast<ADBPlayerController>(GetOwningPlayerController());
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Controller || !Controller->IsLocalController() || !Viewport)
	{
		return; // dedicated servers and headless runs have no UI
	}

	// Graphics: recommended settings on the first start (desktop resolution, TSR, ray tracing when the GPU has it).
	// Automated and explicitly sized runs (-ResX, -windowed) keep their command line settings.
	if (UDBGameUserSettings* Graphics = UDBGameUserSettings::Get())
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		int32 ExplicitWidth = 0;
		const bool bExplicitMode = FApp::IsUnattended() || FParse::Param(CommandLine, TEXT("windowed")) || FParse::Value(CommandLine, TEXT("ResX="), ExplicitWidth);
		if (!Graphics->bRecommendedDefaultsApplied && !bExplicitMode)
		{
			Graphics->ApplyRecommendedDefaults();
			Graphics->ApplySettings(false);
			Graphics->SaveSettings();
		}
		else
		{
			Graphics->ApplyNonResolutionSettings();
		}
	}

	HudWidget = SNew(SDBGameHudWidget).Owner(Controller);
	Viewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), 10);

	DialogueWidget = SNew(SDBDialogueWidget).Dialogue(Controller->GetDialogue());
	Viewport->AddViewportWidgetContent(DialogueWidget.ToSharedRef(), 20);
	DialogueHandle = Controller->GetDialogue()->OnDialogueChanged.AddUObject(this, &ADBGameHUD::OnDialogueChanged);
	bUIReady = true;

	// The creator may have been requested before the HUD existed (host login happens before BeginPlay).
	if (Controller->IsCharacterCreationPending())
	{
		ShowCharacterCreator();
	}
}

void ADBGameHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		if (HudWidget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(HudWidget.ToSharedRef());
		}
		if (DialogueWidget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(DialogueWidget.ToSharedRef());
		}
		if (CreatorRoot.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(CreatorRoot.ToSharedRef());
		}
		if (SkillTreeRoot.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(SkillTreeRoot.ToSharedRef());
		}
		if (SettingsRoot.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(SettingsRoot.ToSharedRef());
		}
		if (InventoryRoot.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(InventoryRoot.ToSharedRef());
		}
		if (MapRoot.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(MapRoot.ToSharedRef());
		}
		if (CraftingWidget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(CraftingWidget.ToSharedRef());
		}
	}
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(GetOwningPlayerController()))
	{
		Controller->GetDialogue()->OnDialogueChanged.Remove(DialogueHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ADBGameHUD::ShowNotification(const FText& Text)
{
	if (HudWidget.IsValid())
	{
		HudWidget->ShowNotification(Text);
	}
}

void ADBGameHUD::ShowCharacterCreator()
{
	ADBPlayerController* Controller = Cast<ADBPlayerController>(GetOwningPlayerController());
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!bUIReady || !Controller || !Viewport || CreatorRoot.IsValid())
	{
		return;
	}

	TArray<FDBCreatorClassOption> Options;
	if (const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this))
	{
		for (const UDBClassDefinition* Class : Data->GetAllClasses())
		{
			Options.Add({Class->ClassId, Class->DisplayName, Class->Description});
		}
	}
	Options.Sort([](const FDBCreatorClassOption& A, const FDBCreatorClassOption& B) { return A.DisplayName.CompareTo(B.DisplayName) < 0; });

	const TWeakObjectPtr<ADBPlayerController> WeakController = Controller;
	CreatorWidget = SNew(SDBCharacterCreatorWidget)
		.Classes(Options)
		.OnCreate_Lambda([WeakController](const FString& Name, FName ClassId, const FDBAppearance& Appearance) -> FText
		{
			FString Error;
			if (!WeakController.IsValid() || !WeakController->SubmitCharacterCreation(Name, ClassId, Appearance, Error))
			{
				return FText::FromString(Error);
			}
			return FText::GetEmpty();
		});
	CreatorRoot = CreatorWidget;
	Viewport->AddViewportWidgetContent(CreatorRoot.ToSharedRef(), 100);

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(CreatorWidget->GetFocusTarget());
	Controller->SetInputMode(Mode);
	Controller->SetShowMouseCursor(true);
}

void ADBGameHUD::HideCharacterCreator()
{
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport(); Viewport && CreatorRoot.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(CreatorRoot.ToSharedRef());
	}
	CreatorRoot.Reset();
	CreatorWidget.Reset();
	UpdateInputMode();
}

void ADBGameHUD::ToggleSkillTree()
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!bUIReady || !Viewport || CreatorRoot.IsValid())
	{
		return;
	}
	if (SkillTreeRoot.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(SkillTreeRoot.ToSharedRef());
		SkillTreeRoot.Reset();
	}
	else
	{
		SkillTreeRoot = SNew(SDBSkillTreeWidget).Owner(GetOwningPlayerController());
		Viewport->AddViewportWidgetContent(SkillTreeRoot.ToSharedRef(), 50);
	}
	UpdateInputMode();
}

void ADBGameHUD::ToggleInventory()
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!bUIReady || !Viewport || CreatorRoot.IsValid())
	{
		return;
	}
	if (InventoryRoot.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(InventoryRoot.ToSharedRef());
		InventoryRoot.Reset();
	}
	else
	{
		InventoryRoot = SNew(SDBInventoryWidget).Owner(GetOwningPlayerController());
		Viewport->AddViewportWidgetContent(InventoryRoot.ToSharedRef(), 50);
	}
	UpdateInputMode();
}

void ADBGameHUD::ToggleMap()
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!bUIReady || !Viewport || CreatorRoot.IsValid())
	{
		return;
	}
	if (MapRoot.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(MapRoot.ToSharedRef());
		MapRoot.Reset();
	}
	else
	{
		MapRoot = SNew(SDBWorldMapWidget).Owner(GetOwningPlayerController());
		Viewport->AddViewportWidgetContent(MapRoot.ToSharedRef(), 45);
	}
	UpdateInputMode();
}

void ADBGameHUD::ToggleSettings()
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!bUIReady || !Viewport || CreatorRoot.IsValid())
	{
		return;
	}
	if (SettingsRoot.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(SettingsRoot.ToSharedRef());
		SettingsRoot.Reset();
	}
	else
	{
		SettingsRoot = SNew(SDBSettingsWidget).OnClose(FSimpleDelegate::CreateUObject(this, &ADBGameHUD::ToggleSettings));
		Viewport->AddViewportWidgetContent(SettingsRoot.ToSharedRef(), 60);
	}
	UpdateInputMode();
}

void ADBGameHUD::ShowCrafting(AActor* Station)
{
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!bUIReady || !Viewport || CreatorRoot.IsValid())
	{
		return;
	}
	HideCrafting();
	CraftingWidget = SNew(SDBCraftingWidget)
		.Owner(GetOwningPlayerController())
		.Station(Station)
		.OnClose(FSimpleDelegate::CreateUObject(this, &ADBGameHUD::HideCrafting));
	Viewport->AddViewportWidgetContent(CraftingWidget.ToSharedRef(), 55);
	UpdateInputMode();
}

void ADBGameHUD::HideCrafting()
{
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport(); Viewport && CraftingWidget.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(CraftingWidget.ToSharedRef());
	}
	CraftingWidget.Reset();
	UpdateInputMode();
}

void ADBGameHUD::DrawHUD()
{
	Super::DrawHUD();
	// Walking away from the station closes its window.
	if (CraftingWidget.IsValid())
	{
		const APawn* Pawn = GetOwningPawn();
		const AActor* Station = CraftingWidget->GetStation();
		if (!Pawn || !Station || FVector::Dist2D(Pawn->GetActorLocation(), Station->GetActorLocation()) > 700.f)
		{
			HideCrafting();
		}
	}
}

void ADBGameHUD::OnDialogueChanged()
{
	if (DialogueWidget.IsValid())
	{
		DialogueWidget->Refresh();
	}
	UpdateInputMode();
}

void ADBGameHUD::UpdateInputMode()
{
	ADBPlayerController* Controller = Cast<ADBPlayerController>(GetOwningPlayerController());
	if (!Controller || CreatorRoot.IsValid())
	{
		return;
	}
	if (Controller->GetDialogue()->IsDialogueOpen() || SkillTreeRoot.IsValid() || InventoryRoot.IsValid() || SettingsRoot.IsValid() || CraftingWidget.IsValid()
		|| MapRoot.IsValid())
	{
		// Mouse for the option buttons; keys 1-4 and E keep working through game input.
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(Mode);
		Controller->SetShowMouseCursor(true);
	}
	else
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(false);
	}
}
