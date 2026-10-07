#include "World/DBDungeon.h"

#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBCombatEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Art/DBArtBatcher.h"
#include "Art/DBArtMaterials.h"
#include "Art/DBModelLibrary.h"
#include "Boss/DBBoss.h"
#include "Boss/DBBossDefinition.h"
#include "Character/DBHorse.h"
#include "Character/DBLesserDemon.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/DBGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StrongObjectPtr.h"
#include "World/DBEchoHall.h"
#include "World/DBEconomyActors.h"
#include "World/DBRealmLayout.h"
#include "World/DBWorldStateComponent.h"

#define LOCTEXT_NAMESPACE "DarkBloodDungeon"

namespace R = DarkBlood::Rules;

namespace
{
	enum : uint8
	{
		RoomDormant = 0,
		RoomFighting = 1,
		RoomCleared = 2,
	};

	/** Interiors stand west of the landscape, over the open sea, 30 m above the water. */
	constexpr double InteriorWestOfRealm = 2500.0;
	constexpr double InteriorSpacing = 500.0;
	constexpr double InteriorFloorHeight = 30.0;
	/** Seconds an uncleared, empty interior waits before it is removed (fresh demons next time). */
	constexpr float EmptyLifetime = 300.f;
	/** Abyss floors are left behind for good: an empty one goes sooner, cleared or not. */
	constexpr float AbyssEmptyLifetime = 90.f;
	/** The two places Abyss floors alternate between are this far apart (cm) - more than a whole 40-cell grid. */
	constexpr double AbyssFloorSpacing = 30000.0;

	const UDBWorldStateComponent* GetDungeonWorldState(const UWorld* World)
	{
		const ADBGameState* GameState = World ? World->GetGameState<ADBGameState>() : nullptr;
		return GameState ? GameState->GetWorldState() : nullptr;
	}

	FText AbyssFloorName(int32 Depth)
	{
		return FText::Format(LOCTEXT("AbyssFloor", "Der Abgrund - Ebene {0}"), FText::AsNumber(Depth));
	}

	UWorld* GetWorldOf(const APlayerController* User)
	{
		return User ? User->GetWorld() : nullptr;
	}

	UAbilitySystemComponent* GetPlayerAbilitySystem(const APawn* Pawn)
	{
		const IAbilitySystemInterface* Owner = Pawn ? Cast<IAbilitySystemInterface>(Pawn->GetPlayerState()) : nullptr;
		return Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	}

	/** Damage that ignores armor (traps), through the damage meta attribute so death works as in combat. */
	void ApplyTrapDamage(UAbilitySystemComponent* ASC, float Amount)
	{
		static TStrongObjectPtr<UGameplayEffect> Effect;
		if (!Effect.IsValid())
		{
			UGameplayEffect* NewEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("DBTrapDamage"));
			NewEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
			FGameplayModifierInfo Modifier;
			Modifier.Attribute = UDBAttributeSet::GetIncomingDamageAttribute();
			Modifier.ModifierOp = EGameplayModOp::Additive;
			FSetByCallerFloat Magnitude;
			Magnitude.DataTag = DBTags::SetByCaller_Magnitude;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
			NewEffect->Modifiers.Add(Modifier);
			Effect.Reset(NewEffect);
		}
		FGameplayEffectSpec Spec(Effect.Get(), ASC->MakeEffectContext(), 1.f);
		Spec.SetSetByCallerMagnitude(DBTags::SetByCaller_Magnitude, Amount);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);
	}

	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
		const FRotator& Rotation = FRotator::ZeroRotator, bool bCollision = true)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		Part->SetRelativeRotation(Rotation);
		Part->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAllDynamic_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Part->SetCanEverAffectNavigation(false);
		return Part;
	}
}

// ---- Sites ---------------------------------------------------------------------------------------------------

R::FDungeonParams FDBDungeonSite::GetParams() const
{
	R::FDungeonParams Params;
	Params.Difficulty = Difficulty;
	Params.RoomCount = 7 + Difficulty;
	Params.GridSize = 40;
	return Params;
}

const TArray<FDBDungeonSite>& DBDungeon::GetSites()
{
	static TArray<FDBDungeonSite> Sites;
	if (Sites.Num() > 0)
	{
		return Sites;
	}
	struct FDef
	{
		const TCHAR* Id;
		const TCHAR* Name;
		const TCHAR* Region;
		int32 Difficulty;
		FVector2D Offset;
	};
	const FDef Defs[] = {
		{TEXT("D_Shrine"), TEXT("Verfallener Schrein"), TEXT("Region01"), 1, FVector2D(700.0, 500.0)},
		{TEXT("D_BambooCrypt"), TEXT("Bambusgruft"), TEXT("Region03"), 2, FVector2D(-600.0, 600.0)},
		{TEXT("D_IceCave"), TEXT("Eishoehle"), TEXT("Region02"), 3, FVector2D(-500.0, -700.0)},
		{TEXT("D_SpiritGrotto"), TEXT("Geistergrotte"), TEXT("Region08"), 3, FVector2D(500.0, -500.0)},
		{TEXT("D_DesertTomb"), TEXT("Wuestengrab"), TEXT("Region06"), 3, FVector2D(-700.0, -600.0)},
		{TEXT("D_AshMine"), TEXT("Aschestollen"), TEXT("Region05"), 4, FVector2D(600.0, 600.0)},
		{TEXT("D_FortressDungeon"), TEXT("Festungskerker"), TEXT("Region14"), 5, FVector2D(-500.0, 400.0)},
		{TEXT("D_DemonMaw"), TEXT("Daemonenschlund"), TEXT("Region13"), 5, FVector2D(400.0, -600.0)},
		// The Abyss: a rift that tore open outside the capital when the demon king fell.
		{TEXT("D_Abyss"), TEXT("Der Abgrund"), TEXT("Capital"), 5, FVector2D(-650.0, -450.0)},
		// The way to the Hall of Echoes, where the fallen vassals are remembered.
		{TEXT("D_EchoHall"), TEXT("Halle der Echos"), TEXT("Capital"), 5, FVector2D(650.0, 450.0)},
	};
	const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
	for (const FDef& Def : Defs)
	{
		FDBDungeonSite& Site = Sites.AddDefaulted_GetRef();
		Site.Id = Def.Id;
		Site.Name = Def.Name;
		Site.RegionId = Def.Region;
		Site.Difficulty = Def.Difficulty;
		Site.Seed = FCrc::StrCrc32(Def.Id);
		Site.bAbyss = FCString::Strcmp(Def.Id, TEXT("D_Abyss")) == 0;
		Site.bEchoHall = FCString::Strcmp(Def.Id, TEXT("D_EchoHall")) == 0;
		const FDBRealmRegion* Region = Regions.FindByPredicate([&Def](const FDBRealmRegion& Candidate) { return Candidate.RegionId == FName(Def.Region); });
		const FVector2D Center = Region ? Region->Center : FVector2D::ZeroVector;
		// On dry land, inside the realm and clear of settlements: turn and push the offset until it fits.
		FVector2D Offset = Def.Offset;
		for (int32 Attempt = 0; Attempt < 24; ++Attempt)
		{
			const FVector2D Candidate = Center + Offset;
			const bool bDry = DBRealm::SampleHeight(Candidate.X, Candidate.Y) > 3.0;
			const bool bFree = !DBRealm::GetSettlements().ContainsByPredicate([&Candidate](const FDBRealmSettlement& Settlement)
			{
				return FVector2D::Distance(Candidate, Settlement.Center) < Settlement.Radius + 150.0;
			});
			if (bDry && bFree && DBRealm::IsInside(Candidate.X * 1.05, Candidate.Y * 1.05))
			{
				break;
			}
			Offset = Offset.GetRotated(30.0) * (Attempt % 6 == 5 ? 0.8 : 1.0);
		}
		Site.Entrance = Center + Offset;
		const int32 Index = Sites.Num() - 1;
		Site.InteriorOrigin = FVector(-(DBRealm::HalfSize + InteriorWestOfRealm + Index * InteriorSpacing) * 100.0, -12000.0, InteriorFloorHeight * 100.0);
	}
	return Sites;
}

