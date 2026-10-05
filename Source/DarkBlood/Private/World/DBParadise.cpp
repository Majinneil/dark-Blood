#include "World/DBParadise.h"

#include "Art/DBArtMaterials.h"
#include "Art/DBModelLibrary.h"
#include "Boss/DBBossDefinition.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DarkBlood.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Quest/DBQuestSubsystem.h"
#include "World/DBRealmLayout.h"
#include "World/DBWorldStateComponent.h"

#include "DarkBloodRules/WorldState.h"

#define LOCTEXT_NAMESPACE "DarkBloodParadise"

namespace
{
	/** The island floats this high over the sea (cm), far above the peaks of DAS ENDE. */
	constexpr double IslandAltitude = 150000.0;
	constexpr float IslandRadius = 5500.f;

	FVector ThroneLocation()
	{
		for (const UDBBossDefinition* Boss : DBBosses::GetAll())
		{
			if (Boss->Rank == EDBBossRank::DemonKing)
			{
				return DBBosses::GetArenaLocation(*Boss);
			}
		}
		return FVector::ZeroVector;
	}

	UStaticMesh* ParadiseMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	UStaticMeshComponent* AddStaticPart(AActor* Owner, USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& Out, UStaticMesh* Mesh,
		const FTransform& Transform, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Part->SetMobility(Parent->Mobility);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeTransform(Transform);
		if (Material)
		{
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Part->SetMaterial(Slot, Material);
			}
		}
		Part->SetCanEverAffectNavigation(false);
		Part->RegisterComponent();
		Out.Add(Part);
		return Part;
	}

	void AddModel(AActor* Owner, USceneComponent* Parent, TArray<TObjectPtr<UStaticMeshComponent>>& Out, const TCHAR* Key, float Height, const FVector& At, float Yaw,
		UMaterialInterface* Material = nullptr)
	{
		const FTransform Placement(FRotator(0.f, Yaw, 0.f), At);
		for (const DBModels::FPlacedPart& Piece : DBModels::GetNormalizedParts(Key, Height))
		{
			AddStaticPart(Owner, Parent, Out, Piece.Mesh, Piece.Local * Placement, Material);
		}
	}

	void TeleportPawn(APawn* Pawn, const FVector& Location, float Yaw)
	{
		if (!Pawn)
		{
			return;
		}
		Pawn->TeleportTo(Location, FRotator(0.f, Yaw, 0.f));
		if (AController* Controller = Pawn->GetController())
		{
			Controller->SetControlRotation(FRotator(-8.f, Yaw, 0.f));
		}
	}
}

// ---- Placement -------------------------------------------------------------------------------------------------

FVector DBParadise::GetIslandCenter()
{
	const FVector Throne = ThroneLocation();
	return FVector(Throne.X, Throne.Y, IslandAltitude);
}

bool DBParadise::IsInParadise(const FVector& WorldLocation)
{
	const FVector Center = GetIslandCenter();
	return WorldLocation.Z > Center.Z - 6000.0 && FVector::Dist2D(WorldLocation, Center) < 20000.0;
}

void DBParadise::SpawnParadise(UWorld* World)
{
	if (!World || ADBParadiseGate::Find(World, EDBParadiseGate::Home) || ThroneLocation().IsZero())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Center = GetIslandCenter();
	World->SpawnActor<ADBParadiseIsland>(ADBParadiseIsland::StaticClass(), Center, FRotator::ZeroRotator, Params);
	// The gate of light at the heart of the throne (inactive until the king falls) and the way home on the island.
	if (ADBParadiseGate* Up = World->SpawnActor<ADBParadiseGate>(ADBParadiseGate::StaticClass(), ThroneLocation() + FVector(0.f, 0.f, 25.f), FRotator::ZeroRotator, Params))
	{
		Up->Setup(EDBParadiseGate::ToParadise);
	}
	if (ADBParadiseGate* Home = World->SpawnActor<ADBParadiseGate>(ADBParadiseGate::StaticClass(), Center + FVector(-IslandRadius * 0.78f, 0.f, 0.f), FRotator::ZeroRotator, Params))
	{
		Home->Setup(EDBParadiseGate::Home);
	}
	World->SpawnActor<ADBPeaceShrine>(ADBPeaceShrine::StaticClass(), Center + FVector(IslandRadius * 0.45f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f), Params);
	UE_LOG(LogDarkBlood, Display, TEXT("DAS PARADIES: island at (%.0f, %.0f, %.0f) m"), Center.X / 100.0, Center.Y / 100.0, Center.Z / 100.0);
}

