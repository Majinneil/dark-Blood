#include "Boss/DBBoss.h"

#include "AI/DBMeleeAIComponent.h"
#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBMeleeAttackAbility.h"
#include "AbilitySystemGlobals.h"
#include "Art/DBArtMaterials.h"
#include "Art/DBModelLibrary.h"
#include "Boss/DBBossDefinition.h"
#include "Character/DBLesserDemon.h"
#include "Combat/DBCombatStatics.h"
#include "Combat/DBProjectile.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Data/DBGameDataSubsystem.h"
#include "Data/DBRegionDefinition.h"
#include "Framework/DBGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Visual/DBCharacterVisualComponent.h"
#include "World/DBRealmVegetation.h"
#include "World/DBWorldStateComponent.h"

#include "DarkBloodRules/Boss.h"
#include "DarkBloodRules/WorldState.h"

#define LOCTEXT_NAMESPACE "DarkBloodBoss"

namespace R = DarkBlood::Rules;

namespace
{
	/** Players count as "in the fight" within this distance of the boss (cm). */
	constexpr float FightRadius = 4500.f;
	constexpr int32 MaxAdds = 6;

	UAbilitySystemComponent* GetASC(const AActor* Actor)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	}

	UDBWorldStateComponent* GetWorldState(const UWorld* World)
	{
		const ADBGameState* GameState = World ? World->GetGameState<ADBGameState>() : nullptr;
		return GameState ? GameState->GetWorldState() : nullptr;
	}

	FText MakeBossName(const UDBBossDefinition& Boss)
	{
		return FText::Format(LOCTEXT("BossName", "{0}, {1}"), Boss.DisplayName, Boss.Title);
	}
}

// ---- Boss ----------------------------------------------------------------------------------------------------

ADBBossCharacter::ADBBossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EnemyId = TEXT("Boss");
	AttackAbility = UDBAbility_DemonClaw::StaticClass();
	RespawnSeconds = 15.f; // corpse lifetime
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 430.f;
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
	PlaceholderBody->SetRelativeScale3D(FVector(0.95f, 0.95f, 1.9f));

	MeleeAI = CreateDefaultSubobject<UDBMeleeAIComponent>(TEXT("MeleeAI"));
	Visuals->SetProfileId(TEXT("CV_Enemy_LesserDemon"));

	Aura = CreateDefaultSubobject<UPointLightComponent>(TEXT("Aura"));
	Aura->SetupAttachment(GetCapsuleComponent());
	Aura->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Aura->SetIntensity(6000.f);
	Aura->SetAttenuationRadius(700.f);
	Aura->SetCastShadows(false);
}

void ADBBossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBBossCharacter, BossId);
	DOREPLIFETIME(ADBBossCharacter, Phase);
}

ADBBossCharacter* ADBBossCharacter::SpawnBoss(UWorld* World, const UDBBossDefinition* Definition, const FVector& Location, const FRotator& Rotation,
	int32 InPlayerCount, float StatMultiplier, ADBBossArena* InArena)
{
	if (!World || !Definition)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.bDeferConstruction = true;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADBBossCharacter* Boss = World->SpawnActor<ADBBossCharacter>(ADBBossCharacter::StaticClass(), Location, Rotation, Params);
	if (Boss)
	{
		Boss->Setup(Definition, InPlayerCount, StatMultiplier, InArena);
		Boss->FinishSpawning(FTransform(Rotation, Location));
		UE_LOG(LogDBCombat, Display, TEXT("Boss %s spawned: %d players, health %.0f"), *Definition->DisplayName.ToString(), InPlayerCount, Boss->MaxHealth);
	}
	return Boss;
}

void ADBBossCharacter::Setup(const UDBBossDefinition* InDefinition, int32 InPlayerCount, float StatMultiplier, ADBBossArena* InArena)
{
	BossId = InDefinition->BossId;
	EnemyId = InDefinition->BossId;
	DisplayName = InDefinition->DisplayName;
	Level = InDefinition->Level;
	PlayerCount = FMath::Max(1, InPlayerCount);
	const R::FBossScaling Scaling = R::GetBossScaling(PlayerCount);
	MaxHealth = InDefinition->MaxHealth * StatMultiplier * Scaling.HealthMultiplier;
	MaxPoise = InDefinition->MaxPoise * StatMultiplier;
	Armor = InDefinition->Armor * StatMultiplier;
	AttackPower = InDefinition->AttackPower * StatMultiplier;
	BaseAttackPower = AttackPower;
	XpReward = FMath::RoundToInt(InDefinition->XpReward * StatMultiplier);
	LootTableId = InDefinition->LootTableId;
	Arena = InArena;
}

const UDBBossDefinition* ADBBossCharacter::GetDefinition() const
{
	return DBBosses::Find(BossId);
}

void ADBBossCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
}

void ADBBossCharacter::OnRep_Boss()
{
	ApplyLook();
}