int32 DBDungeon::FindSite(const FString& IdOrName)
{
	const TArray<FDBDungeonSite>& Sites = GetSites();
	for (int32 Index = 0; Index < Sites.Num(); ++Index)
	{
		if (Sites[Index].Id.ToString().Equals(IdOrName, ESearchCase::IgnoreCase) || Sites[Index].Name.StartsWith(IdOrName, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FVector DBDungeon::CellToWorld(const FDBDungeonSite& Site, int32 X, int32 Y)
{
	return Site.InteriorOrigin + FVector((X + 0.5f) * CellSize, (Y + 0.5f) * CellSize, 0.f);
}

int32 DBDungeon::GetAbyssSite()
{
	return GetSites().IndexOfByPredicate([](const FDBDungeonSite& Site) { return Site.bAbyss; });
}

FVector DBDungeon::GetAbyssOrigin(int32 Depth)
{
	const int32 Site = GetAbyssSite();
	const FVector Base = GetSites().IsValidIndex(Site) ? GetSites()[Site].InteriorOrigin : FVector::ZeroVector;
	return Base + FVector(0.0, (Depth % 2) * AbyssFloorSpacing, 0.0);
}

// ---- Instance ------------------------------------------------------------------------------------------------

ADBDungeonInstance::ADBDungeonInstance()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// The art batcher builds static pieces; they only attach to a static root.
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void ADBDungeonInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBDungeonInstance, SiteIndex);
}

ADBDungeonInstance* ADBDungeonInstance::Find(const UWorld* World, int32 SiteIndex)
{
	for (TActorIterator<ADBDungeonInstance> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->SiteIndex == SiteIndex && !It->IsActorBeingDestroyed() && !It->bRetired)
		{
			return *It;
		}
	}
	return nullptr;
}

ADBDungeonInstance* ADBDungeonInstance::FindAt(const UWorld* World, const FVector& Location)
{
	for (TActorIterator<ADBDungeonInstance> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->ContainsLocation(Location))
		{
			return *It;
		}
	}
	return nullptr;
}

ADBDungeonInstance* ADBDungeonInstance::FindOrSpawn(UWorld* World, int32 SiteIndex)
{
	if (!DBDungeon::GetSites().IsValidIndex(SiteIndex))
	{
		return nullptr;
	}
	if (ADBDungeonInstance* Existing = Find(World, SiteIndex))
	{
		return Existing;
	}
	const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
	if (Site.bAbyss)
	{
		return FindOrSpawnAbyss(World, 1);
	}
	if (Site.bEchoHall)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.bDeferConstruction = true;
	ADBDungeonInstance* Instance = World->SpawnActor<ADBDungeonInstance>(ADBDungeonInstance::StaticClass(), FTransform(Site.InteriorOrigin), Params);
	if (Instance)
	{
		Instance->SiteIndex = SiteIndex;
		Instance->FinishSpawning(FTransform(Site.InteriorOrigin));
	}
	return Instance;
}

