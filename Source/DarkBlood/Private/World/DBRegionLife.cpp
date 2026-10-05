#include "World/DBRegionLife.h"

#include "Art/DBArtMaterials.h"
#include "Art/DBModelLibrary.h"
#include "Boss/DBBoss.h"
#include "Boss/DBBossDefinition.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/DBLesserDemon.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/DBRulesBridge.h"
#include "DarkBlood.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBRegionDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerState.h"
#include "World/DBRealmLayout.h"
#include "World/DBRealmVegetation.h"
#include "World/DBWorldStateComponent.h"

#include "DarkBloodRules/Region.h"
#include "DarkBloodRules/WorldState.h"

#define LOCTEXT_NAMESPACE "DarkBloodRegions"

namespace R = DarkBlood::Rules;

namespace
{
	/** Packs stay within this distance of a player; farther ones are removed (cm). */
	constexpr float PackKeepRadius = 16000.f;
	/** Packs within this distance of a player count against its budget (shared in co-op). */
	constexpr float PackShareRadius = 10000.f;
	constexpr double PackCooldownSeconds = 40.0;

	UDBWorldStateComponent* GetRegionWorldState(const UWorld* World)
	{
		const ADBGameState* GameState = World ? World->GetGameState<ADBGameState>() : nullptr;
		return GameState ? GameState->GetWorldState() : nullptr;
	}

	/** Not on water, in a settlement or in a boss arena. */
	bool IsFreeRegionGround(const FVector2D& Meters, FName RegionId)
	{
		if (DBRealm::SampleHeight(Meters.X, Meters.Y) < 1.5 || !DBRealm::IsInside(Meters.X, Meters.Y))
		{
			return false;
		}
		for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
		{
			if (FVector2D::Distance(Meters, Site.Center) < Site.Radius + 60.0)
			{
				return false;
			}
		}
		for (const UDBBossDefinition* Boss : DBBosses::GetAll())
		{
			const FVector Arena = Boss->RegionId == RegionId ? DBBosses::GetArenaLocation(*Boss) : FVector::ZeroVector;
			if (!Arena.IsZero() && FVector2D::Distance(Meters, FVector2D(Arena) / 100.0) < Boss->ArenaRadius / 100.0 + 25.0)
			{
				return false;
			}
		}
		const FVector Camp = DBRegions::GetCampLocation(RegionId);
		return Camp.IsZero() || FVector2D::Distance(Meters, FVector2D(Camp) / 100.0) > 45.0;
	}

	UStaticMesh* CampEngineMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}
}

// ---- Helpers ---------------------------------------------------------------------------------------------------

int32 DBRegions::GetRegionNumber(FName RegionId)
{
	const FString Id = RegionId.ToString();
	if (Id == TEXT("TheEnd"))
	{
		return 15;
	}
	if (Id.StartsWith(TEXT("Region")) && Id.Len() == 8)
	{
		const int32 Number = FCString::Atoi(*Id.RightChop(6));
		return Number >= 1 && Number <= R::NumVassalRegions ? Number : 0;
	}
	return 0;
}

FText DBRegions::GetDemonName(FName RegionId)
{
	static const TCHAR* Names[] = {TEXT("Blutdaemon"), TEXT("Frostdaemon"), TEXT("Schattendaemon"), TEXT("Donnerdaemon"), TEXT("Glutdaemon"),
		TEXT("Knochendaemon"), TEXT("Seuchendaemon"), TEXT("Trugdaemon"), TEXT("Bestiendaemon"), TEXT("Eisendaemon"), TEXT("Tiefendaemon"),
		TEXT("Sturmdaemon"), TEXT("Nachtdaemon"), TEXT("Leerendaemon"), TEXT("Blutmond-Daemon")};
	const int32 Number = GetRegionNumber(RegionId);
	return FText::FromString(Number > 0 ? Names[Number - 1] : TEXT("Daemon"));
}

FName DBRegions::GetDemonId(FName RegionId)
{
	return FName(*(TEXT("Demon_") + RegionId.ToString()));
}