void ADBBossCharacter::ApplyLook()
{
	const UDBBossDefinition* Definition = GetDefinition();
	if (!Definition || !Definition->Phases.IsValidIndex(Phase))
	{
		return;
	}
	SetActorScale3D(FVector(Definition->Phases[Phase].Scale));
	Aura->SetLightColor(Definition->Color);
	Aura->SetIntensity(5000.f + Phase * 4000.f);
	Nameplate->SetTextRenderColor(Definition->Color.ToFColor(true));
	if (!HasAuthority())
	{
		UE_LOG(LogDBCombat, Log, TEXT("Boss %s replicated: phase %d, scale %.2f"), *BossId.ToString(), Phase + 1, GetActorScale3D().X);
	}
}

float ADBBossCharacter::GetHealthFraction() const
{
	const float Max = AbilitySystem ? AbilitySystem->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()) : 0.f;
	return Max > 0.f ? AbilitySystem->GetNumericAttribute(UDBAttributeSet::GetHealthAttribute()) / Max : 0.f;
}

int32 ADBBossCharacter::CountLivingAdds() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ADBEnemyCharacter>& Add : Adds)
	{
		Count += Add.IsValid() && !Add->IsDead() ? 1 : 0;
	}
	return Count;
}

TArray<APawn*> ADBBossCharacter::GetPlayersInFight(float Radius) const
{
	TArray<APawn*> Players;
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (const APlayerState* Entry : GameState->PlayerArray)
		{
			APawn* Pawn = Entry ? Entry->GetPawn() : nullptr;
			const ADBCharacterBase* Character = Cast<ADBCharacterBase>(Pawn);
			if (Character && !Character->IsDead() && FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) < Radius)
			{
				Players.Add(Pawn);
			}
		}
	}
	return Players;
}

FDBHitParams ADBBossCharacter::MakeHit(float Damage, float Poise, bool bKnockdown) const
{
	FDBHitParams Hit;
	Hit.BaseDamage = Damage;
	Hit.PoiseDamage = Poise;
	const UDBBossDefinition* Definition = GetDefinition();
	Hit.DamageType = Definition ? Definition->DamageType : DBTags::Damage_Type_Physical;
	Hit.bKnockdown = bKnockdown;
	Hit.AttackerLevel = Level;
	return Hit;
}

void ADBBossCharacter::NotifyPlayers(const FText& Text, float Radius) const
{
	for (const APawn* Player : GetPlayersInFight(Radius))
	{
		if (ADBPlayerController* PC = Cast<ADBPlayerController>(Player->GetController()))
		{
			PC->ClientShowNotification(Text);
		}
	}
}

void ADBBossCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UDBBossDefinition* Definition = GetDefinition();
	if (!HasAuthority() || IsDead() || !Definition)
	{
		return;
	}
	const TArray<APawn*> Players = GetPlayersInFight(FightRadius);
	if (Players.Num() == 0)
	{
		return;
	}
	FightSeconds += DeltaSeconds;

	// Phases at health thresholds (never back); the demon king changes form.
	const TArray<float> Thresholds = Definition->GetPhaseThresholds();
	const int32 NewPhase = R::EvaluateBossPhase(GetHealthFraction(), Phase, std::vector<float>(Thresholds.GetData(), Thresholds.GetData() + Thresholds.Num()));
	if (NewPhase > Phase)
	{
		EnterPhase(NewPhase);
	}
	InvulnerableSeconds = FMath::Max(0.f, InvulnerableSeconds - DeltaSeconds);

	// Long fights enrage.
	const float Enraged = BaseAttackPower * R::GetEnrageMultiplier(FightSeconds, Definition->EnrageAfterSeconds);
	if (!FMath::IsNearlyEqual(Enraged, AbilitySystem->GetNumericAttribute(UDBAttributeSet::GetAttackPowerAttribute())))
	{
		AbilitySystem->SetNumericAttributeBase(UDBAttributeSet::GetAttackPowerAttribute(), Enraged);
		if (Enraged > BaseAttackPower * 1.01f)
		{
			UE_LOG(LogDBCombat, Display, TEXT("Boss %s enrages (x%.2f)"), *Definition->DisplayName.ToString(), Enraged / BaseAttackPower);
		}
	}

	// A charge hits everyone it passes.
	if (ChargeSeconds > 0.f)
	{
		ChargeSeconds -= DeltaSeconds;
		const float Reach = 140.f * GetActorScale3D().X + 80.f;
		for (APawn* Player : Players)
		{
			if (!ChargeHits.Contains(Player) && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) < Reach)
			{
				ChargeHits.Add(Player);
				DBCombat::ApplyHit(AbilitySystem, GetASC(Player), MakeHit(AttackPower * 1.1f + 35.f, 50.f, true));
			}
		}
	}

	SpecialCooldown -= DeltaSeconds;
	if (SpecialCooldown <= 0.f && InvulnerableSeconds <= 0.f)
	{
		UseSpecialAttack();
		const R::FBossScaling Scaling = R::GetBossScaling(PlayerCount);
		SpecialCooldown = FMath::Max(2.5f, (8.f - 1.5f * Phase) * Scaling.CooldownMultiplier);
	}
}