ADBDungeonInstance* ADBDungeonInstance::FindOrSpawnAbyss(UWorld* World, int32 Depth)
{
	const int32 Site = DBDungeon::GetAbyssSite();
	if (!World || Site == INDEX_NONE)
	{
		return nullptr;
	}
	Depth = FMath::Max(1, Depth);
	for (TActorIterator<ADBDungeonInstance> It(World); It; ++It)
	{
		if (It->SiteIndex == Site && It->AbyssDepth == Depth && !It->bRetired && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	const UDBWorldStateComponent* WorldState = GetDungeonWorldState(World);
	const FTransform At(DBDungeon::GetAbyssOrigin(Depth));
	FActorSpawnParameters Params;
	Params.bDeferConstruction = true;
	ADBDungeonInstance* Instance = World->SpawnActor<ADBDungeonInstance>(ADBDungeonInstance::StaticClass(), At, Params);
	if (Instance)
	{
		Instance->SiteIndex = Site;
		Instance->AbyssDepth = Depth;
		Instance->AbyssCycle = WorldState ? WorldState->GetCycle() : 0;
		Instance->FinishSpawning(At);
	}
	return Instance;
}

void ADBDungeonInstance::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Site();
	if (HasAuthority())
	{
		SpawnFixtures();
	}
}

void ADBDungeonInstance::OnRep_Site()
{
	if (bBuilt || !DBDungeon::GetSites().IsValidIndex(SiteIndex))
	{
		return;
	}
	const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
	if (Site.bAbyss)
	{
		if (AbyssDepth <= 0)
		{
			return; // the depth replicates with the site; wait for it
		}
		// Each floor its own layout: fewer, tighter rooms than a dungeon, more of them and more demons further down.
		AbyssFloor = R::GetAbyssFloor(AbyssDepth, AbyssCycle);
		R::FDungeonParams Params;
		Params.RoomCount = AbyssFloor.Rooms;
		Params.Difficulty = FMath::Clamp(1 + (AbyssDepth - 1) / 3, 1, 5);
		Params.GridSize = 32;
		Params.ExtraLinks = 1;
		Layout = R::GenerateDungeon(AbyssFloor.Seed, Params);
	}
	else
	{
		Layout = R::GenerateDungeon(Site.Seed, Site.GetParams());
	}
	RoomStates.Init(RoomDormant, static_cast<int32>(Layout.Rooms.size()));
	RoomEnemies.SetNum(static_cast<int32>(Layout.Rooms.size()));
	Build();
	bBuilt = true;
}

void ADBDungeonInstance::Build()
{
	const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
	const bool bDemonic = Site.Difficulty >= 5;
	// The Abyss is old, cold stone; the dark blood only glows in the seams (a vein strip along the floor of every wall)
	// and in the red light of the guardian's room - glowing lava on every surface would drown the fight.
	const EDBArtMaterial Floor = Site.bAbyss ? EDBArtMaterial::StoneRuin : bDemonic ? EDBArtMaterial::DarkBloodStone : EDBArtMaterial::StoneTemple;
	const EDBArtMaterial Wall = Site.bAbyss ? EDBArtMaterial::TerrainCliff : bDemonic ? EDBArtMaterial::StoneCorrupted : EDBArtMaterial::StoneMountain;
	const EDBArtMaterial Ceiling = Site.bAbyss ? EDBArtMaterial::StoneMountain : bDemonic ? EDBArtMaterial::StoneCorrupted : EDBArtMaterial::StoneRuin;
	FDBArtBatcher Batcher(*this, *Root, Pieces);
	Batcher.SetCollision(true);
	const float Half = DBDungeon::CellSize * 0.5f;
	const float Thickness = 60.f;
	const int32 Steps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	int32 CorridorCells = 0;
	for (int32 Y = 0; Y < Layout.Size; ++Y)
	{
		for (int32 X = 0; X < Layout.Size; ++X)
		{
			const R::EDungeonCell Cell = Layout.GetCell(X, Y);
			if (Cell == R::EDungeonCell::Empty)
			{
				continue;
			}
			// Local space: the actor sits at the interior origin.
			const FVector Center((X + 0.5f) * DBDungeon::CellSize, (Y + 0.5f) * DBDungeon::CellSize, 0.f);
			Batcher.Box(Floor, Center - FVector(0.f, 0.f, Thickness * 0.5f), FVector(DBDungeon::CellSize, DBDungeon::CellSize, Thickness));
			Batcher.Box(Ceiling, Center + FVector(0.f, 0.f, DBDungeon::WallHeight + Thickness * 0.5f), FVector(DBDungeon::CellSize, DBDungeon::CellSize, Thickness));
			for (const auto& Step : Steps)
			{
				if (Layout.GetCell(X + Step[0], Y + Step[1]) != R::EDungeonCell::Empty)
				{
					continue;
				}
				const FVector Edge = Center + FVector(Step[0] * Half, Step[1] * Half, DBDungeon::WallHeight * 0.5f);
				const FVector Size = Step[0] != 0 ? FVector(Thickness, DBDungeon::CellSize + Thickness, DBDungeon::WallHeight)
												  : FVector(DBDungeon::CellSize + Thickness, Thickness, DBDungeon::WallHeight);
				Batcher.Box(Wall, Edge, Size);
				if (Site.bAbyss)
				{
					// The seam of dark blood where wall meets floor.
					const FVector Seam = Center + FVector(Step[0] * (Half - Thickness * 0.5f - 6.f), Step[1] * (Half - Thickness * 0.5f - 6.f), 6.f);
					Batcher.Box(EDBArtMaterial::DarkBloodVeins, Seam, Step[0] != 0 ? FVector(12.f, DBDungeon::CellSize, 12.f) : FVector(DBDungeon::CellSize, 12.f, 12.f));
				}
			}
			// A torch every few corridor cells.
			if (Cell == R::EDungeonCell::Corridor && (++CorridorCells % 5) == 0)
			{
				UPointLightComponent* Torch = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
				Torch->SetupAttachment(Root);
				Torch->SetRelativeLocation(Center + FVector(0.f, 0.f, DBDungeon::WallHeight - 80.f));
				Torch->SetIntensity(2500.f);
				Torch->SetAttenuationRadius(1400.f);
				Torch->SetLightColor(Site.bAbyss ? FLinearColor(0.75f, 0.32f, 0.42f) : FLinearColor(1.f, 0.55f, 0.25f));
				Torch->SetCastShadows(false);
				Torch->RegisterComponent();
				Lights.Add(Torch);
			}
		}
	}
	// Rooms: a hanging fire basket each (red in the guardian's room).
	Batcher.SetCollision(false);
	for (const R::FDungeonRoom& Room : Layout.Rooms)
	{
		const FVector Center((Room.CenterX() + 0.5f) * DBDungeon::CellSize, (Room.CenterY() + 0.5f) * DBDungeon::CellSize, 0.f);
		Batcher.Sphere(EDBArtMaterial::LanternFire, Center + FVector(0.f, 0.f, DBDungeon::WallHeight - 60.f), FVector(45.f));
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocation(Center + FVector(0.f, 0.f, DBDungeon::WallHeight - 120.f));
		Light->SetIntensity(Room.Kind == R::EDungeonRoomKind::Boss ? 9000.f : 6000.f);
		Light->SetAttenuationRadius(FMath::Max(Room.W, Room.H) * DBDungeon::CellSize);
		Light->SetLightColor(Room.Kind == R::EDungeonRoomKind::Boss ? FLinearColor(1.f, 0.2f, 0.15f)
							 : Room.Kind == R::EDungeonRoomKind::Rest ? FLinearColor(0.6f, 0.8f, 1.f)
							 : Site.bAbyss							  ? FLinearColor(0.8f, 0.45f, 0.6f)
																		: FLinearColor(1.f, 0.65f, 0.35f));
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		Lights.Add(Light);
	}
	UE_LOG(LogDarkBlood, Log, TEXT("Dungeon %s%s built: %d rooms, %d instances, %d lights"), *Site.Name,
		AbyssDepth > 0 ? *FString::Printf(TEXT(" floor %d"), AbyssDepth) : TEXT(""), static_cast<int32>(Layout.Rooms.size()), Batcher.GetInstanceCount(), Lights.Num());
}

void ADBDungeonInstance::SpawnFixtures()
{
	if (!bBuilt)
	{
		return;
	}
	const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
	const UDBWorldStateComponent* WorldState = GetDungeonWorldState(GetWorld());
	// Abyss floors are always fresh; a dungeon remembers that it was cleared.
	bCleared = !IsAbyss() && WorldState && WorldState->IsDungeonCleared(Site.Id);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < static_cast<int32>(Layout.Rooms.size()); ++Index)
	{
		const R::FDungeonRoom& Room = Layout.Rooms[static_cast<size_t>(Index)];
		const FVector Center = GetRoomCenter(Index);
		switch (Room.Kind)
		{
		case R::EDungeonRoomKind::Entrance:
			if (ADBDungeonPortal* Exit = GetWorld()->SpawnActor<ADBDungeonPortal>(ADBDungeonPortal::StaticClass(),
					GetCellLocation(Room.X + Room.W - 1, Room.CenterY()), FRotator(0.f, 90.f, 0.f), Params))
			{
				Exit->Setup(SiteIndex, true);
			}
			break;
		case R::EDungeonRoomKind::Treasure:
			if (ADBLootChest* Chest = GetWorld()->SpawnActor<ADBLootChest>(ADBLootChest::StaticClass(), Center + FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator, Params))
			{
				Chest->Setup(TEXT("LT_DungeonChest"), IsAbyss() ? AbyssFloor.RarityBonus : 0.f);
			}
			break;
		case R::EDungeonRoomKind::Rest:
			GetWorld()->SpawnActor<ADBDungeonShrine>(ADBDungeonShrine::StaticClass(), Center, FRotator::ZeroRotator, Params);
			break;
		case R::EDungeonRoomKind::Trap:
			// Fire vents on a checkerboard: the two halves burn in turn, so there is always a way through.
			for (int32 Y = Room.Y; Y < Room.Y + Room.H; ++Y)
			{
				for (int32 X = Room.X; X < Room.X + Room.W; ++X)
				{
					if (ADBDungeonTrap* Trap = GetWorld()->SpawnActor<ADBDungeonTrap>(ADBDungeonTrap::StaticClass(), GetCellLocation(X, Y), FRotator::ZeroRotator, Params))
					{
						Trap->PhaseOffset = ((X + Y) % 2) * ADBDungeonTrap::CycleSeconds * 0.5f;
					}
				}
			}
			break;
		default:
			break;
		}
		if (bCleared)
		{
			RoomStates[Index] = RoomCleared;
		}
	}
	if (bCleared)
	{
		const int32 Boss = Layout.FindRoom(R::EDungeonRoomKind::Boss);
		if (ADBDungeonPortal* Exit = GetWorld()->SpawnActor<ADBDungeonPortal>(ADBDungeonPortal::StaticClass(), GetRoomCenter(Boss), FRotator::ZeroRotator, Params))
		{
			Exit->Setup(SiteIndex, true);
		}
	}
}