// ---- Gate ------------------------------------------------------------------------------------------------------

ADBParadiseGate::ADBParadiseGate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetBoxExtent(FVector(250.f, 350.f, 300.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Overlap);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Trigger);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 800.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(70.f);
	Label->SetTextRenderColor(FColor(255, 236, 190));
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Trigger);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	Glow->SetIntensity(12000.f);
	Glow->SetAttenuationRadius(1500.f);
	Glow->SetLightColor(FLinearColor(1.f, 0.85f, 0.55f));
	Glow->SetCastShadows(false);
}

void ADBParadiseGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBParadiseGate, Kind);
	DOREPLIFETIME(ADBParadiseGate, bActive);
}

void ADBParadiseGate::Setup(EDBParadiseGate InKind)
{
	Kind = InKind;
	bActive = Kind == EDBParadiseGate::Home;
	OnRep_Gate();
}

ADBParadiseGate* ADBParadiseGate::Find(const UWorld* World, EDBParadiseGate InKind)
{
	for (TActorIterator<ADBParadiseGate> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->Kind == InKind)
		{
			return *It;
		}
	}
	return nullptr;
}

void ADBParadiseGate::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Gate();
}

void ADBParadiseGate::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	// A torii of light: the frame in pale stone, a curtain of golden light inside.
	AddModel(this, Trigger, Parts, TEXT("torii_game"), 820.f, FVector(0.f, 0.f, -300.f), 0.f, UDBArtMaterialSubsystem::Get(EDBArtMaterial::StoneTemple));
	UMaterialInterface* Curtain = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Master/M_DB_BloodBarrier.M_DB_BloodBarrier"), nullptr,
		LOAD_NoWarn | LOAD_Quiet);
	if (Curtain)
	{
		UMaterialInstanceDynamic* Golden = UMaterialInstanceDynamic::Create(Curtain, this);
		Golden->SetVectorParameterValue(TEXT("BarrierColor"), FLinearColor(1.f, 0.8f, 0.45f));
		Golden->SetScalarParameterValue(TEXT("Intensity"), 1.4f);
		Golden->SetScalarParameterValue(TEXT("Opacity"), 0.3f);
		Curtain = Golden;
	}
	if (UStaticMesh* Cube = ParadiseMesh(TEXT("Cube")))
	{
		Light = AddStaticPart(this, Trigger, Parts, Cube, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 40.f), FVector(4.2f, 0.06f, 6.2f)),
			Curtain ? Curtain : UDBArtMaterialSubsystem::Get(EDBArtMaterial::LanternFire));
		Light->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
		Light->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Light->SetCastShadow(false);
	}
}

