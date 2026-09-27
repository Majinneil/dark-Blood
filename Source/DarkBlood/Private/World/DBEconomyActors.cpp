#include "World/DBEconomyActors.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DarkBlood.h"
#include "Engine/StaticMesh.h"
#include "Inventory/DBInventoryComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void SetupBody(UStaticMeshComponent* Body, const TCHAR* MeshPath, const FVector& Scale)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(MeshPath);
		if (Mesh.Succeeded())
		{
			Body->SetStaticMesh(Mesh.Object);
		}
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (Material.Succeeded())
		{
			Body->SetMaterial(0, Material.Object);
		}
		Body->SetRelativeScale3D(Scale);
		Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	}

	void Tint(UStaticMeshComponent* Body, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* Material = Body->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

// ---- Crafting station ------------------------------------------------------------------------

ADBCraftingStation::ADBCraftingStation()
{
	bReplicates = true;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	SetupBody(Body, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(1.2f, 0.8f, 0.9f));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Body);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	Label->SetAbsolute(false, false, true);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(16.f);
	Label->SetTextRenderColor(FColor(255, 170, 90));
	DisplayName = FText::FromString(TEXT("Schmiede [DEV]"));
}

void ADBCraftingStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBCraftingStation, StationId);
	DOREPLIFETIME(ADBCraftingStation, DisplayName);
}

void ADBCraftingStation::BeginPlay()
{
	Super::BeginPlay();
	Tint(Body, FLinearColor(0.25f, 0.2f, 0.18f));
	OnRep_Identity();
}

void ADBCraftingStation::Setup(FName InStationId, const FText& InDisplayName)
{
	StationId = InStationId;
	DisplayName = InDisplayName;
	OnRep_Identity();
}

void ADBCraftingStation::OnRep_Identity()
{
	Label->SetText(DisplayName);
}

FText ADBCraftingStation::GetInteractionText() const
{
	return FText::Format(NSLOCTEXT("DarkBlood", "UseStation", "Benutzen: {0}"), DisplayName);
}

void ADBCraftingStation::Interact(APlayerController* User)
{
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(User))
	{
		Controller->ClientOpenCrafting(this);
	}
}

// ---- Loot chest --------------------------------------------------------------------------------

ADBLootChest::ADBLootChest()
{
	bReplicates = true;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	SetupBody(Body, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.9f, 0.6f, 0.6f));
}

void ADBLootChest::BeginPlay()
{
	Super::BeginPlay();
	Tint(Body, FLinearColor(0.55f, 0.35f, 0.1f));
}

void ADBLootChest::Setup(FName InLootTableId)
{
	LootTableId = InLootTableId;
}

FText ADBLootChest::GetInteractionText() const
{
	return NSLOCTEXT("DarkBlood", "OpenChest", "Oeffnen: Truhe");
}

bool ADBLootChest::CanInteract(const APawn* User) const
{
	const ADBPlayerState* PlayerState = User ? User->GetPlayerState<ADBPlayerState>() : nullptr;
	// Clients do not know who opened it; the server decides and says "empty".
	return PlayerState && (!HasAuthority() || !OpenedBy.Contains(PlayerState->GetProfile().CharacterId));
}

void ADBLootChest::Interact(APlayerController* User)
{
	ADBPlayerState* PlayerState = User ? User->GetPlayerState<ADBPlayerState>() : nullptr;
	ADBPlayerController* Controller = Cast<ADBPlayerController>(User);
	if (!PlayerState || !Controller)
	{
		return;
	}
	if (OpenedBy.Contains(PlayerState->GetProfile().CharacterId))
	{
		Controller->ClientShowNotification(NSLOCTEXT("DarkBlood", "ChestEmpty", "Die Truhe ist leer."));
		return;
	}
	OpenedBy.Add(PlayerState->GetProfile().CharacterId);
	PlayerState->GetInventory()->GrantLootTable(LootTableId, TEXT("Truhe"));
}