FVector ADBDungeonInstance::GetRoomCenter(int32 Room) const
{
	if ((Room < 0 || Room >= static_cast<int32>(Layout.Rooms.size())) || !DBDungeon::GetSites().IsValidIndex(SiteIndex))
	{
		return GetActorLocation();
	}
	return GetCellLocation(Layout.Rooms[static_cast<size_t>(Room)].CenterX(), Layout.Rooms[static_cast<size_t>(Room)].CenterY());
}

FVector ADBDungeonInstance::GetCellLocation(int32 X, int32 Y) const
{
	return GetActorLocation() + FVector((X + 0.5f) * DBDungeon::CellSize, (Y + 0.5f) * DBDungeon::CellSize, 0.f);
}

bool ADBDungeonInstance::ContainsLocation(const FVector& Location) const
{
	const FVector Local = Location - GetActorLocation();
	const float Extent = Layout.Size * DBDungeon::CellSize;
	return Local.X >= 0.f && Local.Y >= 0.f && Local.X < Extent && Local.Y < Extent && Local.Z > -800.f && Local.Z < DBDungeon::WallHeight + 800.f;
}

TArray<APawn*> ADBDungeonInstance::GetPlayersInside() const
{
	TArray<APawn*> Players;
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
			if (Pawn && ContainsLocation(Pawn->GetActorLocation()))
			{
				Players.Add(Pawn);
			}
		}
	}
	return Players;
}

int32 ADBDungeonInstance::CountLivingEnemies(int32 Room) const
{
	int32 Count = 0;
	if (RoomEnemies.IsValidIndex(Room))
	{
		for (const TWeakObjectPtr<ADBEnemyCharacter>& Enemy : RoomEnemies[Room])
		{
			Count += Enemy.IsValid() && !Enemy->IsDead() ? 1 : 0;
		}
	}
	return Count;
}

void ADBDungeonInstance::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bBuilt)
	{
		return;
	}
	UpdateTimer -= DeltaSeconds;
	if (UpdateTimer > 0.f)
	{
		return;
	}
	UpdateTimer = 0.5f;
	const TArray<APawn*> Players = GetPlayersInside();
	EmptySeconds = Players.Num() > 0 ? 0.f : EmptySeconds + 0.5f;
	for (const APawn* Player : Players)
	{
		const FVector Local = (Player->GetActorLocation() - GetActorLocation()) / DBDungeon::CellSize;
		const int32 Room = Layout.FindRoomAt(FMath::FloorToInt(Local.X), FMath::FloorToInt(Local.Y));
		if (RoomStates.IsValidIndex(Room) && RoomStates[Room] == RoomDormant)
		{
			ActivateRoom(Room);
		}
	}
	for (int32 Room = 0; Room < RoomStates.Num(); ++Room)
	{
		if (RoomStates[Room] == RoomFighting && CountLivingEnemies(Room) == 0)
		{
			RoomStates[Room] = RoomCleared;
			UE_LOG(LogDarkBlood, Display, TEXT("Dungeon %s: room %d (%hs) cleared"), *DBDungeon::GetSites()[SiteIndex].Name, Room, R::ToString(Layout.Rooms[static_cast<size_t>(Room)].Kind));
			if (Layout.Rooms[static_cast<size_t>(Room)].Kind == R::EDungeonRoomKind::Boss)
			{
				ClearDungeon();
			}
		}
	}
	// Abandoned before the end: the demons regroup (a fresh interior next time). Abyss floors are never revisited.
	if ((!bCleared && EmptySeconds > EmptyLifetime) || (IsAbyss() && EmptySeconds > AbyssEmptyLifetime))
	{
		DestroyWithFixtures();
	}
}

void ADBDungeonInstance::DestroyWithFixtures()
{
	for (TArray<TWeakObjectPtr<ADBEnemyCharacter>>& Enemies : RoomEnemies)
	{
		for (const TWeakObjectPtr<ADBEnemyCharacter>& Enemy : Enemies)
		{
			if (Enemy.IsValid())
			{
				Enemy->Destroy();
			}
		}
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if ((It->IsA<ADBDungeonPortal>() || It->IsA<ADBDungeonTrap>() || It->IsA<ADBDungeonShrine>() || It->IsA<ADBLootChest>()) && ContainsLocation(It->GetActorLocation()))
		{
			It->Destroy();
		}
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Dungeon %s%s removed"), *DBDungeon::GetSites()[SiteIndex].Name, IsAbyss() ? *FString::Printf(TEXT(" floor %d"), AbyssDepth) : TEXT(""));
	Destroy();
}

ADBEnemyCharacter* ADBDungeonInstance::SpawnDemon(const FVector& Location, const FRotator& Rotation)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.bDeferConstruction = true;
	ADBLesserDemon* Demon = GetWorld()->SpawnActor<ADBLesserDemon>(ADBLesserDemon::StaticClass(), Location, Rotation, Params);
	if (!Demon)
	{
		return nullptr;
	}
	if (IsAbyss())
	{
		// The floor's strength replaces the cycle scale (the floor already includes the cycle).
		const R::FEndgameScale Cycle = R::GetCycleScale(AbyssCycle);
		Demon->SetEndgameScale({AbyssFloor.EnemyHealth, AbyssFloor.EnemyDamage, Cycle.Experience * (1.f + 0.05f * (AbyssDepth - 1)), AbyssFloor.RarityBonus,
			AbyssFloor.EnemyLevelBonus});
	}
	Demon->FinishSpawning(FTransform(Rotation, Location));
	return Demon;
}

