#include "World/DBCarriageStation.h"

#include "Character/DBHorse.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "Inventory/DBInventoryComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "UObject/ConstructorHelpers.h"
#include "World/DBRealmLayout.h"
#include "World/DBWorldStateComponent.h"
#include "World/DBTeleport.h"

#define LOCTEXT_NAMESPACE "DarkBloodCarriage"

namespace
{
	/** Carriages travel ~20 km/h; the fare is a base plus a price per kilometer. */
	constexpr double CarriageKmPerHour = 20.0;
	constexpr int64 BaseFare = 10;
	constexpr double FarePerKm = 6.0;
	/** Players must stand this close to the station to travel (cm). */
	constexpr double TravelRange = 800.0;
}

ADBCarriageStation::ADBCarriageStation()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	struct FPart
	{
		const TCHAR* Name;
		bool bCube;
		FVector Location;
		FVector Scale;
		FRotator Rotation;
	};
	// A covered cart on two wheels and a sign post.
	const FPart Shapes[] = {
		{TEXT("Cart"), true, FVector(0.f, 0.f, 110.f), FVector(2.4f, 1.4f, 0.9f), FRotator::ZeroRotator},
		{TEXT("Cover"), true, FVector(0.f, 0.f, 190.f), FVector(2.2f, 1.3f, 0.7f), FRotator::ZeroRotator},
		{TEXT("WheelL"), false, FVector(-30.f, 80.f, 60.f), FVector(1.1f, 1.1f, 0.12f), FRotator(0.f, 0.f, 90.f)},
		{TEXT("WheelR"), false, FVector(-30.f, -80.f, 60.f), FVector(1.1f, 1.1f, 0.12f), FRotator(0.f, 0.f, 90.f)},
		{TEXT("Pole"), true, FVector(170.f, 0.f, 70.f), FVector(1.4f, 0.08f, 0.08f), FRotator(0.f, 0.f, 0.f)},
		{TEXT("Post"), false, FVector(0.f, 190.f, 120.f), FVector(0.12f, 0.12f, 2.4f), FRotator::ZeroRotator},
	};
	for (const FPart& Shape : Shapes)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Shape.Name);
		Part->SetupAttachment(Root);
		Part->SetRelativeLocation(Shape.Location);
		Part->SetRelativeRotation(Shape.Rotation);
		Part->SetRelativeScale3D(Shape.Scale);
		Part->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		Part->SetCanEverAffectNavigation(false);
		if (UStaticMesh* Mesh = Shape.bCube ? Cube.Object : Cylinder.Object)
		{
			Part->SetStaticMesh(Mesh);
		}
		if (ShapeMaterial.Succeeded())
		{
			Part->SetMaterial(0, ShapeMaterial.Object);
		}
		Parts.Add(Part);
	}
	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Root);
	Sign->SetRelativeLocation(FVector(0.f, 190.f, 270.f));
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetWorldSize(36.f);
	Sign->SetTextRenderColor(FColor(235, 200, 110));
	Sign->SetText(LOCTEXT("Sign", "Kutsche"));
}

void ADBCarriageStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBCarriageStation, SiteIndex);
}

void ADBCarriageStation::SetSite(int32 InSiteIndex)
{
	SiteIndex = InSiteIndex;
	OnRep_Site();
}

void ADBCarriageStation::OnRep_Site()
{
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	if (Settlements.IsValidIndex(SiteIndex))
	{
		Sign->SetText(FText::Format(LOCTEXT("SignAt", "Kutsche - {0}"), FText::FromString(Settlements[SiteIndex].Name)));
	}
	// Wood brown cart, pale canvas.
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		if (UMaterialInstanceDynamic* Material = Parts[Index]->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Index == 1 ? FLinearColor(0.6f, 0.55f, 0.45f) : FLinearColor(0.15f, 0.08f, 0.03f));
		}
	}
}

FText ADBCarriageStation::GetInteractionText() const
{
	return LOCTEXT("Use", "Kutsche nehmen");
}

void ADBCarriageStation::Interact(APlayerController* User)
{
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(User); Controller && HasAuthority())
	{
		Controller->ClientOpenCarriage(this);
	}
}

int64 ADBCarriageStation::GetFare(int32 From, int32 To)
{
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	if (!Settlements.IsValidIndex(From) || !Settlements.IsValidIndex(To))
	{
		return 0;
	}
	const double Km = FVector2D::Distance(Settlements[From].Center, Settlements[To].Center) / 1000.0;
	return BaseFare + static_cast<int64>(FMath::RoundToDouble(Km * FarePerKm));
}

