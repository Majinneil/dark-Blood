#include "UI/SDBWorldMapWidget.h"

#include "Boss/DBBoss.h"
#include "Boss/DBBossDefinition.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Rendering/DrawElements.h"
#include "UI/DBUIStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "World/DBDungeon.h"
#include "World/DBRegionLife.h"
#include "World/DBRealmLayout.h"

#define LOCTEXT_NAMESPACE "DarkBloodWorldMap"

namespace
{
	const TCHAR* WorldMapTexturePath = TEXT("/Game/DarkBlood/UI/Map/T_WorldMap.T_WorldMap");

	const FLinearColor Shadow(0.f, 0.f, 0.f, 0.85f);
	const FLinearColor AllyBlue(0.35f, 0.65f, 1.f);
	const FLinearColor SettlementWhite(0.95f, 0.92f, 0.85f);
	const FLinearColor DungeonRed(1.f, 0.3f, 0.25f);

	/** Map image pixels covered by the realm square (the continent part of the world map). */
	FBox2D GetRealmPixels()
	{
		return FBox2D(DBRealm::ToMapPixel(FVector2D(-DBRealm::HalfSize)), DBRealm::ToMapPixel(FVector2D(DBRealm::HalfSize)));
	}

	/** The map image and its markers, drawn at the image's pixel size (the window scales it to fit). */
	class SDBWorldMapView : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDBWorldMapView) {}
			SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Owner)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Owner = InArgs._Owner;
			Pixels = GetRealmPixels();
			if (UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, WorldMapTexturePath, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Texture.Reset(Loaded);
				MapBrush.SetResourceObject(Loaded);
				MapBrush.DrawAs = ESlateBrushDrawType::Image;
				MapBrush.ImageSize = Pixels.GetSize();
				MapBrush.SetUVRegion(FBox2f(FVector2f(Pixels.Min / DBRealm::MapImageSize), FVector2f(Pixels.Max / DBRealm::MapImageSize)));
			}
			// Players move every frame.
			ForceVolatile(true);
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return Pixels.GetSize();
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Out,
			int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FVector2f Size = Geometry.GetLocalSize();
			if (Texture.IsValid())
			{
				FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), &MapBrush);
			}
			else
			{
				FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), DBUIStyle::WhiteBrush(), ESlateDrawEffect::None,
					FLinearColor(0.05f, 0.08f, 0.12f));
				Label(Out, LayerId + 1, Geometry, FVector2f(24.f, 24.f), LOCTEXT("Missing", "Kartentextur fehlt: Tools/UE58/db_import_world_map.py ausfuehren"),
					16, DBUIStyle::Gold);
			}
			const int32 Layer = LayerId + 2;

			for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
			{
				const FVector2f At = ToLocal(Site.Center, Size);
				Dot(Out, Layer, Geometry, At, 9.f, Shadow);
				Dot(Out, Layer, Geometry, At, 6.f, SettlementWhite);
				// The painted map already names regions; a settlement of the same name would cover that label.
				const bool bNamedOnMap = DBRealm::GetRegions().ContainsByPredicate([&Site](const FDBRealmRegion& Region)
				{
					return FCString::Stricmp(Region.DisplayName, Site.Name) == 0;
				});
				if (!bNamedOnMap)
				{
					Label(Out, Layer, Geometry, At + FVector2f(7.f, -9.f), FText::FromString(Site.Name), 10, SettlementWhite);
				}
			}

			// Dungeon gates: red diamonds (the client does not know which ones are cleared; the gate itself tells).
			for (const FDBDungeonSite& Site : DBDungeon::GetSites())
			{
				const FVector2f At = ToLocal(Site.Entrance, Size);
				const TArray<FVector2f> Diamond = {At + FVector2f(0.f, -8.f), At + FVector2f(8.f, 0.f), At + FVector2f(0.f, 8.f), At + FVector2f(-8.f, 0.f), At + FVector2f(0.f, -8.f)};
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Diamond, ESlateDrawEffect::None, Shadow, true, 5.f);
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Diamond, ESlateDrawEffect::None, DungeonRed, true, 2.5f);
				Label(Out, Layer, Geometry, At + FVector2f(9.f, -3.f), FText::Format(LOCTEXT("DungeonLabel", "{0} ({1})"), FText::FromString(Site.Name),
					FText::AsNumber(Site.Difficulty)), 10, DungeonRed);
			}

			const UWorld* MapWorld = Owner.IsValid() ? Owner->GetWorld() : nullptr;
			// Demon camps: a triangle, grey once broken.
			for (const FDBRealmRegion& Region : DBRealm::GetRegions())
			{
				const FVector Camp = DBRegions::GetCampLocation(Region.RegionId);
				if (Camp.IsZero())
				{
					continue;
				}
				const ADBDemonCamp* Actor = MapWorld ? ADBDemonCamp::Find(MapWorld, Region.RegionId) : nullptr;
				const FLinearColor Color = Actor && Actor->IsBroken() ? FLinearColor(0.45f, 0.45f, 0.45f) : FLinearColor(1.f, 0.55f, 0.15f);
				const FVector2f At = ToLocal(FVector2D(Camp) / 100.0, Size);
				const TArray<FVector2f> Triangle = {At + FVector2f(0.f, -8.f), At + FVector2f(7.f, 6.f), At + FVector2f(-7.f, 6.f), At + FVector2f(0.f, -8.f)};
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Triangle, ESlateDrawEffect::None, Shadow, true, 5.f);
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Triangle, ESlateDrawEffect::None, Color, true, 2.5f);
			}

			// Vassal and demon king arenas: a square in the boss color, grey once the boss fell.
			for (const UDBBossDefinition* Boss : DBBosses::GetAll())
			{
				const FVector Arena = DBBosses::GetArenaLocation(*Boss);
				if (Arena.IsZero())
				{
					continue;
				}
				const ADBBossArena* Actor = MapWorld ? ADBBossArena::Find(MapWorld, Boss->BossId) : nullptr;
				const bool bDefeated = Actor && Actor->GetArenaState() == EDBArenaState::Defeated;
				const FLinearColor Color = bDefeated ? FLinearColor(0.45f, 0.45f, 0.45f) : Boss->Color;
				const FVector2f At = ToLocal(FVector2D(Arena) / 100.0, Size);
				const float Half = Boss->Rank == EDBBossRank::DemonKing ? 10.f : 7.f;
				const TArray<FVector2f> Square = {At + FVector2f(-Half, -Half), At + FVector2f(Half, -Half), At + FVector2f(Half, Half), At + FVector2f(-Half, Half), At + FVector2f(-Half, -Half)};
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Square, ESlateDrawEffect::None, Shadow, true, 5.f);
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Square, ESlateDrawEffect::None, Color, true, 2.5f);
				Label(Out, Layer, Geometry, At + FVector2f(Half + 3.f, -3.f), Boss->DisplayName, 10, Color);
			}

			const APlayerController* Controller = Owner.Get();
			const APawn* LocalPawn = Controller ? Controller->GetPawn() : nullptr;
			const AGameStateBase* GameState = Controller && Controller->GetWorld() ? Controller->GetWorld()->GetGameState() : nullptr;
			if (GameState)
			{
				for (const APlayerState* PlayerState : GameState->PlayerArray)
				{
					const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
					if (!Pawn || Pawn == LocalPawn)
					{
						continue;
					}
					const FVector2f At = ToLocal(FVector2D(Pawn->GetActorLocation()) / 100.0, Size);
					Dot(Out, Layer + 1, Geometry, At, 14.f, Shadow);
					Dot(Out, Layer + 1, Geometry, At, 10.f, AllyBlue);
					Label(Out, Layer + 1, Geometry, At + FVector2f(10.f, -10.f), FText::FromString(PlayerState->GetPlayerName()), 12, AllyBlue);
				}
			}
			if (LocalPawn)
			{
				const FVector Location = LocalPawn->GetActorLocation();
				const FVector2f At = ToLocal(FVector2D(Location) / 100.0, Size);
				// Direction on the map (the projection stretches X and Y differently).
				FVector2f Dir = ToLocal(FVector2D(Location + LocalPawn->GetActorForwardVector() * 10000.0) / 100.0, Size) - At;
				Dir = Dir.IsNearlyZero() ? FVector2f(0.f, -1.f) : Dir.GetSafeNormal();
				const FVector2f Side(-Dir.Y, Dir.X);
				const TArray<FVector2f> Arrow = {At + Dir * 16.f, At - Dir * 10.f + Side * 10.f, At - Dir * 4.f, At - Dir * 10.f - Side * 10.f, At + Dir * 16.f};
				FSlateDrawElement::MakeLines(Out, Layer + 2, Geometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, Shadow, true, 6.f);
				FSlateDrawElement::MakeLines(Out, Layer + 3, Geometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, DBUIStyle::Gold, true, 3.f);
			}
			return Layer + 3;
		}

	private:
		FVector2f ToLocal(const FVector2D& Meters, const FVector2f& Size) const
		{
			return FVector2f((DBRealm::ToMapPixel(Meters) - Pixels.Min) / Pixels.GetSize()) * Size;
		}

		static void Dot(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& At, float Diameter, const FLinearColor& Color)
		{
			FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2f(Diameter), FSlateLayoutTransform(At - FVector2f(Diameter * 0.5f))),
				DBUIStyle::WhiteBrush(), ESlateDrawEffect::None, Color);
		}

		static void Label(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& At, const FText& Text, int32 FontSize,
			const FLinearColor& Color)
		{
			const FSlateFontInfo Font = DBUIStyle::Font(FontSize, "Bold");
			FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2f(400.f, 40.f), FSlateLayoutTransform(At + FVector2f(1.f))), Text, Font,
				ESlateDrawEffect::None, Shadow);
			FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2f(400.f, 40.f), FSlateLayoutTransform(At)), Text, Font,
				ESlateDrawEffect::None, Color);
		}

		TWeakObjectPtr<APlayerController> Owner;
		TStrongObjectPtr<UTexture2D> Texture;
		FSlateBrush MapBrush;
		FBox2D Pixels;
	};
}

void SDBWorldMapWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;

	ChildSlot
	.Padding(40.f)
	[
		SNew(SBorder)
		.BorderImage(DBUIStyle::WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.02f, 0.01f, 0.01f, 0.97f))
		.Padding(16.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(24, "Bold"))
					.ColorAndOpacity(DBUIStyle::Gold)
					.Text(LOCTEXT("Title", "Weltkarte"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(24.f, 0.f)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(16, "Bold"))
					.Text_Raw(this, &SDBWorldMapWidget::GetRegionText)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(DBUIStyle::Font(14))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					.Text(LOCTEXT("Close", "[M] Schliessen"))
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(SDBWorldMapView).Owner(Owner)
				]
			]
		]
	];
}

FText SDBWorldMapWidget::GetRegionText() const
{
	const APawn* Pawn = Owner.IsValid() ? Owner->GetPawn() : nullptr;
	if (!Pawn)
	{
		return FText::GetEmpty();
	}
	const FVector Location = Pawn->GetActorLocation() / 100.0;
	const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
	const int32 Index = DBRealm::FindRegionIndex(Location.X, Location.Y);
	return Regions.IsValidIndex(Index) ? FText::Format(LOCTEXT("Region", "Gebiet: {0}"), FText::FromString(Regions[Index].DisplayName)) : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