FVector DBRegions::GetCampLocation(FName RegionId)
{
	const int32 Number = GetRegionNumber(RegionId);
	if (Number < 1 || Number > R::NumVassalRegions)
	{
		return FVector::ZeroVector;
	}
	static TMap<FName, FVector> Cache;
	if (const FVector* Known = Cache.Find(RegionId))
	{
		return *Known;
	}
	const FDBRealmRegion* Region = DBRealm::GetRegions().FindByPredicate([RegionId](const FDBRealmRegion& Candidate) { return Candidate.RegionId == RegionId; });
	const FVector2D Center = Region ? Region->Center : FVector2D::ZeroVector;
	// Across the region from the vassal's arena (the coast region faces west: inland).
	const FVector2D Offset = RegionId == FName(TEXT("Region11")) ? FVector2D(800.0, -350.0) : FVector2D(480.0, -420.0);
	TArray<FVector2D> Taken;
	for (const UDBBossDefinition* Boss : DBBosses::GetAll())
	{
		const FVector Arena = Boss->RegionId == RegionId ? DBBosses::GetArenaLocation(*Boss) : FVector::ZeroVector;
		if (!Arena.IsZero())
		{
			Taken.Add(FVector2D(Arena) / 100.0);
		}
	}
	// The camp must lie in its own region (it starts the region's quest and packs there): move toward the heart if not.
	FVector Location = FVector::ZeroVector;
	for (const double Pull : {1.0, 0.7, 0.45, 0.2, 0.0})
	{
		Location = DBBosses::FindOpenGround(Center + Offset * Pull, 16.0, Taken, 450.0, TEXT("Demon camp ") + RegionId.ToString());
		const int32 Index = DBRealm::FindRegionIndex(Location.X / 100.0, Location.Y / 100.0);
		if (DBRealm::GetRegions().IsValidIndex(Index) && DBRealm::GetRegions()[Index].RegionId == RegionId)
		{
			break;
		}
	}
	Cache.Add(RegionId, Location);
	return Location;
}

ADBEnemyCharacter* DBRegions::SpawnDemon(UWorld* World, FName RegionId, const FVector& Location, const FRotator& Rotation, uint32 Seed, bool bElite)
{
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(World);
	const UDBRegionDefinition* Region = Data ? Data->FindRegion(RegionId) : nullptr;
	if (!World || !Region)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.bDeferConstruction = true;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADBLesserDemon* Demon = World->SpawnActor<ADBLesserDemon>(ADBLesserDemon::StaticClass(), Location, Rotation, Params);
	if (!Demon)
	{
		return nullptr;
	}
	const int32 Level = R::GetRegionalDemonLevel(Region->RecommendedPowerMin, Region->RecommendedPowerMax, Seed, bElite);
	const FText Name = bElite ? FText::Format(LOCTEXT("Elite", "Elite-{0}"), GetDemonName(RegionId)) : GetDemonName(RegionId);
	Demon->ConfigureSpawn(Level, R::GetRegionalStatMultiplier(Level) * (bElite ? 1.6f : 1.f), Name, GetDemonId(RegionId));
	Demon->FinishSpawning(FTransform(Rotation, Location));
	return Demon;
}

// ---- Packs -----------------------------------------------------------------------------------------------------

UDBRegionLifeComponent::UDBRegionLifeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDBRegionLifeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	UpdateTimer -= DeltaTime;
	if (UpdateTimer <= 0.f)
	{
		UpdateTimer = 2.f;
		UpdatePacks();
	}
}

int32 UDBRegionLifeComponent::CountPacksNear(const FVector& Location, float Radius) const
{
	int32 Count = 0;
	for (const FPack& Pack : Packs)
	{
		Count += FVector::Dist2D(Pack.Center, Location) < Radius ? 1 : 0;
	}
	return Count;
}