void ADBBossCharacter::EnterPhase(int32 NewPhase)
{
	const UDBBossDefinition* Definition = GetDefinition();
	Phase = NewPhase;
	ApplyLook();
	InvulnerableSeconds = 2.f;
	if (UDBAbilitySystemComponent* ASC = Cast<UDBAbilitySystemComponent>(AbilitySystem))
	{
		ASC->AddTimedLooseTag(DBTags::State_Invulnerable, InvulnerableSeconds);
	}
	SpecialCooldown = 2.5f;
	if (Definition && Definition->Phases.IsValidIndex(Phase))
	{
		NotifyPlayers(FText::Format(LOCTEXT("Phase", "{0}: {1}!"), Definition->DisplayName, Definition->Phases[Phase].Name), FightRadius);
		UE_LOG(LogDBCombat, Display, TEXT("Boss %s enters phase %d (%s)"), *Definition->DisplayName.ToString(), Phase + 1, *Definition->Phases[Phase].Name.ToString());
	}
}

void ADBBossCharacter::UseSpecialAttack()
{
	const UDBBossDefinition* Definition = GetDefinition();
	TArray<APawn*> Players = GetPlayersInFight(FightRadius);
	if (!Definition || Players.Num() == 0)
	{
		return;
	}
	const int32 Mechanics = Definition->GetMechanics(Phase);
	TArray<int32> Options;
	for (const int32 Mechanic : {EDBBossMechanic::Slam, EDBBossMechanic::Charge, EDBBossMechanic::Volley, EDBBossMechanic::Hazard, EDBBossMechanic::Summon})
	{
		if ((Mechanics & Mechanic) && !(Mechanic == EDBBossMechanic::Summon && CountLivingAdds() >= MaxAdds))
		{
			Options.Add(Mechanic);
		}
	}
	if (Options.Num() == 0)
	{
		return;
	}
	Players.Sort([this](const APawn& A, const APawn& B) { return FVector::DistSquared(A.GetActorLocation(), GetActorLocation()) < FVector::DistSquared(B.GetActorLocation(), GetActorLocation()); });
	APawn* Target = Players[0];
	const R::FBossScaling Scaling = R::GetBossScaling(PlayerCount);
	const int32 Mechanic = Options[FMath::RandRange(0, Options.Num() - 1)];
	const float Scale = GetActorScale3D().X;
	const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const TCHAR* Name = TEXT("?");
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto Telegraph = [&](const FVector& At, float Radius, float Delay, float Duration, const FDBHitParams& Hit)
	{
		if (ADBBossTelegraph* Warning = GetWorld()->SpawnActor<ADBBossTelegraph>(ADBBossTelegraph::StaticClass(), At, FRotator::ZeroRotator, Params))
		{
			Warning->Arm(this, Radius, Delay, Duration, Hit, Definition->Color);
		}
	};
	switch (Mechanic)
	{
	case EDBBossMechanic::Slam:
		Name = TEXT("Slam");
		Telegraph(GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 5.f), 420.f * Scale, 1.3f, 0.f,
			MakeHit(AttackPower * 1.2f + 45.f, 60.f, true));
		break;
	case EDBBossMechanic::Charge:
		Name = TEXT("Charge");
		SetActorRotation(ToTarget.Rotation());
		LaunchCharacter(ToTarget * 2600.f + FVector(0.f, 0.f, 120.f), true, true);
		ChargeSeconds = 0.6f;
		ChargeHits.Reset();
		break;
	case EDBBossMechanic::Volley:
	{
		Name = TEXT("Volley");
		const int32 Count = 3 + Phase;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Spread = (Index - (Count - 1) * 0.5f) * 11.f;
			const FRotator Aim = (Target->GetActorLocation() - GetActorLocation()).Rotation() + FRotator(0.f, Spread, 0.f);
			const FVector Muzzle = GetActorLocation() + Aim.Vector() * (90.f * Scale) + FVector(0.f, 0.f, 40.f * Scale);
			if (ADBProjectile* Bolt = GetWorld()->SpawnActor<ADBProjectile>(ADBProjectile::StaticClass(), Muzzle, Aim, Params))
			{
				Bolt->Launch(this, MakeHit(AttackPower * 0.6f + 28.f, 15.f, false), 1900.f, 0, Definition->Color);
			}
		}
		break;
	}
	case EDBBossMechanic::Hazard:
	{
		Name = TEXT("Hazard");
		// Three or more players: a zone under everyone.
		TArray<APawn*> Targets = Scaling.bAreaPressure ? Players : TArray<APawn*>{Target};
		for (const APawn* Victim : Targets)
		{
			const float HalfHeight = Victim->GetSimpleCollisionHalfHeight();
			Telegraph(Victim->GetActorLocation() - FVector(0.f, 0.f, HalfHeight - 5.f), 380.f, 1.f, 4.f, MakeHit(AttackPower * 0.35f + 14.f, 0.f, false));
		}
		break;
	}
	case EDBBossMechanic::Summon:
	{
		Name = TEXT("Summon");
		const int32 Count = FMath::Min(MaxAdds - CountLivingAdds(), 2 + Scaling.ExtraAdds);
		FActorSpawnParameters AddParams;
		AddParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = 2.f * UE_PI * Index / FMath::Max(1, Count);
			const FVector At = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 450.f;
			if (ADBLesserDemon* Demon = GetWorld()->SpawnActor<ADBLesserDemon>(ADBLesserDemon::StaticClass(), At, FRotator(0.f, Angle * 57.3f + 180.f, 0.f), AddParams))
			{
				Adds.Add(Demon);
			}
		}
		NotifyPlayers(FText::Format(LOCTEXT("Summon", "{0} ruft Daemonen herbei!"), Definition->DisplayName), FightRadius);
		break;
	}
	default:
		break;
	}
	++SpecialAttacks;
	UE_LOG(LogDBCombat, Display, TEXT("Boss %s uses %s (phase %d)"), *Definition->DisplayName.ToString(), Name, Phase + 1);

	// Two or more players: the boss turns to someone else after each special attack.
	if (Scaling.bSplitAttention && Players.Num() > 1)
	{
		MeleeAI->NotifyAttackedBy(Players[FMath::RandRange(1, Players.Num() - 1)]);
	}
}

void ADBBossCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageMagnitude)
{
	if (IsDead() || !HasAuthority())
	{
		return;
	}
	const UDBBossDefinition* Definition = GetDefinition();
	const TArray<APawn*> Fighters = GetPlayersInFight(FightRadius);
	Super::HandleOutOfHealth(DamageInstigator, DamageCauser, DamageMagnitude);
	for (const TWeakObjectPtr<ADBEnemyCharacter>& Add : Adds)
	{
		if (Add.IsValid() && !Add->IsDead())
		{
			Add->Destroy();
		}
	}
	if (!Definition)
	{
		return;
	}
	UDBWorldStateComponent* WorldState = GetWorldState(GetWorld());
	if (WorldState)
	{
		WorldState->NotifyBossDefeated(BossId, Definition->Rank, Definition->RegionId);
	}
	// Everyone who fought shares the victory: skill points, and the XP the killer already got.
	const APlayerState* KillerState = Cast<APlayerState>(DamageInstigator);
	if (const APawn* KillerPawn = Cast<APawn>(DamageInstigator); !KillerState && KillerPawn)
	{
		KillerState = KillerPawn->GetPlayerState();
	}
	for (const APawn* Fighter : Fighters)
	{
		const ADBPlayerState* FighterState = Fighter->GetPlayerState<ADBPlayerState>();
		if (!FighterState || !FighterState->GetProgression())
		{
			continue;
		}
		if (FighterState != KillerState)
		{
			FighterState->GetProgression()->AwardXp(XpReward);
		}
		if (Definition->SkillPoints > 0)
		{
			FighterState->GetProgression()->AwardSkillPoints(Definition->SkillPoints);
		}
	}
	FText Message;
	if (Definition->Rank == EDBBossRank::Vassal)
	{
		const R::FWorldState* State = WorldState ? &WorldState->GetRulesState() : nullptr;
		const R::FRegionState* Region = State ? State->FindRegion(TCHAR_TO_UTF8(*Definition->RegionId.ToString())) : nullptr;
		Message = FText::Format(LOCTEXT("VassalDown", "{0} ist gefallen! {1}/{2} Vasallen. {3}"), MakeBossName(*Definition),
			FText::AsNumber(State ? State->CountDefeatedVassals() : 0), FText::AsNumber(R::NumVassals),
			Region && Region->bVassalDefeated && Region->Kind == R::ERegionKind::VassalRegion ? LOCTEXT("Freed", "Die Region ist befreit.")
			: State && State->IsDemonKingReachable() ? LOCTEXT("ThroneOpen", "Der Weg zum Daemonenkoenig ist frei.")
													  : FText::GetEmpty());
	}
	else if (Definition->Rank == EDBBossRank::MidBoss)
	{
		const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
		const UDBRegionDefinition* Region = Data ? Data->FindRegion(Definition->RegionId) : nullptr;
		Message = FText::Format(LOCTEXT("CommanderDown", "{0} ist gefallen! Das Lager ist zerschlagen, {1} ist umkaempft."), MakeBossName(*Definition),
			Region ? Region->DisplayName : FText::FromName(Definition->RegionId));
	}
	else if (Definition->Rank == EDBBossRank::DemonKing)
	{
		Message = LOCTEXT("KingDown", "Der Daemonenkoenig ist gefallen! Das Dunkle Blut verstummt.");
	}
	else
	{
		Message = FText::Format(LOCTEXT("BossDown", "{0} besiegt!"), Definition->DisplayName);
	}
	for (const APawn* Fighter : Fighters)
	{
		if (ADBPlayerController* PC = Cast<ADBPlayerController>(Fighter->GetController()))
		{
			PC->ClientShowNotification(Message);
		}
	}
	UE_LOG(LogDBCombat, Display, TEXT("Boss %s defeated after %.0f s by %d players (%d special attacks)"), *Definition->DisplayName.ToString(), FightSeconds,
		Fighters.Num(), SpecialAttacks);
}

