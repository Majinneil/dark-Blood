#include "World/DBEchoHall.h"

#include "Art/DBArtBatcher.h"
#include "Art/DBArtMaterials.h"
#include "Art/DBModelLibrary.h"
#include "Boss/DBBoss.h"
#include "Boss/DBBossDefinition.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/DBHorse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DarkBlood.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "World/DBDungeon.h"
#include "World/DBRealmLayout.h"
#include "World/DBWorldStateComponent.h"

#include "DarkBloodRules/Endgame.h"
#include "World/DBTeleport.h"

#define LOCTEXT_NAMESPACE "DarkBloodEchoHall"

namespace R = DarkBlood::Rules;

namespace
{
	/** The stones stand on a ring, leaving the west open for the way out. */
	constexpr float StoneRing = 3500.f;
	constexpr float StoneArc = 300.f;

	const UDBWorldStateComponent* GetHallWorldState(const UWorld* World)
	{
		const ADBGameState* GameState = World ? World->GetGameState<ADBGameState>() : nullptr;
		return GameState ? GameState->GetWorldState() : nullptr;
	}

	/** Vassals in the order they are fought, the demon king last. */
	TArray<const UDBBossDefinition*> GetRememberable()
	{
		TArray<const UDBBossDefinition*> Bosses;
		for (const UDBBossDefinition* Boss : DBBosses::GetAll())
		{
			if (Boss->Rank == EDBBossRank::Vassal || Boss->Rank == EDBBossRank::DemonKing)
			{
				Bosses.Add(Boss);
			}
		}
		Bosses.Sort([](const UDBBossDefinition& A, const UDBBossDefinition& B)
		{
			const bool AKing = A.Rank == EDBBossRank::DemonKing;
			const bool BKing = B.Rank == EDBBossRank::DemonKing;
			return AKing != BKing ? BKing : A.Order < B.Order;
		});
		return Bosses;
	}

	FVector GetArrival()
	{
		return DBEchoHall::GetCenter() + FVector(-DBEchoHall::Radius + 900.f, 0.f, 120.f);
	}