int32 UDBRegionLifeComponent::CountLivingDemons() const
{
	int32 Count = 0;
	for (const FPack& Pack : Packs)
	{
		for (const TWeakObjectPtr<ADBEnemyCharacter>& Demon : Pack.Demons)
		{
			Count += Demon.IsValid() && !Demon->IsDead() ? 1 : 0;
		}
	}
	return Count;
}

void UDBRegionLifeComponent::UpdatePacks()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return;
	}
	TArray<FVector> PlayerLocations;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		if (const APawn* Pawn = Player ? Player->GetPawn() : nullptr)
		{
			PlayerLocations.Add(Pawn->GetActorLocation());
		}
	}
	// Drop packs that are dead or left behind (no player near): their demons vanish.
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = Packs.Num() - 1; Index >= 0; --Index)
	{
		FPack& Pack = Packs[Index];
		Pack.Demons.RemoveAll([](const TWeakObjectPtr<ADBEnemyCharacter>& Demon) { return !Demon.IsValid() || Demon->IsDead(); });
		const bool bNear = PlayerLocations.ContainsByPredicate([&Pack](const FVector& At) { return FVector::Dist2D(At, Pack.Center) < PackKeepRadius; });
		if (Pack.Demons.Num() == 0 || !bNear)
		{
			for (const TWeakObjectPtr<ADBEnemyCharacter>& Demon : Pack.Demons)
			{
				if (Demon.IsValid())
				{
					Demon->Destroy();
				}
			}
			Packs.RemoveAt(Index);
		}
	}
	for (APlayerState* Player : GameState->PlayerArray)
	{
		const double* Next = NextPackAt.Find(Player);
		if (!Next || Now >= *Next)
		{
			if (SpawnPackNear(Player, false))
			{
				NextPackAt.Add(Player, Now + PackCooldownSeconds);
			}
		}
	}
}

bool UDBRegionLifeComponent::SpawnPackNear(APlayerState* Player, bool bIgnoreBudget)
{
	const ADBPlayerState* DBPlayer = Cast<ADBPlayerState>(Player);
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const UDBWorldStateComponent* WorldState = GetRegionWorldState(GetWorld());
	if (!DBPlayer || !Pawn || !WorldState)
	{
		return false;
	}
	const FName RegionId = DBPlayer->GetCurrentRegionId();
	const R::FRegionState* Region = WorldState->GetRulesState().FindRegion(DBBridge::ToStd(RegionId));
	if (!Region || DBRegions::GetRegionNumber(RegionId) == 0)
	{
		return false;
	}
	const int32 Budget = R::GetRegionalPackBudget(*Region, WorldState->IsNight());
	if (!bIgnoreBudget && CountPacksNear(Pawn->GetActorLocation(), PackShareRadius) >= Budget)
	{
		return false;
	}
	// A spot 45-75 m away in a random direction, on free dry ground.
	const R::FRegionalSpawnRules Rules;
	const FVector From = Pawn->GetActorLocation();
	for (int32 Attempt = 0; Attempt < 10; ++Attempt)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * UE_PI);
		const float Distance = FMath::FRandRange(4500.f, 7500.f);
		const FVector2D Meters = (FVector2D(From) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance) / 100.0;
		if (!IsFreeRegionGround(Meters, RegionId))
		{
			continue;
		}
		FPack Pack;
		Pack.Region = RegionId;
		Pack.Center = FVector(Meters.X * 100.0, Meters.Y * 100.0, DBRealm::SampleHeight(Meters.X, Meters.Y) * 100.0);
		const bool bEliteLeader = Region->Control == R::ERegionControl::Occupied || Region->Kind == R::ERegionKind::FinalRegion;
		for (int32 Index = 0; Index < Rules.PackSize; ++Index)
		{
			const float Around = 2.f * UE_PI * Index / Rules.PackSize;
			const FVector2D Spot = Meters + FVector2D(FMath::Cos(Around), FMath::Sin(Around)) * 3.5;
			const FVector At(Spot.X * 100.0, Spot.Y * 100.0, DBRealm::SampleHeight(Spot.X, Spot.Y) * 100.0 + 120.0);
			if (ADBEnemyCharacter* Demon = DBRegions::SpawnDemon(GetWorld(), RegionId, At, (From - At).GetSafeNormal2D().Rotation(), ++SpawnCounter, bEliteLeader && Index == 0))
			{
				Pack.Demons.Add(Demon);
			}
		}
		if (Pack.Demons.Num() == 0)
		{
			return false;
		}
		UE_LOG(LogDarkBlood, Display, TEXT("Region %s: pack of %d %s at (%.0f, %.0f) m for %s (%s, %s)"), *RegionId.ToString(), Pack.Demons.Num(),
			*DBRegions::GetDemonName(RegionId).ToString(), Meters.X, Meters.Y, *Player->GetPlayerName(), UTF8_TO_TCHAR(R::ToString(Region->Control)),
			WorldState->IsNight() ? TEXT("night") : TEXT("day"));
		Packs.Add(MoveTemp(Pack));
		return true;
	}
	return false;
}