void ADBDungeonInstance::ActivateRoom(int32 Room)
{
	const R::FDungeonRoom& Data = Layout.Rooms[static_cast<size_t>(Room)];
	if (Data.Enemies <= 0)
	{
		RoomStates[Room] = RoomCleared;
		return;
	}
	RoomStates[Room] = RoomFighting;
	// Co-op: one more demon per additional player inside. Abyss floors set their own numbers; without a guardian the
	// last room holds an elite pack instead.
	const bool bGuardian = Data.Kind == R::EDungeonRoomKind::Boss && (!IsAbyss() || AbyssFloor.bGuardianFloor);
	const int32 Base = !IsAbyss() ? Data.Enemies : Data.Kind == R::EDungeonRoomKind::Boss ? (bGuardian ? 2 : AbyssFloor.EnemiesPerRoom + 2) : AbyssFloor.EnemiesPerRoom;
	const int32 Count = FMath::Min(10, Base + FMath::Max(0, GetPlayersInside().Num() - 1));
	const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
	FRandomStream Random(static_cast<int32>(IsAbyss() ? AbyssFloor.Seed : Site.Seed) + Room * 131);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Along the far side of the room from the entrance.
		const int32 X = Data.X + Random.RandRange(0, Data.W - 1);
		const int32 Y = Data.Y + Random.RandRange(0, Data.H - 1);
		const FVector Location = GetCellLocation(X, Y) + FVector(Random.FRandRange(-150.f, 150.f), Random.FRandRange(-150.f, 150.f), 120.f);
		if (ADBEnemyCharacter* Demon = SpawnDemon(Location, FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f)))
		{
			RoomEnemies[Room].Add(Demon);
		}
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Dungeon %s%s: room %d (%hs) wakes with %d demons"), *Site.Name, IsAbyss() ? *FString::Printf(TEXT(" floor %d"), AbyssDepth) : TEXT(""),
		Room, R::ToString(Data.Kind), Count);
	if (bGuardian)
	{
		// The guardian: a boss (phases, telegraphed attacks) that grows with the dungeon stage and the party size; in the
		// Abyss with the floor (every fifth floor).
		const FVector Center = GetRoomCenter(Room) + FVector(0.f, 0.f, 150.f);
		FDBEndgameScale GuardianScale;
		if (IsAbyss())
		{
			GuardianScale = {AbyssFloor.EnemyHealth, AbyssFloor.EnemyDamage, R::GetCycleScale(AbyssCycle).Experience * (1.f + 0.1f * AbyssDepth), AbyssFloor.RarityBonus,
				AbyssFloor.EnemyLevelBonus};
		}
		if (ADBBossCharacter* Guardian = ADBBossCharacter::SpawnBoss(GetWorld(), DBBosses::Find(TEXT("B_DungeonGuardian")), Center, FRotator::ZeroRotator,
				FMath::Max(1, GetPlayersInside().Num()), 0.5f + 0.25f * Site.Difficulty, nullptr, IsAbyss() ? &GuardianScale : nullptr))
		{
			RoomEnemies[Room].Add(Guardian);
		}
		NotifyPlayersInside(IsAbyss() ? FText::Format(LOCTEXT("AbyssGuardian", "Der Waechter der Ebene {0} erwacht!"), FText::AsNumber(AbyssDepth))
									  : LOCTEXT("Guardian", "Der Waechter des Dungeons erwacht!"));
	}
}

void ADBDungeonInstance::ClearDungeon()
{
	if (bCleared)
	{
		return;
	}
	bCleared = true;
	const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
	if (IsAbyss())
	{
		ClearAbyssFloor();
		return;
	}
	if (const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>(); GameState && GameState->GetWorldState())
	{
		GameState->GetWorldState()->NotifyDungeonCleared(Site.Id);
	}
	const int64 Xp = 120 * Site.Difficulty;
	for (const APawn* Player : GetPlayersInside())
	{
		if (const ADBPlayerState* PlayerState = Player->GetPlayerState<ADBPlayerState>(); PlayerState && PlayerState->GetProgression())
		{
			PlayerState->GetProgression()->AwardXp(Xp);
		}
	}
	NotifyPlayersInside(FText::Format(LOCTEXT("Cleared", "{0} gesaeubert! (+{1} XP) Der Weg hinaus ist offen."), FText::FromString(Site.Name), FText::AsNumber(Xp)));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const int32 Boss = Layout.FindRoom(R::EDungeonRoomKind::Boss);
	const FVector Center = GetRoomCenter(Boss);
	if (ADBDungeonPortal* Exit = GetWorld()->SpawnActor<ADBDungeonPortal>(ADBDungeonPortal::StaticClass(), Center, FRotator::ZeroRotator, Params))
	{
		Exit->Setup(SiteIndex, true);
	}
	if (ADBLootChest* Hoard = GetWorld()->SpawnActor<ADBLootChest>(ADBLootChest::StaticClass(), Center + FVector(DBDungeon::CellSize, 0.f, 40.f), FRotator::ZeroRotator, Params))
	{
		Hoard->Setup(TEXT("LT_DungeonGuardian"));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Dungeon %s cleared (+%lld XP for %d players)"), *Site.Name, Xp, GetPlayersInside().Num());
}

void ADBDungeonInstance::ClearAbyssFloor()
{
	if (const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>(); GameState && GameState->GetWorldState())
	{
		GameState->GetWorldState()->NotifyAbyssFloorCleared(AbyssDepth);
	}
	const int64 Xp = FMath::RoundToInt64(90.0 * AbyssDepth * R::GetCycleScale(AbyssCycle).Experience);
	for (const APawn* Player : GetPlayersInside())
	{
		if (const ADBPlayerState* PlayerState = Player->GetPlayerState<ADBPlayerState>(); PlayerState && PlayerState->GetProgression())
		{
			PlayerState->GetProgression()->AwardXp(Xp);
		}
	}
	const FText Text = AbyssFloor.bGuardianFloor
		? FText::Format(LOCTEXT("AbyssGuardianDown", "Ebene {0} bezwungen, der Waechter ist gefallen! (+{1} XP) Ab hier beginnt ihr kuenftig tiefer."),
			FText::AsNumber(AbyssDepth), FText::AsNumber(Xp))
		: FText::Format(LOCTEXT("AbyssFloorDown", "Ebene {0} bezwungen (+{1} XP). Die Treppe fuehrt tiefer hinab."), FText::AsNumber(AbyssDepth), FText::AsNumber(Xp));
	NotifyPlayersInside(Text);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Center = GetRoomCenter(Layout.FindRoom(R::EDungeonRoomKind::Boss));
	// The stair down in the middle, the way home beside it, the floor's hoard behind.
	if (ADBDungeonPortal* Down = GetWorld()->SpawnActor<ADBDungeonPortal>(ADBDungeonPortal::StaticClass(), Center, FRotator::ZeroRotator, Params))
	{
		Down->Setup(SiteIndex, false, AbyssDepth + 1);
	}
	if (ADBDungeonPortal* Exit = GetWorld()->SpawnActor<ADBDungeonPortal>(ADBDungeonPortal::StaticClass(), Center + FVector(0.f, DBDungeon::CellSize, 0.f), FRotator::ZeroRotator, Params))
	{
		Exit->Setup(SiteIndex, true);
	}
	if (ADBLootChest* Hoard = GetWorld()->SpawnActor<ADBLootChest>(ADBLootChest::StaticClass(), Center + FVector(DBDungeon::CellSize, 0.f, 40.f), FRotator::ZeroRotator, Params))
	{
		Hoard->Setup(AbyssFloor.bGuardianFloor ? TEXT("LT_DungeonGuardian") : TEXT("LT_DungeonChest"), AbyssFloor.RarityBonus);
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Abyss floor %d cleared (+%lld XP for %d players, guardian %d, rarity bonus %.2f)"), AbyssDepth, Xp, GetPlayersInside().Num(),
		AbyssFloor.bGuardianFloor ? 1 : 0, AbyssFloor.RarityBonus);
}

void ADBDungeonInstance::NotifyPlayersInside(const FText& Text) const
{
	for (const APawn* Player : GetPlayersInside())
	{
		if (ADBPlayerController* Controller = Cast<ADBPlayerController>(Player->GetController()))
		{
			Controller->ClientShowNotification(Text);
		}
	}
}

bool ADBDungeonInstance::Enter(APlayerController* User, int32 SiteIndex)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	UWorld* World = GetWorldOf(User);
	if (!Pawn || !World || !Pawn->HasAuthority() || !DBDungeon::GetSites().IsValidIndex(SiteIndex))
	{
		return false;
	}
	if (DBDungeon::GetSites()[SiteIndex].bEchoHall)
	{
		return DBEchoHall::Enter(User);
	}
	if (DBDungeon::GetSites()[SiteIndex].bAbyss)
	{
		const UDBWorldStateComponent* WorldState = GetDungeonWorldState(World);
		if (!WorldState || !WorldState->IsEndgameOpen())
		{
			if (ADBPlayerController* Controller = Cast<ADBPlayerController>(User))
			{
				Controller->ClientShowNotification(LOCTEXT("AbyssSealed", "Der Abgrund ist versiegelt. Erst wenn der Daemonenkoenig faellt, reisst er auf."));
			}
			return false;
		}
		// A party already below: join it. Otherwise resume after the last guardian this world has beaten.
		const int32 Site = SiteIndex;
		for (TActorIterator<ADBDungeonInstance> It(World); It; ++It)
		{
			if (It->SiteIndex == Site && !It->bRetired && !It->IsActorBeingDestroyed() && It->GetPlayersInside().Num() > 0)
			{
				return EnterAbyss(User, It->AbyssDepth);
			}
		}
		return EnterAbyss(User, WorldState->GetAbyssDeepest() / 5 * 5 + 1);
	}
	if (ADBHorse* Horse = ADBHorse::FindRiddenBy(Pawn))
	{
		Horse->Interact(User); // horses wait outside
	}
	const bool bNew = Find(World, SiteIndex) == nullptr;
	ADBDungeonInstance* Instance = FindOrSpawn(World, SiteIndex);
	if (!Instance)
	{
		return false;
	}
	const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
	const R::FDungeonRoom& Entrance = Instance->Layout.Rooms[static_cast<size_t>(Instance->Layout.FindRoom(R::EDungeonRoomKind::Entrance))];
	const FVector Arrival = Instance->GetCellLocation(Entrance.X, Entrance.CenterY()) + FVector(0.f, 0.f, 120.f);
	const TWeakObjectPtr<APawn> WeakPawn = Pawn;
	auto Teleport = [WeakPawn, Arrival]()
	{
		if (WeakPawn.IsValid())
		{
			WeakPawn->TeleportTo(Arrival, FRotator(0.f, 0.f, 0.f));
		}
	};
	// A new interior first reaches the clients, then the player follows (never fall into an empty void).
	if (bNew)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Teleport), 0.6f, false);
	}
	else
	{
		Teleport();
	}
	const ADBGameState* GameState = World->GetGameState<ADBGameState>();
	const bool bCleared = GameState && GameState->GetWorldState() && GameState->GetWorldState()->IsDungeonCleared(Site.Id);
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(User))
	{
		Controller->ClientShowNotification(bCleared ? FText::Format(LOCTEXT("EnterCleared", "{0} - gesaeubert, die Daemonen kehren spaeter zurueck."), FText::FromString(Site.Name))
													: FText::Format(LOCTEXT("Enter", "{0} (Stufe {1}) betreten"), FText::FromString(Site.Name), FText::AsNumber(Site.Difficulty)));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Dungeon %s: %s enters%s"), *Site.Name, *GetNameSafe(Pawn), bCleared ? TEXT(" (cleared)") : TEXT(""));
	return true;
}