	UMaterialInterface* GetGlowMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Master/M_DB_BloodBarrier.M_DB_BloodBarrier"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

// ---- Placement -------------------------------------------------------------------------------------------------

FVector DBEchoHall::GetCenter()
{
	// Over the open sea west of the realm, north of the dungeon interiors, 30 m above the water.
	return FVector(-(DBRealm::HalfSize + 3200.0) * 100.0, 120000.0, 3000.0);
}

void DBEchoHall::SpawnHall(UWorld* World)
{
	if (!World || ADBEchoHall::Find(World))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ADBEchoHall>(ADBEchoHall::StaticClass(), GetCenter(), FRotator::ZeroRotator, Params);
	UE_LOG(LogDarkBlood, Display, TEXT("Hall of Echoes at (%.0f, %.0f, %.0f) m"), GetCenter().X / 100.0, GetCenter().Y / 100.0, GetCenter().Z / 100.0);
}

bool DBEchoHall::Enter(APlayerController* User)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	if (!Pawn || !Pawn->HasAuthority() || !ADBEchoHall::Find(Pawn->GetWorld()))
	{
		return false;
	}
	const UDBWorldStateComponent* WorldState = GetHallWorldState(Pawn->GetWorld());
	ADBPlayerController* Controller = Cast<ADBPlayerController>(User);
	if (!WorldState || !WorldState->IsEndgameOpen())
	{
		if (Controller)
		{
			Controller->ClientShowNotification(LOCTEXT("HallSealed", "Die Halle der Echos schweigt. Erst wenn der Daemonenkoenig faellt, erwachen ihre Steine."));
		}
		return false;
	}
	if (ADBHorse* Horse = ADBHorse::FindRiddenBy(Pawn))
	{
		Horse->Interact(User); // horses wait outside
	}
	DBTeleport::MovePawn(Pawn, GetArrival(), 0.f);
	if (Controller)
	{
		Controller->ClientShowNotification(LOCTEXT("HallEnter", "Halle der Echos: Beruehre den Stein eines besiegten Vasallen, um sein Echo herauszufordern."));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Hall of Echoes: %s enters"), *GetNameSafe(Pawn));
	return true;
}

// ---- Hall ------------------------------------------------------------------------------------------------------

ADBEchoHall::ADBEchoHall()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

ADBEchoHall* ADBEchoHall::Find(const UWorld* World)
{
	for (TActorIterator<ADBEchoHall> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADBEchoHall::BeginPlay()
{
	Super::BeginPlay();
	Build();
	if (HasAuthority())
	{
		SpawnFixtures();
	}
}

void ADBEchoHall::Build()
{
	using M = EDBArtMaterial;
	FDBArtBatcher Batcher(*this, *Root, Pieces);
	Batcher.SetCollision(true);
	const float Diameter = DBEchoHall::Radius * 2.f + 600.f;
	// A disc of temple stone on a rock that tapers into the sea mist. The fighting ground is set in dark old stone,
	// bounded by a thin seam of glowing dark blood.
	Batcher.Cylinder(M::StoneTemple, FVector(0.f, 0.f, -60.f), Diameter, 120.f);
	Batcher.Cylinder(M::DarkBloodVeins, FVector(0.f, 0.f, 2.f), StoneRing * 2.f - 700.f, 6.f);
	Batcher.Cylinder(M::StoneRuin, FVector(0.f, 0.f, 5.f), StoneRing * 2.f - 1000.f, 8.f);
	Batcher.Cylinder(M::StoneMountain, FVector(0.f, 0.f, 10.f), 900.f, 10.f);
	Batcher.Cone(M::TerrainCliff, FVector(0.f, 0.f, -1700.f), Diameter * 0.97f, 3200.f, FRotator(180.f, 0.f, 0.f));
	// The colonnade: weathered pillars round the edge (open to the west), some broken long ago, each on a plinth and
	// under a cap stone.
	const int32 Pillars = 24;
	for (int32 Index = 0; Index < Pillars; ++Index)
	{
		const float Angle = 360.f * Index / Pillars;
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(Angle, 180.f)) < 16.f)
		{
			continue;
		}
		const FVector At = FRotator(0.f, Angle, 0.f).Vector() * (DBEchoHall::Radius + 150.f);
		const bool bBroken = Index % 5 == 2;
		const float Height = bBroken ? 520.f + (Index % 3) * 120.f : 1600.f;
		Batcher.Box(M::StoneRuin, At + FVector(0.f, 0.f, 45.f), FVector(300.f, 300.f, 90.f), FRotator(0.f, Angle, 0.f));
		Batcher.Cylinder(M::StoneMountain, At + FVector(0.f, 0.f, 90.f + Height * 0.5f), 170.f, Height);
		if (!bBroken)
		{
			Batcher.Box(M::StoneRuin, At + FVector(0.f, 0.f, 90.f + Height + 50.f), FVector(260.f, 260.f, 100.f), FRotator(0.f, Angle, 0.f));
		}
		else
		{
			// The fallen drum lies at its foot.
			Batcher.Cylinder(M::StoneMountain, At - FRotator(0.f, Angle, 0.f).Vector() * 320.f + FVector(0.f, 0.f, 85.f), 170.f, 420.f,
				FRotator(90.f, Angle + 70.f, 0.f));
		}
	}
	// Stone lanterns along the way in.
	Batcher.SetCollision(false);
	for (int32 Step = 0; Step < 3; ++Step)
	{
		for (const float Side : {-1.f, 1.f})
		{
			const FTransform Placement(FRotator(0.f, Side > 0.f ? -90.f : 90.f, 0.f), FVector(-DBEchoHall::Radius + 700.f + Step * 700.f, Side * 450.f, 0.f));
			for (const DBModels::FPlacedPart& Piece : DBModels::GetNormalizedParts(TEXT("lantern_stone"), 220.f))
			{
				Batcher.Mesh(Piece.Mesh, nullptr, Piece.Local * Placement);
			}
		}
	}
	// Cold spirit light in the middle, warm lantern light at the way in.
	auto AddLight = [this](const FVector& At, float Intensity, float Radius, const FLinearColor& Color)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(At);
		Light->SetIntensity(Intensity);
		Light->SetAttenuationRadius(Radius);
		Light->SetLightColor(Color);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		Lights.Add(Light);
	};
	AddLight(FVector(0.f, 0.f, 1600.f), 60000.f, 6500.f, FLinearColor(0.55f, 0.72f, 1.f));
	AddLight(FVector(-DBEchoHall::Radius + 1200.f, 0.f, 500.f), 9000.f, 2200.f, FLinearColor(1.f, 0.62f, 0.32f));
	UE_LOG(LogDarkBlood, Log, TEXT("Hall of Echoes built: %d instances, %d lights"), Batcher.GetInstanceCount(), Lights.Num());
}

