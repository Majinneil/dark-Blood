#include "World/DBTheEnd.h"

#include "Art/DBArtMaterials.h"
#include "Art/DBModelLibrary.h"
#include "Boss/DBBossDefinition.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DarkBlood.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Quest/DBQuestComponent.h"
#include "Quest/DBQuestSubsystem.h"
#include "World/DBRealmLayout.h"
#include "World/DBRealmVegetation.h"
#include "World/DBWorldStateComponent.h"

#include "DarkBloodRules/WorldState.h"

#define LOCTEXT_NAMESPACE "DarkBloodTheEnd"

namespace
{
	const FName TheEndId(TEXT("TheEnd"));
	const FName QuestSeal(TEXT("MQ10_TheEndSeal"));
	const FName QuestGuardians(TEXT("MQ11_ThroneGuardians"));
	const FName QuestThrone(TEXT("MQ12_DemonKing"));
	const FName QuestParadise(TEXT("MQ13_Paradise"));

	const FDBRealmRegion* FindTheEnd()
	{
		return DBRealm::GetRegions().FindByPredicate([](const FDBRealmRegion& Region) { return Region.RegionId == TheEndId; });
	}

	bool IsInRegion(const FVector& Location, FName RegionId)
	{
		const int32 Index = DBRealm::FindRegionIndex(Location.X / 100.0, Location.Y / 100.0);
		return DBRealm::GetRegions().IsValidIndex(Index) && DBRealm::GetRegions()[Index].RegionId == RegionId;
	}

	/** Toward the realm's heart from DAS ENDE (unit, meters space). */
	FVector2D TowardRealm()
	{
		const FDBRealmRegion* End = FindTheEnd();
		return End ? (-End->Center).GetSafeNormal() : FVector2D(-1.0, 0.0);
	}

	TArray<FVector2D> ThroneArenas()
	{
		TArray<FVector2D> Arenas;
		for (const UDBBossDefinition* Boss : DBBosses::GetAll())
		{
			const FVector Arena = Boss->RegionId == TheEndId ? DBBosses::GetArenaLocation(*Boss) : FVector::ZeroVector;
			if (!Arena.IsZero())
			{
				Arenas.Add(FVector2D(Arena) / 100.0);
			}
		}
		return Arenas;
	}

	UDBWorldStateComponent* GetEndWorldState(const UWorld* World)
	{
		const ADBGameState* GameState = World ? World->GetGameState<ADBGameState>() : nullptr;
		return GameState ? GameState->GetWorldState() : nullptr;
	}

	UStaticMesh* EndEngineMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	void FaceLocalCamera(const UWorld* World, UTextRenderComponent* Label)
	{
		const APlayerController* Local = World ? World->GetFirstPlayerController() : nullptr;
		if (Local && Local->PlayerCameraManager && Label)
		{
			const FVector ToCamera = Local->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
			Label->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
		}
	}
}

// ---- Placement -------------------------------------------------------------------------------------------------

FVector DBTheEnd::GetGateLocation()
{
	static FVector Cache = FVector::ZeroVector;
	if (!Cache.IsZero())
	{
		return Cache;
	}
	const FDBRealmRegion* End = FindTheEnd();
	if (!End)
	{
		return FVector::ZeroVector;
	}
	// On the realm side of DAS ENDE, inside it, clear of the throne arenas.
	for (const double Reach : {0.85, 0.7, 0.55, 0.4})
	{
		Cache = DBBosses::FindOpenGround(End->Center + TowardRealm() * End->Radius * Reach, 26.0, ThroneArenas(), 260.0, TEXT("Gate of the End"));
		if (IsInRegion(Cache, TheEndId))
		{
			break;
		}
	}
	return Cache;
}

FVector DBTheEnd::GetBastionLocation()
{
	static FVector Cache = FVector::ZeroVector;
	if (!Cache.IsZero())
	{
		return Cache;
	}
	const FVector2D Gate = FVector2D(GetGateLocation()) / 100.0;
	// In front of the gate, outside DAS ENDE.
	for (const double Distance : {160.0, 230.0, 320.0, 450.0})
	{
		Cache = DBBosses::FindOpenGround(Gate + TowardRealm() * Distance, 16.0, {Gate}, 110.0, TEXT("Last Bastion"));
		if (!IsInRegion(Cache, TheEndId))
		{
			break;
		}
	}
	return Cache;
}