void UDBRegionLifeComponent::LogState() const
{
	for (const FPack& Pack : Packs)
	{
		int32 Alive = 0;
		int32 Level = 0;
		for (const TWeakObjectPtr<ADBEnemyCharacter>& Demon : Pack.Demons)
		{
			if (Demon.IsValid() && !Demon->IsDead())
			{
				++Alive;
				Level = FMath::Max(Level, Demon->GetCombatLevel());
			}
		}
		UE_LOG(LogDarkBlood, Display, TEXT("DBREGION pack %s at (%.0f, %.0f) m: %d alive, level up to %d"), *Pack.Region.ToString(), Pack.Center.X / 100.0,
			Pack.Center.Y / 100.0, Alive, Level);
	}
	UE_LOG(LogDarkBlood, Display, TEXT("DBREGION %d packs, %d demons"), Packs.Num(), CountLivingDemons());
}

// ---- Camps -----------------------------------------------------------------------------------------------------

ADBDemonCamp::ADBDemonCamp()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 820.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(60.f);
	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Root);
	FireLight->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	FireLight->SetMobility(EComponentMobility::Movable);
	FireLight->SetIntensity(9000.f);
	FireLight->SetAttenuationRadius(1600.f);
	FireLight->SetLightColor(FLinearColor(1.f, 0.35f, 0.12f));
	FireLight->SetCastShadows(false);
}

void ADBDemonCamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBDemonCamp, RegionId);
	DOREPLIFETIME(ADBDemonCamp, bBroken);
}

FName ADBDemonCamp::GetCommanderId() const
{
	return FName(*(TEXT("MidBoss_") + RegionId.ToString()));
}

void ADBDemonCamp::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Camp();
}

void ADBDemonCamp::OnRep_Camp()
{
	BuildCamp();
	for (UStaticMeshComponent* Part : StandingParts)
	{
		Part->SetVisibility(!bBroken);
	}
	FireLight->SetVisibility(!bBroken);
	const UDBBossDefinition* Definition = DBBosses::Find(GetCommanderId());
	if (Definition)
	{
		Label->SetText(bBroken ? LOCTEXT("CampBroken", "Daemonenlager (zerschlagen)")
							   : FText::Format(LOCTEXT("Camp", "Daemonenlager: {0}"), Definition->DisplayName));
		Label->SetTextRenderColor((bBroken ? FLinearColor(0.5f, 0.5f, 0.5f) : Definition->Color).ToFColor(true));
	}
}