bool ADBDungeonInstance::EnterAbyss(APlayerController* User, int32 Depth)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	UWorld* World = GetWorldOf(User);
	if (!Pawn || !World || !Pawn->HasAuthority())
	{
		return false;
	}
	if (ADBHorse* Horse = ADBHorse::FindRiddenBy(Pawn))
	{
		Horse->Interact(User);
	}
	Depth = FMath::Max(1, Depth);
	bool bNew = true;
	for (TActorIterator<ADBDungeonInstance> It(World); It && bNew; ++It)
	{
		bNew = !(It->SiteIndex == DBDungeon::GetAbyssSite() && It->AbyssDepth == Depth && !It->bRetired);
	}
	ADBDungeonInstance* Instance = FindOrSpawnAbyss(World, Depth);
	if (!Instance || !Instance->bBuilt)
	{
		return false;
	}
	const R::FDungeonRoom& Entrance = Instance->Layout.Rooms[static_cast<size_t>(Instance->Layout.FindRoom(R::EDungeonRoomKind::Entrance))];
	const FVector Arrival = Instance->GetCellLocation(Entrance.X, Entrance.CenterY()) + FVector(0.f, 0.f, 120.f);
	const TWeakObjectPtr<APawn> WeakPawn = Pawn;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakPawn, Arrival]()
	{
		if (WeakPawn.IsValid())
		{
			WeakPawn->TeleportTo(Arrival, FRotator(0.f, 0.f, 0.f));
		}
	}), bNew ? 0.6f : 0.05f, false);
	const R::FAbyssFloor& Floor = Instance->AbyssFloor;
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(User))
	{
		Controller->ClientShowNotification(FText::Format(LOCTEXT("EnterAbyss", "{0}: {1} Raeume, Daemonen Stufe +{2}{3}"), AbyssFloorName(Depth),
			FText::AsNumber(Floor.Rooms), FText::AsNumber(Floor.EnemyLevelBonus), Floor.bGuardianFloor ? LOCTEXT("GuardianFloor", " - ein Waechter wartet") : FText::GetEmpty()));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Abyss floor %d: %s enters (seed %u, %d rooms, %d per room, health x%.2f, damage x%.2f, guardian %d)"), Depth, *GetNameSafe(Pawn),
		Floor.Seed, Floor.Rooms, Floor.EnemiesPerRoom, Floor.EnemyHealth, Floor.EnemyDamage, Floor.bGuardianFloor ? 1 : 0);
	return true;
}

bool ADBDungeonInstance::Descend(APlayerController* User)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	ADBDungeonInstance* Current = Pawn ? FindAt(Pawn->GetWorld(), Pawn->GetActorLocation()) : nullptr;
	if (!Current || !Current->IsAbyss() || !Current->bCleared || Current->bRetired)
	{
		return false;
	}
	// The whole party goes down together; the floor they leave is gone a moment later.
	const TArray<APawn*> Party = Current->GetPlayersInside();
	bool bAll = true;
	for (APawn* Member : Party)
	{
		APlayerController* Controller = Cast<APlayerController>(Member->GetController());
		bAll &= Controller && EnterAbyss(Controller, Current->AbyssDepth + 1);
	}
	if (!bAll)
	{
		return false;
	}
	Current->bRetired = true;
	const TWeakObjectPtr<ADBDungeonInstance> WeakOld = Current;
	FTimerHandle Handle;
	Current->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakOld]()
	{
		if (WeakOld.IsValid())
		{
			WeakOld->DestroyWithFixtures();
		}
	}), 4.f, false);
	return true;
}

void ADBDungeonInstance::Leave(APlayerController* User, int32 SiteIndex)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	if (!Pawn || !DBDungeon::GetSites().IsValidIndex(SiteIndex))
	{
		return;
	}
	const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
	const FVector2D Out = Site.Entrance + FVector2D(4.0, 0.0);
	Pawn->TeleportTo(FVector(Out.X * 100.0, Out.Y * 100.0, FMath::Max(DBRealm::SampleHeight(Out.X, Out.Y), 0.0) * 100.0 + 150.0), FRotator(0.f, 0.f, 0.f));
	UE_LOG(LogDarkBlood, Display, TEXT("Dungeon %s: %s leaves"), *Site.Name, *GetNameSafe(Pawn));
}