float DBTheEnd::GetGateYaw()
{
	const FVector2D Into = -TowardRealm();
	return FMath::RadiansToDegrees(FMath::Atan2(Into.Y, Into.X));
}

FName DBTheEnd::GetBastionRespawnId()
{
	return FName(TEXT("Bastion_TheEnd"));
}

void DBTheEnd::SpawnLandmarks(UWorld* World)
{
	if (!World || ADBEndGate::Find(World) || GetGateLocation().IsZero())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ADBEndGate>(ADBEndGate::StaticClass(), GetGateLocation(), FRotator(0.f, GetGateYaw(), 0.f), Params);
	const FVector Bastion = GetBastionLocation();
	World->SpawnActor<ADBBastionShrine>(ADBBastionShrine::StaticClass(), Bastion, FRotator(0.f, GetGateYaw(), 0.f), Params);
	// The respawn point of the bastion (players rested there come back here).
	if (APlayerStart* Start = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Bastion + FVector(-500.f, 0.f, 120.f), FRotator(0.f, GetGateYaw(), 0.f), Params))
	{
		Start->PlayerStartTag = GetBastionRespawnId();
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DAS ENDE: gate at (%.0f, %.0f) m, bastion at (%.0f, %.0f) m"), GetGateLocation().X / 100.0, GetGateLocation().Y / 100.0,
		Bastion.X / 100.0, Bastion.Y / 100.0);
}

// ---- Gate ------------------------------------------------------------------------------------------------------

ADBEndGate::ADBEndGate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 1650.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(110.f);
}

void ADBEndGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBEndGate, bOpen);
	DOREPLIFETIME(ADBEndGate, OuterVassalsDefeated);
}

ADBEndGate* ADBEndGate::Find(const UWorld* World)
{
	TActorIterator<ADBEndGate> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

void ADBEndGate::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Gate();
}

void ADBEndGate::Build()
{
	UStaticMesh* Cube = EndEngineMesh(TEXT("Cube"));
	if (bBuilt || !Cube)
	{
		return;
	}
	bBuilt = true;
	auto AddPart = [this](UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Part->SetMobility(EComponentMobility::Static);
		Part->SetupAttachment(Root);
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
		Parts.Add(Part);
		return Part;
	};
	auto Model = [&AddPart](const TCHAR* Key, float Height, const FTransform& Placement, UMaterialInterface* Material)
	{
		for (const DBModels::FPlacedPart& Piece : DBModels::GetNormalizedParts(Key, Height))
		{
			AddPart(Piece.Mesh, Piece.Local * Placement, Material);
		}
	};
	UMaterialInterface* Corrupted = UDBArtMaterialSubsystem::Get(EDBArtMaterial::StoneCorrupted);
	UMaterialInterface* DarkStone = UDBArtMaterialSubsystem::Get(EDBArtMaterial::DarkBloodStone);
	// Three great torii one behind the other (local +X looks into DAS ENDE); the middle one carries the seal.
	for (int32 Index = -1; Index <= 1; ++Index)
	{
		Model(TEXT("torii_game"), Index == 0 ? 1300.f : 1000.f, FTransform(FRotator::ZeroRotator, FVector(Index * 900.f, 0.f, 0.f)), Index == 0 ? DarkStone : Corrupted);
	}
	// Guardian lanterns along the way through.
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const float X = -1300.f + Index * 520.f;
		for (const float Side : {-1.f, 1.f})
		{
			Model(TEXT("lantern_stone"), 260.f, FTransform(FRotator(0.f, Side > 0.f ? -90.f : 90.f, 0.f), FVector(X, Side * 900.f, 0.f)), Corrupted);
		}
	}
	// The blood seal across the middle gate.
	UMaterialInterface* SealMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Master/M_DB_BloodBarrier.M_DB_BloodBarrier"),
		nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (SealMaterial)
	{
		UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(SealMaterial, this);
		Tinted->SetVectorParameterValue(TEXT("BarrierColor"), FLinearColor(1.f, 0.02f, 0.03f));
		Tinted->SetScalarParameterValue(TEXT("Intensity"), 5.f);
		SealMaterial = Tinted;
	}
	Seal = AddPart(Cube, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 430.f), FVector(0.1f, 6.8f, 8.6f)),
		SealMaterial ? SealMaterial : UDBArtMaterialSubsystem::Get(EDBArtMaterial::BloodRiver));
	Seal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Seal->SetCastShadow(false);

	// The Path of Shame: corrupted torii from the gate to the throne, clear of the arenas.
	const FVector Throne = [&]()
	{
		for (const UDBBossDefinition* Boss : DBBosses::GetAll())
		{
			if (Boss->Rank == EDBBossRank::DemonKing)
			{
				return DBBosses::GetArenaLocation(*Boss);
			}
		}
		return FVector::ZeroVector;
	}();
	if (!Throne.IsZero())
	{
		const TArray<FVector2D> Arenas = ThroneArenas();
		const FVector2D From = FVector2D(GetActorLocation()) / 100.0;
		const FVector2D To = FVector2D(Throne) / 100.0;
		const double Length = FVector2D::Distance(From, To);
		const FVector2D Along = (To - From).GetSafeNormal();
		const float PathYaw = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X));
		for (double Step = 120.0; Step < Length - 60.0; Step += 130.0)
		{
			const FVector2D Spot = From + Along * Step;
			if (Arenas.ContainsByPredicate([&Spot](const FVector2D& Arena) { return FVector2D::Distance(Arena, Spot) < 60.0; }))
			{
				continue;
			}
			const FVector World(Spot.X * 100.0, Spot.Y * 100.0, DBRealm::SampleHeight(Spot.X, Spot.Y) * 100.0 - 30.0);
			const FTransform Placement(FRotator(0.f, PathYaw - GetActorRotation().Yaw, 0.f), GetActorTransform().InverseTransformPosition(World));
			Model(TEXT("torii_game"), 750.f, Placement, Corrupted);
		}
	}
}