void ADBEchoHall::SpawnFixtures()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TArray<const UDBBossDefinition*> Bosses = GetRememberable();
	for (int32 Index = 0; Index < Bosses.Num(); ++Index)
	{
		// From the north-west round the east to the south-west; the demon king last, beside the way out.
		const float Angle = 180.f - (360.f - StoneArc) * 0.5f - StoneArc * Index / FMath::Max(1, Bosses.Num() - 1);
		const FVector At = GetActorLocation() + FRotator(0.f, Angle, 0.f).Vector() * StoneRing;
		if (ADBEchoStone* Stone = GetWorld()->SpawnActor<ADBEchoStone>(ADBEchoStone::StaticClass(), At, FRotator(0.f, Angle + 180.f, 0.f), Params))
		{
			Stone->Setup(Bosses[Index]->BossId);
		}
	}
	const int32 Site = DBDungeon::GetSites().IndexOfByPredicate([](const FDBDungeonSite& Candidate) { return Candidate.bEchoHall; });
	if (ADBDungeonPortal* Exit = GetWorld()->SpawnActor<ADBDungeonPortal>(ADBDungeonPortal::StaticClass(), GetActorLocation() + FVector(-DBEchoHall::Radius + 250.f, 0.f, 0.f),
			FRotator(0.f, 0.f, 0.f), Params))
	{
		Exit->Setup(Site, true);
	}
}

TArray<APawn*> ADBEchoHall::GetPlayersInside() const
{
	TArray<APawn*> Players;
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
			const FVector Local = Pawn ? Pawn->GetActorLocation() - GetActorLocation() : FVector(1e9);
			if (Pawn && Local.Size2D() < DBEchoHall::Radius + 400.f && FMath::Abs(Local.Z) < 3000.f)
			{
				Players.Add(Pawn);
			}
		}
	}
	return Players;
}

bool ADBEchoHall::SummonEcho(FName BossId, APlayerController* User)
{
	ADBPlayerController* Controller = Cast<ADBPlayerController>(User);
	const UDBBossDefinition* Definition = DBBosses::Find(BossId);
	const UDBWorldStateComponent* WorldState = GetHallWorldState(GetWorld());
	if (!HasAuthority() || !Definition || !WorldState)
	{
		return false;
	}
	if (Echo.IsValid() && !Echo->IsDead())
	{
		if (Controller)
		{
			Controller->ClientShowNotification(FText::Format(LOCTEXT("EchoBusy", "Das Echo von {0} kaempft noch."), Echo->GetDefinition()->DisplayName));
		}
		return false;
	}
	if (!WorldState->IsBossRemembered(BossId))
	{
		return false;
	}
	const TArray<APawn*> Players = GetPlayersInside();
	const int32 NextRank = WorldState->GetEchoRank(BossId) + 1;
	const R::FEndgameScale Scale = R::GetEchoScale(WorldState->GetCycle(), NextRank);
	const FDBEndgameScale Endgame{Scale.EnemyHealth, Scale.EnemyDamage, Scale.Experience, Scale.RarityBonus, Scale.EnemyLevelBonus};
	const APawn* Caller = User ? User->GetPawn() : nullptr;
	const FVector Facing = Caller ? (Caller->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : FVector::ForwardVector;
	Echo = ADBBossCharacter::SpawnBoss(GetWorld(), Definition, GetActorLocation() + FVector(0.f, 0.f, 250.f), Facing.Rotation(), FMath::Max(1, Players.Num()), 1.f, nullptr,
		&Endgame, true);
	if (!Echo.IsValid())
	{
		return false;
	}
	EmptySeconds = 0.f;
	for (const APawn* Player : Players)
	{
		if (ADBPlayerController* PC = Cast<ADBPlayerController>(Player->GetController()))
		{
			PC->ClientShowNotification(FText::Format(LOCTEXT("EchoRises", "Das Echo von {0} erhebt sich (Echo-Rang {1})."), Definition->DisplayName, FText::AsNumber(NextRank)));
		}
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Hall of Echoes: echo of %s rises (rank %d, cycle %d, health x%.2f, damage x%.2f, %d players)"), *BossId.ToString(), NextRank,
		WorldState->GetCycle(), Scale.EnemyHealth, Scale.EnemyDamage, Players.Num());
	return true;
}

void ADBEchoHall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !Echo.IsValid())
	{
		return;
	}
	UpdateTimer -= DeltaSeconds;
	if (UpdateTimer > 0.f)
	{
		return;
	}
	UpdateTimer = 0.5f;
	if (Echo->IsDead())
	{
		Echo.Reset();
		return;
	}
	// Everyone left or fell: the echo fades and waits, unharmed, for the next challenge.
	EmptySeconds = GetPlayersInside().Num() > 0 ? 0.f : EmptySeconds + 0.5f;
	if (EmptySeconds > 8.f)
	{
		for (TActorIterator<ADBBossTelegraph> It(GetWorld()); It; ++It)
		{
			if (It->GetBoss() == Echo.Get())
			{
				It->Destroy();
			}
		}
		UE_LOG(LogDarkBlood, Display, TEXT("Hall of Echoes: the echo of %s fades"), *Echo->GetBossId().ToString());
		Echo->Destroy();
		Echo.Reset();
	}
}

// ---- Stone -----------------------------------------------------------------------------------------------------