// ---- Portal --------------------------------------------------------------------------------------------------

ADBDungeonPortal::ADBDungeonPortal()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	// A torii: two posts, two beams and a glowing veil between them.
	Parts.Add(MakePart(this, Root, TEXT("PostL"), Cylinder.Object, FVector(0.f, 180.f, 200.f), FVector(0.4f, 0.4f, 4.f)));
	Parts.Add(MakePart(this, Root, TEXT("PostR"), Cylinder.Object, FVector(0.f, -180.f, 200.f), FVector(0.4f, 0.4f, 4.f)));
	Parts.Add(MakePart(this, Root, TEXT("Beam"), Cube.Object, FVector(0.f, 0.f, 420.f), FVector(0.5f, 5.2f, 0.35f)));
	Parts.Add(MakePart(this, Root, TEXT("Tie"), Cube.Object, FVector(0.f, 0.f, 350.f), FVector(0.3f, 4.f, 0.2f)));
	Parts.Add(MakePart(this, Root, TEXT("Veil"), Cube.Object, FVector(0.f, 0.f, 170.f), FVector(0.05f, 3.2f, 3.3f), FRotator::ZeroRotator, false));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 500.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(40.f);
	Label->SetTextRenderColor(FColor(235, 200, 110));
}

void ADBDungeonPortal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBDungeonPortal, SiteIndex);
	DOREPLIFETIME(ADBDungeonPortal, bExit);
	DOREPLIFETIME(ADBDungeonPortal, DescendTo);
}

void ADBDungeonPortal::Setup(int32 InSiteIndex, bool bInExit, int32 InDescendTo)
{
	SiteIndex = InSiteIndex;
	bExit = bInExit;
	DescendTo = InDescendTo;
	OnRep_Setup();
}

void ADBDungeonPortal::OnRep_Setup()
{
	// The veil: a translucent shimmer (warm for the way out, blood red for the way in) - an opaque emissive block
	// burned out to white under the auto exposure.
	if (!VeilMaterial)
	{
		if (UMaterialInterface* Barrier = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Master/M_DB_BloodBarrier.M_DB_BloodBarrier"), nullptr,
				LOAD_NoWarn | LOAD_Quiet))
		{
			VeilMaterial = UMaterialInstanceDynamic::Create(Barrier, this);
		}
	}
	if (VeilMaterial)
	{
		const bool bAbyssSite = DBDungeon::GetSites().IsValidIndex(SiteIndex) && DBDungeon::GetSites()[SiteIndex].bAbyss;
		const bool bHallSite = DBDungeon::GetSites().IsValidIndex(SiteIndex) && DBDungeon::GetSites()[SiteIndex].bEchoHall;
		const FLinearColor Color = bExit ? FLinearColor(1.f, 0.72f, 0.38f) : bHallSite ? FLinearColor(0.45f, 0.7f, 1.f)
								 : DescendTo > 0 || bAbyssSite ? FLinearColor(0.75f, 0.1f, 0.35f) : FLinearColor(1.f, 0.12f, 0.06f);
		VeilMaterial->SetVectorParameterValue(TEXT("BarrierColor"), Color);
		VeilMaterial->SetScalarParameterValue(TEXT("Intensity"), 1.6f);
		VeilMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
	}
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		const bool bAbyssGate = DescendTo > 0 || (!bExit && DBDungeon::GetSites().IsValidIndex(SiteIndex) && DBDungeon::GetSites()[SiteIndex].bAbyss);
		const bool bHallGate = !bExit && DBDungeon::GetSites().IsValidIndex(SiteIndex) && DBDungeon::GetSites()[SiteIndex].bEchoHall;
		const EDBArtMaterial Frame = bAbyssGate ? EDBArtMaterial::StoneCorrupted : bHallGate ? EDBArtMaterial::StoneTemple : EDBArtMaterial::WoodLacquerRed;
		const bool bVeil = Index == Parts.Num() - 1;
		const EDBArtMaterial Material = bVeil ? (bExit ? EDBArtMaterial::LanternPaper : EDBArtMaterial::DarkBloodVeins) : Frame;
		Parts[Index]->SetMaterial(0, bVeil && VeilMaterial ? static_cast<UMaterialInterface*>(VeilMaterial) : UDBArtMaterialSubsystem::Get(Material));
	}
	if (DBDungeon::GetSites().IsValidIndex(SiteIndex))
	{
		const FDBDungeonSite& Site = DBDungeon::GetSites()[SiteIndex];
		if (DescendTo > 0)
		{
			Label->SetText(FText::Format(LOCTEXT("DescendLabel", "Tiefer hinab - Ebene {0}"), FText::AsNumber(DescendTo)));
		}
		else if ((Site.bAbyss || Site.bEchoHall) && !bExit)
		{
			Label->SetText(FText::FromString(Site.Name));
		}
		else
		{
			Label->SetText(bExit ? LOCTEXT("ExitLabel", "Ausgang") : FText::Format(LOCTEXT("GateLabel", "{0} (Stufe {1})"), FText::FromString(Site.Name), FText::AsNumber(Site.Difficulty)));
		}
	}
}

FText ADBDungeonPortal::GetInteractionText() const
{
	if (DescendTo > 0)
	{
		return FText::Format(LOCTEXT("DescendPrompt", "Hinabsteigen (Ebene {0})"), FText::AsNumber(DescendTo));
	}
	if (!bExit && DBDungeon::GetSites().IsValidIndex(SiteIndex) && DBDungeon::GetSites()[SiteIndex].bAbyss)
	{
		return LOCTEXT("AbyssPrompt", "In den Abgrund steigen");
	}
	if (!bExit && DBDungeon::GetSites().IsValidIndex(SiteIndex) && DBDungeon::GetSites()[SiteIndex].bEchoHall)
	{
		return LOCTEXT("EchoHallPrompt", "Die Halle der Echos betreten");
	}
	return bExit ? LOCTEXT("Leave", "Dungeon verlassen") : LOCTEXT("EnterPrompt", "Dungeon betreten");
}

void ADBDungeonPortal::Interact(APlayerController* User)
{
	if (!HasAuthority())
	{
		return;
	}
	if (DescendTo > 0)
	{
		ADBDungeonInstance::Descend(User);
	}
	else if (bExit)
	{
		ADBDungeonInstance::Leave(User, SiteIndex);
	}
	else
	{
		ADBDungeonInstance::Enter(User, SiteIndex);
	}
}