void ADBEndGate::OnRep_Gate()
{
	Build();
	if (Seal)
	{
		Seal->SetVisibility(!bOpen);
	}
	Label->SetText(bOpen ? LOCTEXT("GateOpen", "Tor des Endes")
						 : FText::Format(LOCTEXT("GateSealed", "Tor des Endes - versiegelt ({0}/14 Vasallen)"), FText::AsNumber(OuterVassalsDefeated)));
	Label->SetTextRenderColor(bOpen ? FColor(230, 200, 200) : FColor(255, 40, 40));
}

void ADBEndGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		FaceLocalCamera(GetWorld(), Label);
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
	UpdateTimer = 2.f;
	const UDBWorldStateComponent* WorldState = GetEndWorldState(GetWorld());
	if (!WorldState)
	{
		return;
	}
	const DarkBlood::Rules::FWorldState& State = WorldState->GetRulesState();
	int32 Outer = 0;
	for (const DarkBlood::Rules::FRegionState& Region : State.GetRegions())
	{
		Outer += Region.Kind == DarkBlood::Rules::ERegionKind::VassalRegion && Region.bVassalDefeated ? 1 : 0;
	}
	const bool bNowOpen = State.IsFinalRegionOpen();
	if (bNowOpen != bOpen || Outer != OuterVassalsDefeated)
	{
		const bool bBroke = bKnownState && bNowOpen && !bOpen;
		bOpen = bNowOpen;
		OuterVassalsDefeated = Outer;
		OnRep_Gate();
		if (bBroke)
		{
			// Everyone hears the seal break.
			for (TActorIterator<ADBPlayerController> It(GetWorld()); It; ++It)
			{
				It->ClientShowNotification(LOCTEXT("SealBroken", "Das Siegel des Endes ist gebrochen! Der Weg zum Daemonenkoenig liegt offen."));
			}
			UE_LOG(LogDarkBlood, Display, TEXT("DAS ENDE: the seal breaks"));
		}
	}
	bKnownState = true;
	if (bOpen)
	{
		AdvanceQuests();
	}
}

void ADBEndGate::AdvanceQuests()
{
	const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
	UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this);
	const UDBQuestComponent* Log = GameState ? GameState->GetSharedQuests() : nullptr;
	if (!Quests || !Log || GameState->PlayerArray.Num() == 0)
	{
		return;
	}
	APlayerState* Anyone = GameState->PlayerArray[0];
	const FName Chain[] = {QuestSeal, QuestGuardians, QuestThrone, QuestParadise};
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Chain)); ++Index)
	{
		const EDBQuestStatus Status = Log->GetQuestStatus(Chain[Index]);
		if (Status == EDBQuestStatus::Completed)
		{
			continue;
		}
		if (Status == EDBQuestStatus::Inactive && (Index == 0 || Log->GetQuestStatus(Chain[Index - 1]) == EDBQuestStatus::Completed))
		{
			Quests->StartQuest(Chain[Index], Anyone);
		}
		break;
	}
}