float ADBCarriageStation::GetTravelHours(int32 From, int32 To)
{
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	if (!Settlements.IsValidIndex(From) || !Settlements.IsValidIndex(To))
	{
		return 0.f;
	}
	return static_cast<float>(0.5 + FVector2D::Distance(Settlements[From].Center, Settlements[To].Center) / 1000.0 / CarriageKmPerHour);
}

FVector ADBCarriageStation::GetStationLocation(int32 SiteIndex)
{
	const FDBRealmSettlement& Site = DBRealm::GetSettlements()[SiteIndex];
	// At the edge of the leveled ground, away from the sea for harbor towns.
	const FVector2D Away = Site.SeaDirection.IsNearlyZero() ? FVector2D(1.0, 0.0) : -Site.SeaDirection;
	const FVector2D Meters = Site.Center + Away * (Site.Radius * 0.9);
	return FVector(Meters.X * 100.0, Meters.Y * 100.0, Site.GroundHeight * 100.0);
}

ADBCarriageStation* ADBCarriageStation::FindStation(const UWorld* World, int32 SiteIndex)
{
	for (TActorIterator<ADBCarriageStation> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->SiteIndex == SiteIndex)
		{
			return *It;
		}
	}
	return nullptr;
}

void ADBCarriageStation::SpawnStations(UWorld* World)
{
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	for (int32 Index = 0; Index < Settlements.Num(); ++Index)
	{
		if (FindStation(World, Index))
		{
			continue;
		}
		const FVector Location = GetStationLocation(Index);
		const FVector ToCenter = FVector(Settlements[Index].Center.X * 100.0, Settlements[Index].Center.Y * 100.0, Location.Z) - Location;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADBCarriageStation* Station = World->SpawnActor<ADBCarriageStation>(ADBCarriageStation::StaticClass(), Location,
				FRotator(0.f, ToCenter.Rotation().Yaw + 90.f, 0.f), Params))
		{
			Station->SetSite(Index);
		}
	}
}

bool ADBCarriageStation::Travel(APlayerController* User, int32 Destination, FText& OutReason)
{
	const TArray<FDBRealmSettlement>& Settlements = DBRealm::GetSettlements();
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	ADBPlayerState* PlayerState = User ? User->GetPlayerState<ADBPlayerState>() : nullptr;
	if (!HasAuthority() || !Pawn || !PlayerState || !Settlements.IsValidIndex(Destination) || Destination == SiteIndex)
	{
		OutReason = LOCTEXT("Invalid", "Kein gueltiges Ziel.");
		return false;
	}
	if (FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > TravelRange)
	{
		OutReason = LOCTEXT("TooFar", "Du bist zu weit von der Kutsche entfernt.");
		return false;
	}
	const int64 Fare = GetFare(SiteIndex, Destination);
	UDBInventoryComponent* Inventory = PlayerState->GetInventory();
	if (!Inventory || Inventory->GetServerCurrency() < Fare || !Inventory->SpendCurrency(Fare))
	{
		OutReason = FText::Format(LOCTEXT("NoMoney", "Die Fahrt kostet {0} Mon."), FText::AsNumber(Fare));
		return false;
	}
	if (ADBHorse* Horse = ADBHorse::FindRiddenBy(Pawn))
	{
		Horse->Interact(User); // the horse stays; call it at the destination
	}
	// Arrive next to the destination's carriage, facing the settlement.
	const FVector Arrival = GetStationLocation(Destination);
	const FVector Center(Settlements[Destination].Center.X * 100.0, Settlements[Destination].Center.Y * 100.0, Arrival.Z);
	const FVector Spot = Arrival + (Center - Arrival).GetSafeNormal2D() * 400.f + FVector(0.f, 0.f, 120.f);
	DBTeleport::MovePawn(Pawn, Spot, (Center - Arrival).Rotation().Yaw);

	const float Hours = GetTravelHours(SiteIndex, Destination);
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	const bool bAlone = GameState && GameState->PlayerArray.Num() <= 1;
	if (bAlone && GameState->GetWorldState())
	{
		GameState->GetWorldState()->SkipHours(Hours);
	}
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(User))
	{
		Controller->ClientShowNotification(FText::Format(LOCTEXT("Arrived", "Ankunft in {0} nach {1} Std. ({2} Mon)"),
			FText::FromString(Settlements[Destination].Name), FText::AsNumber(FMath::RoundToInt(Hours * 10.f) / 10.f), FText::AsNumber(Fare)));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Carriage: %s %s -> %s, %lld Mon, %.1f h%s"), *PlayerState->GetPlayerName(), Settlements[SiteIndex].Name,
		Settlements[Destination].Name, Fare, Hours, bAlone ? TEXT(" (time passed)") : TEXT(" (co-op: no time skip)"));
	return true;
}

#undef LOCTEXT_NAMESPACE