ADBEchoStone::ADBEchoStone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// A tall memorial slab and the echo's flame floating above it.
	Pillar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pillar"));
	Pillar->SetupAttachment(Root);
	Pillar->SetStaticMesh(Cube.Object);
	Pillar->SetRelativeLocation(FVector(0.f, 0.f, 360.f));
	Pillar->SetRelativeScale3D(FVector(0.8f, 1.9f, 7.2f));
	Pillar->SetCanEverAffectNavigation(false);
	Flame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flame"));
	Flame->SetupAttachment(Root);
	Flame->SetStaticMesh(Sphere.Object);
	Flame->SetRelativeLocation(FVector(0.f, 0.f, 870.f));
	Flame->SetRelativeScale3D(FVector(0.9f));
	Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flame->SetCastShadow(false);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 1060.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(60.f);
}

void ADBEchoStone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBEchoStone, BossId);
	DOREPLIFETIME(ADBEchoStone, bRemembered);
	DOREPLIFETIME(ADBEchoStone, EchoRank);
}

void ADBEchoStone::Setup(FName InBossId)
{
	BossId = InBossId;
	OnRep_Stone();
}

void ADBEchoStone::OnRep_Stone()
{
	const UDBBossDefinition* Definition = DBBosses::Find(BossId);
	if (!Definition)
	{
		return;
	}
	Pillar->SetMaterial(0, UDBArtMaterialSubsystem::Get(bRemembered ? EDBArtMaterial::StoneTemple : EDBArtMaterial::StoneMountain));
	if (!FlameMaterial)
	{
		if (UMaterialInterface* Glow = GetGlowMaterial())
		{
			FlameMaterial = UMaterialInstanceDynamic::Create(Glow, this);
			Flame->SetMaterial(0, FlameMaterial);
		}
	}
	const FLinearColor Spirit = FMath::Lerp(Definition->Color, FLinearColor(0.45f, 0.7f, 1.f), 0.5f);
	if (FlameMaterial)
	{
		FlameMaterial->SetVectorParameterValue(TEXT("BarrierColor"), Spirit);
		FlameMaterial->SetScalarParameterValue(TEXT("Intensity"), 2.f + EchoRank * 0.4f);
		FlameMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.85f);
	}
	Flame->SetVisibility(bRemembered);
	Flame->SetRelativeScale3D(FVector(0.8f + FMath::Min(EchoRank, 10) * 0.06f));
	const FText Name = Definition->DisplayName;
	Label->SetText(!bRemembered ? FText::Format(LOCTEXT("StoneDark", "{0}\n(unbesiegt)"), Name)
				   : EchoRank > 0 ? FText::Format(LOCTEXT("StoneRank", "{0}\nEcho-Rang {1}"), Name, FText::AsNumber(EchoRank))
								  : FText::Format(LOCTEXT("StoneAwake", "{0}\nEcho wartet"), Name));
	Label->SetTextRenderColor((bRemembered ? Spirit : FLinearColor(0.35f, 0.35f, 0.38f)).ToFColor(true));
}

FText ADBEchoStone::GetInteractionText() const
{
	const UDBBossDefinition* Definition = DBBosses::Find(BossId);
	return FText::Format(LOCTEXT("ChallengeEcho", "Echo herausfordern: {0} (Rang {1})"), Definition ? Definition->DisplayName : FText::FromName(BossId),
		FText::AsNumber(EchoRank + 1));
}

void ADBEchoStone::Interact(APlayerController* User)
{
	if (!HasAuthority() || !bRemembered)
	{
		return;
	}
	if (ADBEchoHall* Hall = ADBEchoHall::Find(GetWorld()))
	{
		Hall->SummonEcho(BossId, User);
	}
}

void ADBEchoStone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The name faces the local camera (text renders are one-sided).
	if (GetNetMode() != NM_DedicatedServer)
	{
		const APlayerController* Local = GetWorld()->GetFirstPlayerController();
		if (Local && Local->PlayerCameraManager)
		{
			const FVector ToCamera = Local->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
			Label->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
		}
	}
	if (!HasAuthority())
	{
		return;
	}
	UpdateTimer -= DeltaSeconds;
	if (UpdateTimer > 0.f)
	{
		return;
	}
	UpdateTimer = 1.f;
	const UDBWorldStateComponent* WorldState = GetHallWorldState(GetWorld());
	const bool bNowRemembered = WorldState && WorldState->IsBossRemembered(BossId);
	const int32 NowRank = WorldState ? WorldState->GetEchoRank(BossId) : 0;
	if (bNowRemembered != bRemembered || NowRank != EchoRank)
	{
		bRemembered = bNowRemembered;
		EchoRank = NowRank;
		OnRep_Stone();
	}
}

#undef LOCTEXT_NAMESPACE