void ADBDemonCamp::BuildCamp()
{
	const UDBBossDefinition* Definition = DBBosses::Find(GetCommanderId());
	UStaticMesh* Cube = CampEngineMesh(TEXT("Cube"));
	UStaticMesh* Cylinder = CampEngineMesh(TEXT("Cylinder"));
	UStaticMesh* Cone = CampEngineMesh(TEXT("Cone"));
	if (bBuilt || !Definition || !Cube || !Cylinder || !Cone)
	{
		return;
	}
	bBuilt = true;
	auto AddPart = [this](UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material, bool bStanding)
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
		if (bStanding)
		{
			StandingParts.Add(Part);
		}
		return Part;
	};
	auto Model = [&AddPart](const TCHAR* Key, float Height, const FVector& At, float Yaw, UMaterialInterface* Material)
	{
		const FTransform Placement(FRotator(0.f, Yaw, 0.f), At);
		for (const DBModels::FPlacedPart& Part : DBModels::GetNormalizedParts(Key, Height))
		{
			AddPart(Part.Mesh, Part.Local * Placement, Material, false);
		}
	};
	using M = EDBArtMaterial;
	// The entrance faces the region's heart; corrupted torii mark the way in.
	const FDBRealmRegion* Region = DBRealm::GetRegions().FindByPredicate([this](const FDBRealmRegion& Candidate) { return Candidate.RegionId == RegionId; });
	const FVector2D Here = FVector2D(GetActorLocation()) / 100.0;
	const float Facing = Region ? FMath::RadiansToDegrees(FMath::Atan2(Region->Center.Y - Here.Y, Region->Center.X - Here.X)) : 0.f;
	const FVector Forward = FRotator(0.f, Facing, 0.f).Vector();
	Model(TEXT("torii_game"), 620.f, Forward * 1300.f, Facing, UDBArtMaterialSubsystem::Get(M::StoneCorrupted));
	// Bonfire: charred logs round a glowing heart (one unshadowed light, off once the camp is broken).
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const float Yaw = 60.f * Index;
		AddPart(Cylinder, FTransform(FRotator(70.f, Yaw, 0.f), FRotator(0.f, Yaw, 0.f).Vector() * 45.f + FVector(0.f, 0.f, 40.f), FVector(0.25f, 0.25f, 1.6f)),
			UDBArtMaterialSubsystem::Get(M::WoodBurnt), false);
	}
	AddPart(Cone, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 70.f), FVector(0.9f, 0.9f, 1.4f)), UDBArtMaterialSubsystem::Get(M::LanternFire), true);
	AddPart(Cylinder, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 4.f), FVector(3.2f, 3.2f, 0.08f)), UDBArtMaterialSubsystem::Get(M::DarkBloodSoil), false);
	// War banners in the commander's color round the fire, stone lanterns of corrupted stone further out.
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float Yaw = Facing + 36.f + 72.f * Index;
		const FVector At = FRotator(0.f, Yaw, 0.f).Vector() * 820.f;
		AddPart(Cylinder, FTransform(FRotator::ZeroRotator, At + FVector(0.f, 0.f, 330.f), FVector(0.14f, 0.14f, 6.6f)), UDBArtMaterialSubsystem::Get(M::WoodDark), false);
		AddPart(Cube, FTransform(FRotator(0.f, Yaw + 90.f, 0.f), At + FVector(0.f, 0.f, 470.f) + FRotator(0.f, Yaw + 90.f, 0.f).Vector() * 60.f, FVector(1.1f, 0.04f, 2.4f)),
			UDBArtMaterialSubsystem::Get(M::FabricCrimson), true);
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Yaw = Facing + 90.f * Index + 45.f;
		Model(TEXT("lantern_stone"), 200.f, FRotator(0.f, Yaw, 0.f).Vector() * 1150.f, Yaw + 180.f, UDBArtMaterialSubsystem::Get(M::StoneCorrupted));
	}
	// Ruined palisade stakes on the far side.
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const float Yaw = Facing + 130.f + 12.5f * Index;
		AddPart(Cone, FTransform(FRotator(0.f, 0.f, 0.f), FRotator(0.f, Yaw, 0.f).Vector() * 1450.f + FVector(0.f, 0.f, 150.f), FVector(0.45f, 0.45f, 3.f)),
			UDBArtMaterialSubsystem::Get(M::WoodBurnt), false);
	}
}