void ADBParadiseGate::OnRep_Gate()
{
	Build();
	// The throne gate only shows once it is open.
	const bool bVisible = bActive;
	for (UStaticMeshComponent* Part : Parts)
	{
		Part->SetVisibility(bVisible);
		Part->SetCollisionEnabled(bVisible && Part != Light ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	Glow->SetVisibility(bVisible);
	Label->SetVisibility(bVisible);
	Trigger->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Label->SetText(Kind == EDBParadiseGate::Home ? LOCTEXT("GateHome", "Rueckkehr in die Welt") : LOCTEXT("GateUp", "Pforte ins Paradies"));
}

FText ADBParadiseGate::GetInteractionText() const
{
	return Kind == EDBParadiseGate::Home ? LOCTEXT("UseHome", "Zurueck in die Hauptstadt") : LOCTEXT("UseUp", "Durch die Pforte ins Paradies schreiten");
}

void ADBParadiseGate::Interact(APlayerController* User)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	if (!HasAuthority() || !bActive || !Pawn)
	{
		return;
	}
	if (Kind == EDBParadiseGate::ToParadise)
	{
		const FVector Center = DBParadise::GetIslandCenter();
		TeleportPawn(Pawn, Center + FVector(-IslandRadius * 0.65f, 0.f, 200.f), 0.f);
		if (UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this))
		{
			Quests->ReportEvent(EDBObjectiveKind::Interact, TEXT("ParadiseGate"), 1, User->PlayerState);
		}
	}
	else
	{
		const FDBRealmRegion& Capital = DBRealm::GetCapital();
		const FVector2D At = Capital.Center + FVector2D(0.0, 120.0);
		TeleportPawn(Pawn, FVector(At.X * 100.0, At.Y * 100.0, FMath::Max(DBRealm::SampleHeight(At.X, At.Y), 0.0) * 100.0 + 300.0), 0.f);
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Paradise gate (%s): %s"), Kind == EDBParadiseGate::Home ? TEXT("home") : TEXT("up"), *GetNameSafe(Pawn));
}

void ADBParadiseGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		const APlayerController* Local = GetWorld()->GetFirstPlayerController();
		if (Local && Local->PlayerCameraManager)
		{
			const FVector ToCamera = Local->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
			Label->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
		}
	}
	if (!HasAuthority() || Kind != EDBParadiseGate::ToParadise)
	{
		return;
	}
	UpdateTimer -= DeltaSeconds;
	if (UpdateTimer > 0.f)
	{
		return;
	}
	UpdateTimer = 1.f;
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	const UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr;
	const bool bOpen = WorldState && WorldState->GetRulesState().StoryFlags.count("Story.DemonKingDefeated") > 0;
	if (bOpen != bActive)
	{
		bActive = bOpen;
		OnRep_Gate();
		if (bActive)
		{
			for (TActorIterator<ADBPlayerController> It(GetWorld()); It; ++It)
			{
				It->ClientShowNotification(LOCTEXT("GateOpens", "Am Thron oeffnet sich eine Pforte aus Licht ..."));
			}
			UE_LOG(LogDarkBlood, Display, TEXT("Paradise gate opens at the throne"));
		}
	}
}

// ---- Island ----------------------------------------------------------------------------------------------------

ADBParadiseIsland::ADBParadiseIsland()
{
	bReplicates = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	ShrineLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ShrineLight"));
	ShrineLight->SetupAttachment(Root);
	ShrineLight->SetRelativeLocation(FVector(IslandRadius * 0.45f, 0.f, 500.f));
	ShrineLight->SetIntensity(9000.f);
	ShrineLight->SetAttenuationRadius(2500.f);
	ShrineLight->SetLightColor(FLinearColor(1.f, 0.9f, 0.7f));
	ShrineLight->SetCastShadows(false);
}