// ---- Telegraph -----------------------------------------------------------------------------------------------

ADBBossTelegraph::ADBBossTelegraph()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	RootComponent = Disc;
	Disc->SetStaticMesh(Cylinder.Object);
	Disc->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Disc->SetCastShadow(false);
	Disc->SetCanEverAffectNavigation(false);
	Disc->SetRelativeScale3D(FVector(1.f, 1.f, 0.02f));
}

void ADBBossTelegraph::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBBossTelegraph, Radius);
	DOREPLIFETIME(ADBBossTelegraph, Delay);
	DOREPLIFETIME(ADBBossTelegraph, Duration);
	DOREPLIFETIME(ADBBossTelegraph, Color);
}

void ADBBossTelegraph::Arm(ADBBossCharacter* InBoss, float InRadius, float InDelay, float InDuration, const FDBHitParams& InHit, const FLinearColor& InColor)
{
	Boss = InBoss;
	Radius = InRadius;
	Delay = InDelay;
	Duration = InDuration;
	Hit = InHit;
	Color = InColor;
	OnRep_Setup();
}

void ADBBossTelegraph::OnRep_Setup()
{
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Disc->GetMaterial(0));
	if (!Material)
	{
		Material = UMaterialInstanceDynamic::Create(UDBArtMaterialSubsystem::Get(EDBArtMaterial::LanternFire), this);
		Disc->SetMaterial(0, Material);
	}
	Material->SetVectorParameterValue(TEXT("BaseTint"), FLinearColor::Black);
	Material->SetVectorParameterValue(TEXT("TintVariation"), FLinearColor::Black);
	Material->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 0.12f);
}

void ADBBossTelegraph::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// Warning: the ring grows to the full size; then it flares while it hits.
	const float Grow = Delay > 0.f ? FMath::Clamp(Age / Delay, 0.f, 1.f) : 1.f;
	const float Size = Radius * 2.f / 100.f * (0.3f + 0.7f * Grow);
	Disc->SetRelativeScale3D(FVector(Size, Size, 0.02f));
	const bool bNowActive = Age >= Delay;
	if (bNowActive != bActive)
	{
		bActive = bNowActive;
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Disc->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(TEXT("EmissiveColor"), Color * (bActive ? 0.9f : 0.12f));
		}
	}
	if (!HasAuthority() || !bActive)
	{
		return;
	}
	if (Duration <= 0.f)
	{
		if (NextHit == 0.f)
		{
			HitPlayers();
			NextHit = -1.f;
			SetLifeSpan(0.35f);
		}
		return;
	}
	if (Age >= Delay + Duration)
	{
		Destroy();
		return;
	}
	if (Age >= Delay + NextHit)
	{
		HitPlayers();
		NextHit += 1.f;
	}
}

void ADBBossTelegraph::HitPlayers()
{
	UAbilitySystemComponent* Source = Boss.IsValid() && !Boss->IsDead() ? Boss->GetAbilitySystemComponent() : nullptr;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!Source || !GameState)
	{
		return;
	}
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!Pawn || !Cast<ADBCharacterBase>(Pawn) || Cast<ADBCharacterBase>(Pawn)->IsDead())
		{
			continue;
		}
		const FVector Offset = Pawn->GetActorLocation() - GetActorLocation();
		if (Offset.Size2D() < Radius && Offset.Z > -100.f && Offset.Z < 400.f)
		{
			DBCombat::ApplyHit(Source, GetASC(Pawn), Hit);
		}
	}
}

// ---- Arena ---------------------------------------------------------------------------------------------------

ADBBossArena::ADBBossArena()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 650.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(70.f);
}

void ADBBossArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBBossArena, BossId);
	DOREPLIFETIME(ADBBossArena, ArenaState);
}

void ADBBossArena::SetBoss(FName InBossId)
{
	BossId = InBossId;
	OnRep_State();
}

void ADBBossArena::BeginPlay()
{
	Super::BeginPlay();
	OnRep_State();
}