void ADBDemonCamp::SpawnCamps(UWorld* World)
{
	for (const FDBRealmRegion& Region : DBRealm::GetRegions())
	{
		const FVector Location = DBRegions::GetCampLocation(Region.RegionId);
		if (Location.IsZero() || Find(World, Region.RegionId))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADBDemonCamp* Camp = World->SpawnActor<ADBDemonCamp>(ADBDemonCamp::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Camp->RegionId = Region.RegionId;
			Camp->FinishSpawning(FTransform(Location));
		}
	}
}

ADBDemonCamp* ADBDemonCamp::Find(const UWorld* World, FName RegionId)
{
	for (TActorIterator<ADBDemonCamp> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->RegionId == RegionId)
		{
			return *It;
		}
	}
	return nullptr;
}

int32 ADBDemonCamp::CountPlayersWithin(float Radius) const
{
	int32 Count = 0;
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
			Count += Pawn && FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation()) < Radius ? 1 : 0;
		}
	}
	return Count;
}

void ADBDemonCamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Everywhere (vegetation is not replicated): no baked trees or rocks inside the camp. Cells stream, so keep looking.
	VegetationTimer -= DeltaSeconds;
	if (VegetationTimer <= 0.f)
	{
		VegetationTimer = 2.f;
		for (TActorIterator<ADBRealmVegetation> It(GetWorld()); It; ++It)
		{
			if (!ClearedVegetation.Contains(*It))
			{
				ClearedVegetation.Add(*It);
				It->RemoveInstancesInCircle(GetActorLocation(), 1700.f);
			}
		}
	}
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
	const UDBWorldStateComponent* WorldState = GetRegionWorldState(GetWorld());
	const bool bDefeated = WorldState && WorldState->GetRulesState().DefeatedBosses.count(DBBridge::ToStd(GetCommanderId())) > 0;
	if (bDefeated != bBroken)
	{
		bBroken = bDefeated;
		OnRep_Camp();
		if (bBroken)
		{
			UE_LOG(LogDarkBlood, Display, TEXT("Camp %s broken"), *RegionId.ToString());
		}
	}
	if (bBroken)
	{
		return;
	}
	if (!Commander.IsValid())
	{
		if (const int32 Players = CountPlayersWithin(5500.f))
		{
			StartEncounter(Players);
		}
		return;
	}
	EmptySeconds = CountPlayersWithin(12000.f) > 0 ? 0.f : EmptySeconds + 1.f;
	if (EmptySeconds > 8.f && !Commander->IsDead())
	{
		EndEncounter();
	}
}

void ADBDemonCamp::StartEncounter(int32 Players)
{
	const UDBBossDefinition* Definition = DBBosses::Find(GetCommanderId());
	if (!Definition)
	{
		return;
	}
	Commander = ADBBossCharacter::SpawnBoss(GetWorld(), Definition, GetActorLocation() + FVector(-350.f, 0.f, 200.f), FRotator::ZeroRotator, Players);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float Yaw = 120.f * Index + 30.f;
		const FVector At = GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * 600.f + FVector(0.f, 0.f, 150.f);
		if (ADBEnemyCharacter* Guard = DBRegions::SpawnDemon(GetWorld(), RegionId, At, FRotator(0.f, Yaw + 180.f, 0.f), 9000 + Index, false))
		{
			Guards.Add(Guard);
		}
	}
	EmptySeconds = 0.f;
	UE_LOG(LogDarkBlood, Display, TEXT("Camp %s: %s and %d guards wake (%d players)"), *RegionId.ToString(), *Definition->DisplayName.ToString(), Guards.Num(), Players);
}

void ADBDemonCamp::EndEncounter()
{
	if (Commander.IsValid())
	{
		Commander->Destroy();
	}
	for (const TWeakObjectPtr<ADBEnemyCharacter>& Guard : Guards)
	{
		if (Guard.IsValid() && !Guard->IsDead())
		{
			Guard->Destroy();
		}
	}
	Commander.Reset();
	Guards.Reset();
	UE_LOG(LogDarkBlood, Display, TEXT("Camp %s: nobody near, the camp rests"), *RegionId.ToString());
}

#undef LOCTEXT_NAMESPACE