void ADBParadiseIsland::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ADBParadiseIsland::Build()
{
	UStaticMesh* Cylinder = ParadiseMesh(TEXT("Cylinder"));
	UStaticMesh* Cone = ParadiseMesh(TEXT("Cone"));
	if (!Cylinder || !Cone)
	{
		return;
	}
	using M = EDBArtMaterial;
	const float Size = IslandRadius * 2.f / 100.f;
	// The island: a meadow on a rock that tapers into the clouds; three small rocks drift round it.
	AddStaticPart(this, Root, Parts, Cylinder, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -200.f), FVector(Size, Size, 4.f)), UDBArtMaterialSubsystem::Get(M::GroundMeadow));
	AddStaticPart(this, Root, Parts, Cone, FTransform(FRotator(180.f, 0.f, 0.f), FVector(0.f, 0.f, -3400.f), FVector(Size * 0.98f, Size * 0.98f, 60.f)),
		UDBArtMaterialSubsystem::Get(M::TerrainCliff));
	const FVector Drift[] = {FVector(9000.f, 5000.f, -1500.f), FVector(-7000.f, 8000.f, 600.f), FVector(4000.f, -9500.f, -900.f)};
	for (const FVector& At : Drift)
	{
		AddStaticPart(this, Root, Parts, Cylinder, FTransform(FRotator::ZeroRotator, At, FVector(22.f, 22.f, 2.f)), UDBArtMaterialSubsystem::Get(M::GroundMeadow));
		AddStaticPart(this, Root, Parts, Cone, FTransform(FRotator(180.f, 0.f, 0.f), At + FVector(0.f, 0.f, -1300.f), FVector(21.f, 21.f, 25.f)),
			UDBArtMaterialSubsystem::Get(M::TerrainCliff));
	}
	// A still pond, a stone path from the gate to the shrine.
	AddStaticPart(this, Root, Parts, Cylinder, FTransform(FRotator::ZeroRotator, FVector(500.f, -2600.f, -45.f), FVector(22.f, 16.f, 1.f)), UDBArtMaterialSubsystem::Get(M::Water));
	for (int32 Step = 0; Step < 10; ++Step)
	{
		AddStaticPart(this, Root, Parts, Cylinder, FTransform(FRotator::ZeroRotator, FVector(-IslandRadius * 0.7f + Step * 520.f, 0.f, 2.f), FVector(2.2f, 2.2f, 0.06f)),
			UDBArtMaterialSubsystem::Get(M::StoneTemple));
	}
	// Cherry trees in bloom round the edge, a pagoda and a shrine, lanterns along the path.
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const float Angle = 40.f * Index + 15.f;
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(Angle, 180.f)) < 25.f)
		{
			continue; // keep the gate free
		}
		AddModel(this, Root, Parts, Index % 3 == 0 ? TEXT("maple_red") : TEXT("cherry_tree"), Index % 3 == 0 ? 900.f : 1000.f,
			FRotator(0.f, Angle, 0.f).Vector() * (IslandRadius * 0.78f), Angle + 90.f);
	}
	AddModel(this, Root, Parts, TEXT("pagoda"), 2600.f, FVector(-500.f, 2900.f, 0.f), -90.f);
	AddModel(this, Root, Parts, TEXT("asian_shrine"), 800.f, FVector(IslandRadius * 0.62f, 0.f, 0.f), 180.f);
	for (int32 Step = 0; Step < 4; ++Step)
	{
		for (const float Side : {-1.f, 1.f})
		{
			AddModel(this, Root, Parts, TEXT("lantern_stone"), 200.f, FVector(-IslandRadius * 0.55f + Step * 1300.f, Side * 380.f, 0.f), Side > 0.f ? -90.f : 90.f);
		}
	}
}

// ---- Shrine of Peace -------------------------------------------------------------------------------------------

FText ADBPeaceShrine::GetInteractionText() const
{
	return LOCTEXT("PeaceRest", "Am Schrein des Friedens verweilen");
}

void ADBPeaceShrine::Interact(APlayerController* User)
{
	Super::Interact(User);
	ADBPlayerState* Player = User ? User->GetPlayerState<ADBPlayerState>() : nullptr;
	if (!HasAuthority() || !Player)
	{
		return;
	}
	if (UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this))
	{
		Quests->ReportEvent(EDBObjectiveKind::Interact, TEXT("PeaceShrine"), 1, Player);
	}
	// The end of the story for everyone in the Paradise.
	for (TActorIterator<ADBPlayerController> It(GetWorld()); It; ++It)
	{
		const APawn* Pawn = It->GetPawn();
		if (Pawn && DBParadise::IsInParadise(Pawn->GetActorLocation()))
		{
			It->ClientShowFinale();
		}
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Finale: %s rests at the Shrine of Peace"), *Player->GetPlayerName());
}

#undef LOCTEXT_NAMESPACE