// ---- Bastion ---------------------------------------------------------------------------------------------------

ADBBastionShrine::ADBBastionShrine()
{
	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(RootComponent);
	FireLight->SetRelativeLocation(FVector(0.f, 0.f, 260.f));
	FireLight->SetIntensity(7000.f);
	FireLight->SetAttenuationRadius(1500.f);
	FireLight->SetLightColor(FLinearColor(1.f, 0.62f, 0.3f));
	FireLight->SetCastShadows(false);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(-60.f, 0.f, 520.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(60.f);
	Label->SetText(LOCTEXT("Bastion", "Letzte Bastion"));
	PrimaryActorTick.bCanEverTick = true;
	Label->SetTextRenderColor(FColor(220, 225, 255));
}

FText ADBBastionShrine::GetInteractionText() const
{
	return LOCTEXT("BastionRest", "In der Letzten Bastion rasten (Ruhepunkt)");
}

void ADBBastionShrine::Interact(APlayerController* User)
{
	Super::Interact(User);
	ADBPlayerState* Player = User ? User->GetPlayerState<ADBPlayerState>() : nullptr;
	if (!HasAuthority() || !Player)
	{
		return;
	}
	Player->SetRespawnPointId(DBTheEnd::GetBastionRespawnId());
	if (UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this))
	{
		Quests->ReportEvent(EDBObjectiveKind::Interact, DBTheEnd::GetBastionRespawnId(), 1, Player);
	}
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(User))
	{
		Controller->ClientShowNotification(LOCTEXT("BastionRespawn", "Die Letzte Bastion ist jetzt dein Ruhepunkt."));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Bastion: %s rests, respawn point set"), *Player->GetPlayerName());
}

void ADBBastionShrine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		FaceLocalCamera(GetWorld(), Label);
	}
}

void ADBBastionShrine::BeginPlay()
{
	Super::BeginPlay();
	BuildCamp();
}

void ADBBastionShrine::BuildCamp()
{
	UStaticMesh* Cube = EndEngineMesh(TEXT("Cube"));
	UStaticMesh* Cylinder = EndEngineMesh(TEXT("Cylinder"));
	if (!Cube || !Cylinder)
	{
		return;
	}
	auto AddPart = [this](UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Part->SetupAttachment(RootComponent);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeTransform(Transform);
		Part->SetMaterial(0, Material);
		Part->SetCanEverAffectNavigation(false);
		Part->RegisterComponent();
		CampParts.Add(Part);
	};
	using M = EDBArtMaterial;
	// Tents of the last defenders, a palisade toward DAS ENDE, banners of the capital.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Yaw = 135.f + 30.f * Index;
		const FVector At = FRotator(0.f, Yaw, 0.f).Vector() * 750.f;
		AddPart(Cube, FTransform(FRotator(0.f, Yaw, 45.f), At, FVector(3.2f, 2.6f, 2.6f)), UDBArtMaterialSubsystem::Get(M::FabricLinen));
	}
	for (int32 Index = 0; Index < 11; ++Index)
	{
		const float Y = -1000.f + Index * 200.f;
		AddPart(Cylinder, FTransform(FRotator(0.f, 0.f, 0.f), FVector(900.f, Y, 150.f), FVector(0.3f, 0.3f, 3.f)), UDBArtMaterialSubsystem::Get(M::WoodDark));
	}
	for (const float Y : {-500.f, 500.f})
	{
		AddPart(Cylinder, FTransform(FRotator::ZeroRotator, FVector(-200.f, Y, 350.f), FVector(0.14f, 0.14f, 7.f)), UDBArtMaterialSubsystem::Get(M::WoodLacquerBlack));
		AddPart(Cube, FTransform(FRotator(0.f, 90.f, 0.f), FVector(-200.f, Y + 60.f, 520.f), FVector(1.f, 0.04f, 2.2f)), UDBArtMaterialSubsystem::Get(M::FabricIndigo));
	}
	AddPart(Cylinder, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 3.f), FVector(4.f, 4.f, 0.06f)), UDBArtMaterialSubsystem::Get(M::GroundCourtyard));
}

#undef LOCTEXT_NAMESPACE