void ADBBossArena::BuildRing()
{
	const UDBBossDefinition* Definition = DBBosses::Find(BossId);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (bRingBuilt || !Definition || !Cube)
	{
		return;
	}
	bRingBuilt = true;
	const float Radius = Definition->ArenaRadius;
	// A stone floor (the ground under the ring is never quite flat); its sides reach down into the slope.
	if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		UStaticMeshComponent* Floor = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Floor->SetMobility(EComponentMobility::Static);
		Floor->SetupAttachment(Root);
		Floor->SetStaticMesh(Cylinder);
		const float Size = (Radius + 150.f) * 2.f / 100.f;
		Floor->SetRelativeLocation(FVector(0.f, 0.f, 25.f - 600.f));
		Floor->SetRelativeScale3D(FVector(Size, Size, 12.f));
		Floor->SetMaterial(0, UDBArtMaterialSubsystem::Get(Definition->Rank == EDBBossRank::DemonKing ? EDBArtMaterial::DarkBloodStone : EDBArtMaterial::StoneTemple));
		Floor->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Floor->SetCanEverAffectNavigation(false);
		Floor->RegisterComponent();
		Posts.Add(Floor);
	}
	const bool bKing = Definition->Rank == EDBBossRank::DemonKing;
	auto AddPart = [this](UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Override)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Part->SetMobility(EComponentMobility::Static);
		Part->SetupAttachment(Root);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeTransform(Transform);
		if (Override)
		{
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Part->SetMaterial(Slot, Override);
			}
		}
		Part->SetCanEverAffectNavigation(false);
		Part->RegisterComponent();
		Posts.Add(Part);
		return Part;
	};
	// An authored model scaled to Height, its bottom on At, turned by ModelRotation (import orientation) and Yaw.
	auto AddModel = [&AddPart](const TArray<const TCHAR*>& Paths, const FRotator& ModelRotation, float Height, const FVector& At, float Yaw,
		UMaterialInterface* Override = nullptr)
	{
		TArray<UStaticMesh*> Meshes;
		FBox Bounds(ForceInit);
		const FTransform Turn(ModelRotation);
		for (const TCHAR* Path : Paths)
		{
			const FString Name(Path);
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(Name + TEXT(".") + FPaths::GetBaseFilename(Name)), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Meshes.Add(Mesh);
				Bounds += Mesh->GetBoundingBox().TransformBy(Turn);
			}
		}
		if (Meshes.IsEmpty() || !Bounds.IsValid || Bounds.GetSize().Z < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float Scale = Height / Bounds.GetSize().Z;
		const FVector Center = Bounds.GetCenter();
		const FTransform Fit(FRotator::ZeroRotator, FVector(-Center.X, -Center.Y, -Bounds.Min.Z) * Scale, FVector(Scale));
		const FTransform Placement(FRotator(0.f, Yaw, 0.f), At);
		for (UStaticMesh* Mesh : Meshes)
		{
			AddPart(Mesh, Turn * Fit * Placement, Override);
		}
		return true;
	};
	const float Top = 25.f; // floor surface
	// Stone lanterns (Sketchfab CC BY) round the ring, unlit on purpose (no extra shadow-casting lights).
	const TArray<DBModels::FPlacedPart> Lantern = DBModels::GetNormalizedParts(TEXT("lantern_stone"), 240.f);
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const float Degrees = 360.f * Index / 12.f + 15.f;
		const FVector At = FRotator(0.f, Degrees, 0.f).Vector() * (Radius - 220.f) + FVector(0.f, 0.f, Top);
		const FTransform Placement(FRotator(0.f, Degrees + 180.f, 0.f), At);
		if (Lantern.IsEmpty())
		{
			AddPart(Cube, FTransform(FRotator::ZeroRotator, At + FVector(0.f, 0.f, 150.f), FVector(0.6f, 0.6f, 3.f)),
				UDBArtMaterialSubsystem::Get(bKing ? EDBArtMaterial::StoneCorrupted : EDBArtMaterial::StoneTemple));
			continue;
		}
		for (const DBModels::FPlacedPart& Part : Lantern)
		{
			AddPart(Part.Mesh, Part.Local * Placement, bKing ? UDBArtMaterialSubsystem::Get(EDBArtMaterial::StoneCorrupted) : nullptr);
		}
	}
	// Two temple guardians (MTSU photogrammetry, CC BY) across from the gate, facing the center.
	for (const float Degrees : {-28.f, 28.f})
	{
		const FVector At = FRotator(0.f, Degrees, 0.f).Vector() * Radius * 0.84f + FVector(0.f, 0.f, Top);
		AddPart(Cube, FTransform(FRotator(0.f, Degrees, 0.f), At + FVector(0.f, 0.f, 30.f), FVector(2.2f, 2.2f, 0.6f)),
			UDBArtMaterialSubsystem::Get(bKing ? EDBArtMaterial::StoneCorrupted : EDBArtMaterial::StoneRuin));
		AddModel({TEXT("/Game/DarkBlood/Art/Fab/Statue_Ibaraki_MTSU/scene/StaticMeshes/scene")}, FRotator::ZeroRotator, bKing ? 420.f : 340.f,
			At + FVector(0.f, 0.f, 60.f), Degrees + 180.f, bKing ? UDBArtMaterialSubsystem::Get(EDBArtMaterial::DarkBloodStone) : nullptr);
	}
	// The gate (Fab torii, CC BY) where the way in crosses the ring (west, where DBBossArena puts players).
	AddModel({TEXT("/Game/DarkBlood/Art/Fab/Torii_Pikas/scene/StaticMeshes/Torri_Gate_Torri_gate_0"), TEXT("/Game/DarkBlood/Art/Fab/Torii_Pikas/scene/StaticMeshes/Torri_Gate_Rope_Gold_0")},
		FRotator(0.f, 90.f, 0.f), 640.f, FVector(-Radius, 0.f, Top), 0.f);

	// The blood barrier: a glowing translucent wall in the boss' color while the fight lasts.
	UMaterialInterface* BarrierMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DarkBlood/Art/Materials/Master/M_DB_BloodBarrier.M_DB_BloodBarrier"),
		nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* WallMaterial = UDBArtMaterialSubsystem::Get(EDBArtMaterial::BloodRiver);
	if (BarrierMaterial)
	{
		UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(BarrierMaterial, this);
		Tinted->SetVectorParameterValue(TEXT("BarrierColor"), Definition->Color);
		WallMaterial = Tinted;
	}
	const int32 Segments = 40;
	const float Length = 2.f * UE_PI * Radius / Segments + 30.f;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float Angle = 2.f * UE_PI * (Index + 0.5f) / Segments;
		UStaticMeshComponent* Wall = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Wall->SetMobility(EComponentMobility::Static);
		Wall->SetupAttachment(Root);
		Wall->SetStaticMesh(Cube);
		Wall->SetRelativeLocation(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Top + 200.f));
		Wall->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f));
		Wall->SetRelativeScale3D(FVector(Length / 100.f, 0.08f, 5.f));
		Wall->SetMaterial(0, WallMaterial);
		Wall->SetCastShadow(false);
		Wall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Wall->RegisterComponent();
		Barrier.Add(Wall);
	}
}