void ADBDungeonPortal::SpawnEntrances(UWorld* World)
{
	const TArray<FDBDungeonSite>& Sites = DBDungeon::GetSites();
	for (int32 Index = 0; Index < Sites.Num(); ++Index)
	{
		bool bExists = false;
		for (TActorIterator<ADBDungeonPortal> It(World); It && !bExists; ++It)
		{
			bExists = It->SiteIndex == Index && !It->bExit;
		}
		if (bExists)
		{
			continue;
		}
		const FVector2D At = Sites[Index].Entrance;
		const FVector Location(At.X * 100.0, At.Y * 100.0, DBRealm::SampleHeight(At.X, At.Y) * 100.0);
		// The gate opens towards its region's heart, where travellers come from.
		const FDBRealmRegion* Region = DBRealm::GetRegions().FindByPredicate([&](const FDBRealmRegion& Candidate) { return Candidate.RegionId == Sites[Index].RegionId; });
		const FVector2D Towards = Region ? (Region->Center - At).GetSafeNormal() : FVector2D(1.0, 0.0);
		const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Towards.Y, Towards.X)));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADBDungeonPortal* Gate = World->SpawnActor<ADBDungeonPortal>(ADBDungeonPortal::StaticClass(), Location, FRotator(0.f, Yaw, 0.f), Params))
		{
			Gate->Setup(Index, false);
		}
	}
}

// ---- Trap ----------------------------------------------------------------------------------------------------

ADBDungeonTrap::ADBDungeonTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	RootComponent = Plate;
	Plate->SetStaticMesh(Cube.Object);
	Plate->SetRelativeScale3D(FVector(5.4f, 5.4f, 0.12f));
	Plate->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Plate->SetCanEverAffectNavigation(false);
}

void ADBDungeonTrap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBDungeonTrap, PhaseOffset);
}

void ADBDungeonTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Time = (GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + PhaseOffset;
	const double Phase = FMath::Fmod(Time, static_cast<double>(CycleSeconds));
	const int32 State = Phase < 1.2 ? 0 : Phase < 2.2 ? 1 : 2;
	if (State != VisualState)
	{
		VisualState = State;
		Plate->SetMaterial(0, UDBArtMaterialSubsystem::Get(State == 0 ? EDBArtMaterial::StoneCorrupted : State == 1 ? EDBArtMaterial::DarkBloodVeins : EDBArtMaterial::BloodRiver));
	}
	if (!HasAuthority() || State != 2)
	{
		return;
	}
	// One burst per cycle: everyone standing on the vent burns.
	const int32 Cycle = static_cast<int32>(Time / CycleSeconds);
	if (Cycle == LastBurstCycle)
	{
		return;
	}
	LastBurstCycle = Cycle;
	const FVector Center = GetActorLocation();
	if (const AGameStateBase* Players = GetWorld()->GetGameState())
	{
		for (const APlayerState* PlayerState : Players->PlayerArray)
		{
			const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
			if (!Pawn)
			{
				continue;
			}
			const FVector Offset = Pawn->GetActorLocation() - Center;
			if (FMath::Abs(Offset.X) < DBDungeon::CellSize * 0.5f && FMath::Abs(Offset.Y) < DBDungeon::CellSize * 0.5f && Offset.Z > -50.f && Offset.Z < 300.f)
			{
				if (UAbilitySystemComponent* ASC = GetPlayerAbilitySystem(Pawn))
				{
					ApplyTrapDamage(ASC, ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()) * DamageShare);
					++BurstHits;
					UE_LOG(LogDarkBlood, Log, TEXT("Fire vent burns %s"), *GetNameSafe(Pawn));
				}
			}
		}
	}
}

// ---- Shrine --------------------------------------------------------------------------------------------------

ADBDungeonShrine::ADBDungeonShrine()
{
	bReplicates = true;
	USceneComponent* ShrineRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = ShrineRoot;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Parts.Add(MakePart(this, ShrineRoot, TEXT("Base"), Cube.Object, FVector(0.f, 0.f, 40.f), FVector(1.2f, 1.2f, 0.8f)));
	Parts.Add(MakePart(this, ShrineRoot, TEXT("Pillar"), Cube.Object, FVector(0.f, 0.f, 120.f), FVector(0.5f, 0.5f, 1.f)));
	Parts.Add(MakePart(this, ShrineRoot, TEXT("Flame"), Sphere.Object, FVector(0.f, 0.f, 200.f), FVector(0.45f), FRotator::ZeroRotator, false));
}

void ADBDungeonShrine::BeginPlay()
{
	Super::BeginPlay();
	// The primitive pillar gives way to a stone lantern from the art library; the plinth keeps its collision.
	const TArray<DBModels::FPlacedPart> Lantern = DBModels::GetNormalizedParts(TEXT("lantern_stone"), 230.f);
	if (Lantern.Num() > 0 && Parts.Num() >= 3)
	{
		Parts[1]->SetVisibility(false);
		Parts[1]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Parts[0]->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.3f));
		Parts[0]->SetRelativeLocation(FVector(0.f, 0.f, 15.f));
		Parts[0]->SetMaterial(0, UDBArtMaterialSubsystem::Get(EDBArtMaterial::StoneRuin));
		for (const DBModels::FPlacedPart& Piece : Lantern)
		{
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			Part->SetupAttachment(RootComponent);
			Part->SetStaticMesh(Piece.Mesh);
			Part->SetRelativeTransform(Piece.Local * FTransform(FVector(0.f, 0.f, 30.f)));
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false);
			Part->RegisterComponent();
			LanternParts.Add(Part);
		}
		// The flame sits in the lantern's light chamber, small and spirit-blue.
		Parts[2]->SetRelativeLocation(FVector(0.f, 0.f, 30.f + 230.f * 0.62f));
		Parts[2]->SetRelativeScale3D(FVector(0.28f));
	}
	if (UMaterialInterface* Barrier = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Master/M_DB_BloodBarrier.M_DB_BloodBarrier"), nullptr,
			LOAD_NoWarn | LOAD_Quiet); Barrier && Parts.Num() >= 3)
	{
		UMaterialInstanceDynamic* Flame = UMaterialInstanceDynamic::Create(Barrier, this);
		Flame->SetVectorParameterValue(TEXT("BarrierColor"), FLinearColor(0.5f, 0.78f, 1.f));
		Flame->SetScalarParameterValue(TEXT("Intensity"), 3.f);
		Flame->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
		Parts[2]->SetMaterial(0, Flame);
		Parts[2]->SetCastShadow(false);
	}
}

FText ADBDungeonShrine::GetInteractionText() const
{
	return LOCTEXT("Pray", "Am Schrein rasten");
}

void ADBDungeonShrine::Interact(APlayerController* User)
{
	APawn* Pawn = User ? User->GetPawn() : nullptr;
	UAbilitySystemComponent* ASC = GetPlayerAbilitySystem(Pawn);
	if (!HasAuthority() || !ASC)
	{
		return;
	}
	auto Apply = [ASC](TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& Tag, float Magnitude)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(Tag, Magnitude);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	};
	Apply(UDBHealEffect::StaticClass(), DBTags::SetByCaller_Magnitude, ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()));
	Apply(UDBStaminaCostEffect::StaticClass(), DBTags::SetByCaller_StaminaCost, ASC->GetNumericAttribute(UDBAttributeSet::GetMaxStaminaAttribute()));
	Apply(UDBManaChangeEffect::StaticClass(), DBTags::SetByCaller_Magnitude, ASC->GetNumericAttribute(UDBAttributeSet::GetMaxManaAttribute()));
	if (ADBPlayerController* Controller = Cast<ADBPlayerController>(User))
	{
		Controller->ClientShowNotification(LOCTEXT("Rested", "Der Schrein heilt dich."));
	}
	UE_LOG(LogDarkBlood, Display, TEXT("Shrine heals %s"), *GetNameSafe(Pawn));
}

#undef LOCTEXT_NAMESPACE