void ADBBossArena::OnRep_State()
{
	BuildRing();
	if (!HasAuthority() && !BossId.IsNone())
	{
		UE_LOG(LogDBCombat, Log, TEXT("Arena %s replicated: state %d, barrier %d"), *BossId.ToString(), static_cast<int32>(ArenaState), Barrier.Num());
	}
	const bool bClosed = ArenaState == EDBArenaState::Fighting;
	for (UStaticMeshComponent* Wall : Barrier)
	{
		Wall->SetVisibility(bClosed);
		Wall->SetCollisionEnabled(bClosed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (const UDBBossDefinition* Definition = DBBosses::Find(BossId))
	{
		Label->SetText(ArenaState == EDBArenaState::Defeated ? FText::Format(LOCTEXT("ArenaDone", "{0} (besiegt)"), Definition->DisplayName) : MakeBossName(*Definition));
		Label->SetTextRenderColor((ArenaState == EDBArenaState::Defeated ? FLinearColor(0.5f, 0.5f, 0.5f) : Definition->Color).ToFColor(true));
	}
}

ADBBossArena* ADBBossArena::Find(const UWorld* World, FName BossId)
{
	for (TActorIterator<ADBBossArena> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (It->BossId == BossId)
		{
			return *It;
		}
	}
	return nullptr;
}

void ADBBossArena::SpawnArenas(UWorld* World)
{
	for (const UDBBossDefinition* Definition : DBBosses::GetAll())
	{
		const FVector Location = DBBosses::GetArenaLocation(*Definition);
		if (Location.IsZero() || Find(World, Definition->BossId))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADBBossArena* Arena = World->SpawnActor<ADBBossArena>(ADBBossArena::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Arena->BossId = Definition->BossId;
			Arena->FinishSpawning(FTransform(Location));
		}
	}
}

TArray<APawn*> ADBBossArena::GetPlayersInside(float Fraction) const
{
	TArray<APawn*> Players;
	const UDBBossDefinition* Definition = DBBosses::Find(BossId);
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!Definition || !GameState)
	{
		return Players;
	}
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		const ADBCharacterBase* Character = Cast<ADBCharacterBase>(Pawn);
		if (Character && !Character->IsDead() && FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation()) < Definition->ArenaRadius * Fraction &&
			FMath::Abs(Pawn->GetActorLocation().Z - GetActorLocation().Z) < 3000.f)
		{
			Players.Add(Pawn);
		}
	}
	return Players;
}

FText ADBBossArena::GetSealReason() const
{
	const UDBBossDefinition* Definition = DBBosses::Find(BossId);
	const UDBWorldStateComponent* WorldState = GetWorldState(GetWorld());
	if (!Definition || !WorldState)
	{
		return FText::GetEmpty();
	}
	const R::FWorldState& State = WorldState->GetRulesState();
	if (Definition->Rank == EDBBossRank::DemonKing && !State.IsDemonKingReachable())
	{
		return LOCTEXT("ThroneSealed", "Der Thron ist versiegelt: Erst muessen Tsukigami und Shirogane fallen.");
	}
	if (Definition->Rank == EDBBossRank::Vassal && Definition->RegionId == FName(TEXT("TheEnd")) && !State.IsFinalRegionOpen())
	{
		return FText::Format(LOCTEXT("EndSealed", "Das Ende ist versiegelt: Befreie erst alle 14 Gebiete ({0}/16 Vasallen)."), FText::AsNumber(State.CountDefeatedVassals()));
	}
	return FText::GetEmpty();
}

void ADBBossArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UDBBossDefinition* ArenaBoss = DBBosses::Find(BossId);
	// Everywhere (vegetation is not replicated): trees and rocks baked into the realm must not stand on the floor.
	// Vegetation cells stream in and out, so keep looking.
	VegetationTimer -= DeltaSeconds;
	if (VegetationTimer <= 0.f && ArenaBoss)
	{
		VegetationTimer = 2.f;
		for (TActorIterator<ADBRealmVegetation> It(GetWorld()); It; ++It)
		{
			if (!ClearedVegetation.Contains(*It))
			{
				ClearedVegetation.Add(*It);
				const int32 Removed = It->RemoveInstancesInCircle(GetActorLocation(), ArenaBoss->ArenaRadius + 300.f);
				UE_CLOG(Removed > 0, LogDBCombat, Log, TEXT("Arena %s: cleared %d trees and rocks"), *BossId.ToString(), Removed);
			}
		}
	}
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
	UpdateTimer = 0.5f;
	const UDBWorldStateComponent* WorldState = GetWorldState(GetWorld());
	const bool bDefeatedInWorld = WorldState && WorldState->GetRulesState().DefeatedBosses.count(TCHAR_TO_UTF8(*BossId.ToString())) > 0;
	switch (ArenaState)
	{
	case EDBArenaState::Idle:
	{
		if (bDefeatedInWorld)
		{
			ArenaState = EDBArenaState::Defeated;
			OnRep_State();
			break;
		}
		const TArray<APawn*> Players = GetPlayersInside(0.75f);
		if (Players.Num() == 0)
		{
			break;
		}
		const FText Seal = GetSealReason();
		if (!Seal.IsEmpty())
		{
			const double Now = GetWorld()->GetTimeSeconds();
			for (APawn* Player : Players)
			{
				float& Last = LastSealNotice.FindOrAdd(Player, -100.f);
				if (Now - Last > 15.0)
				{
					Last = static_cast<float>(Now);
					if (ADBPlayerController* Controller = Cast<ADBPlayerController>(Player->GetController()))
					{
						Controller->ClientShowNotification(Seal);
					}
				}
			}
			break;
		}
		StartFight(Players);
		break;
	}
	case EDBArenaState::Fighting:
		if (!Boss.IsValid() || Boss->IsDead())
		{
			EndFight(Boss.IsValid() || bDefeatedInWorld);
			break;
		}
		EmptySeconds = GetPlayersInside(1.05f).Num() > 0 ? 0.f : EmptySeconds + 0.5f;
		if (EmptySeconds > 5.f)
		{
			EndFight(false);
		}
		break;
	case EDBArenaState::Defeated:
		break;
	}
}

void ADBBossArena::StartFight(const TArray<APawn*>& Players)
{
	const UDBBossDefinition* Definition = DBBosses::Find(BossId);
	if (!Definition)
	{
		return;
	}
	const FVector Facing = (Players[0]->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	Boss = ADBBossCharacter::SpawnBoss(GetWorld(), Definition, GetActorLocation() + FVector(0.f, 0.f, 250.f), Facing.Rotation(), Players.Num(), 1.f, this);
	ArenaState = EDBArenaState::Fighting;
	EmptySeconds = 0.f;
	OnRep_State();
	for (APawn* Player : Players)
	{
		if (ADBPlayerController* Controller = Cast<ADBPlayerController>(Player->GetController()))
		{
			Controller->ClientShowNotification(FText::Format(LOCTEXT("FightStart", "{0} stellt sich dir!"), MakeBossName(*Definition)));
		}
	}
	FString Names;
	for (const APawn* Player : Players)
	{
		Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + (Player->GetPlayerState() ? Player->GetPlayerState()->GetPlayerName() : Player->GetName());
	}
	UE_LOG(LogDBCombat, Display, TEXT("Arena %s: fight starts (%d players: %s)"), *BossId.ToString(), Players.Num(), *Names);
}

void ADBBossArena::EndFight(bool bVictory)
{
	if (!bVictory && Boss.IsValid())
	{
		// Everyone fled or fell: the boss returns to full strength next time.
		Boss->Destroy();
	}
	for (TActorIterator<ADBBossTelegraph> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	Boss.Reset();
	ArenaState = bVictory ? EDBArenaState::Defeated : EDBArenaState::Idle;
	OnRep_State();
	UE_LOG(LogDBCombat, Display, TEXT("Arena %s: %s"), *BossId.ToString(), bVictory ? TEXT("boss defeated") : TEXT("fight reset"));
}

#undef LOCTEXT_NAMESPACE
