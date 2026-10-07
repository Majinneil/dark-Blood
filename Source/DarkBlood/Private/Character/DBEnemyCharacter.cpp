#include "Character/DBEnemyCharacter.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBCombatAbilities.h"
#include "Abilities/DBRegenerationEffect.h"
#include "AI/DBMeleeAIComponent.h"
#include "Combat/DBCombatStatics.h"
#include "DarkBloodRules/Endgame.h"
#include "Framework/DBGameState.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"
#include "Inventory/DBInventoryComponent.h"
#include "Quest/DBQuestSubsystem.h"
#include "TimerManager.h"
#include "World/DBWorldStateComponent.h"

ADBEnemyCharacter::ADBEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Team = EDBTeam::Demons;
	PlaceholderColor = FLinearColor(0.6f, 0.08f, 0.08f);
	bUseControllerRotationYaw = false;
	// Spawned enemies get an AI controller; movement (knockback, gravity) must also run without one.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	AbilitySystem = CreateDefaultSubobject<UDBAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	Attributes = CreateDefaultSubobject<UDBAttributeSet>(TEXT("Attributes"));

	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Nameplate->SetHorizontalAlignment(EHTA_Center);
	Nameplate->SetWorldSize(16.f);
	Nameplate->SetTextRenderColor(FColor(220, 90, 80));
}

void ADBEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	SpawnTransform = GetActorTransform();

	AbilitySystem->InitAbilityActorInfo(this, this);
	CachedAbilitySystem = AbilitySystem;
	BindToAttributeSet(AbilitySystem);
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDBAttributeSet::GetHealthAttribute()).AddUObject(this, &ADBEnemyCharacter::OnHealthChanged);
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDBAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ADBEnemyCharacter::OnHealthChanged);

	if (HasAuthority())
	{
		ApplyEndgameScale();
		InitializeCombatState();
	}
	RefreshNameplate();
}

void ADBEnemyCharacter::SetEndgameScale(const FDBEndgameScale& Scale)
{
	bEndgameScaleSet = true;
	Endgame = Scale;
}

void ADBEnemyCharacter::ApplyEndgameScale()
{
	// Only demons grow with the cycle; training dummies stay what they are.
	if (Team != EDBTeam::Demons || bRespawnInPlace)
	{
		return;
	}
	if (!bEndgameScaleSet)
	{
		const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
		const UDBWorldStateComponent* WorldState = GameState ? GameState->GetWorldState() : nullptr;
		const int32 Cycle = WorldState ? WorldState->GetCycle() : 0;
		if (Cycle <= 0)
		{
			return;
		}
		const DarkBlood::Rules::FEndgameScale Scale = DarkBlood::Rules::GetCycleScale(Cycle);
		SetEndgameScale({Scale.EnemyHealth, Scale.EnemyDamage, Scale.Experience, Scale.RarityBonus, Scale.EnemyLevelBonus});
	}
	MaxHealth *= Endgame.Health;
	MaxPoise *= FMath::Sqrt(Endgame.Health);
	AttackPower *= Endgame.Damage;
	XpReward = FMath::RoundToInt(XpReward * Endgame.Experience);
	Level += Endgame.LevelBonus;
	LootRarityBonus = Endgame.RarityBonus;
	UE_LOG(LogDBCombat, Log, TEXT("%s endgame scale: level %d, health %.0f, attack %.0f, xp %d, loot +%.2f"), *GetCombatDisplayName(), Level, MaxHealth, AttackPower,
		XpReward, LootRarityBonus);
}

void ADBEnemyCharacter::InitializeCombatState()
{
	AbilitySystem->SetNumericAttributeBase(UDBAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystem->SetNumericAttributeBase(UDBAttributeSet::GetHealthAttribute(), MaxHealth);
	AbilitySystem->SetNumericAttributeBase(UDBAttributeSet::GetMaxPoiseAttribute(), MaxPoise);
	AbilitySystem->SetNumericAttributeBase(UDBAttributeSet::GetPoiseAttribute(), MaxPoise);
	AbilitySystem->SetNumericAttributeBase(UDBAttributeSet::GetArmorAttribute(), Armor);
	AbilitySystem->SetNumericAttributeBase(UDBAttributeSet::GetAttackPowerAttribute(), AttackPower);

	TArray<TSubclassOf<UDBGameplayAbility>> ToGrant = Abilities;
	ToGrant.AddUnique(UDBAbility_HitReact::StaticClass());
	if (AttackAbility)
	{
		ToGrant.AddUnique(AttackAbility);
	}
	for (const TSubclassOf<UDBGameplayAbility>& AbilityClass : ToGrant)
	{
		if (AbilityClass)
		{
			AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, Level, INDEX_NONE, this));
		}
	}
	AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UDBRegenerationEffect>(), 1.f, AbilitySystem->MakeEffectContext());
}

UAbilitySystemComponent* ADBEnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ADBEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBEnemyCharacter, DisplayName);
	DOREPLIFETIME(ADBEnemyCharacter, Level);
}

void ADBEnemyCharacter::ConfigureSpawn(int32 InLevel, float StatMultiplier, const FText& InDisplayName, FName InEnemyId, FName InLootTableId)
{
	const float Multiplier = FMath::Max(0.1f, StatMultiplier);
	Level = FMath::Max(1, InLevel);
	MaxHealth *= Multiplier;
	MaxPoise *= FMath::Sqrt(Multiplier);
	Armor *= Multiplier;
	AttackPower *= Multiplier;
	XpReward = FMath::RoundToInt(XpReward * Multiplier);
	if (!InDisplayName.IsEmpty())
	{
		DisplayName = InDisplayName;
	}
	if (!InEnemyId.IsNone())
	{
		EnemyId = InEnemyId;
	}
	if (!InLootTableId.IsNone())
	{
		LootTableId = InLootTableId;
	}
}

FString ADBEnemyCharacter::GetCombatDisplayName() const
{
	return DisplayName.IsEmpty() ? EnemyId.ToString() : DisplayName.ToString();
}

bool ADBEnemyCharacter::AttackTarget(AActor* Target)
{
	if (!HasAuthority() || IsDead() || !AttackAbility || !DBCombat::CanTarget(this, Target))
	{
		return false;
	}
	CurrentTarget = Target;
	return AbilitySystem->TryActivateAbilityByClass(AttackAbility);
}

AActor* ADBEnemyCharacter::FindNearestPlayer(float Radius) const
{
	AActor* Best = nullptr;
	float BestDistanceSq = FMath::Square(Radius);
	for (TActorIterator<ADBCharacterBase> It(GetWorld()); It; ++It)
	{
		const UAbilitySystemComponent* TargetASC = It->GetAbilitySystemComponent();
		const bool bVeiled = TargetASC && TargetASC->HasMatchingGameplayTag(DBTags::State_Veiled);
		if (It->GetTeam() == EDBTeam::Players && !bVeiled && DBCombat::CanTarget(this, *It))
		{
			const float DistanceSq = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
			if (DistanceSq <= BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				Best = *It;
			}
		}
	}
	return Best;
}

void ADBEnemyCharacter::OnAttackedBy(AActor* Attacker)
{
	if (UDBMeleeAIComponent* AI = FindComponentByClass<UDBMeleeAIComponent>())
	{
		AI->NotifyAttackedBy(Attacker);
	}
}

void ADBEnemyCharacter::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, float DamageMagnitude)
{
	if (IsDead() || !HasAuthority())
	{
		return;
	}
	Super::HandleOutOfHealth(DamageInstigator, DamageCauser, DamageMagnitude);

	// Kill credit: the instigator of player damage is the player's PlayerState.
	ADBPlayerState* Killer = Cast<ADBPlayerState>(DamageInstigator);
	if (!Killer)
	{
		if (const APawn* KillerPawn = Cast<APawn>(DamageCauser))
		{
			Killer = KillerPawn->GetPlayerState<ADBPlayerState>();
		}
	}
	UE_LOG(LogDBCombat, Log, TEXT("%s defeated by %s"), *GetCombatDisplayName(), Killer ? *Killer->GetPlayerName() : TEXT("?"));
	if (Killer)
	{
		Killer->GetProgression()->AwardXp(XpReward);
	}
	if (!LootTableId.IsNone())
	{
		for (TActorIterator<ADBCharacterBase> It(GetWorld()); It; ++It)
		{
			ADBPlayerState* Looter = It->GetTeam() == EDBTeam::Players ? It->GetPlayerState<ADBPlayerState>() : nullptr;
			if (Looter && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 6000.f)
			{
				Looter->GetInventory()->GrantLootTable(LootTableId, GetCombatDisplayName(), LootRarityBonus);
			}
		}
	}
	if (UDBQuestSubsystem* Quests = UDBQuestSubsystem::Get(this); Quests && !EnemyId.IsNone())
	{
		Quests->ReportEvent(EDBObjectiveKind::Kill, EnemyId, 1, Killer);
	}

	if (bRespawnInPlace)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &ADBEnemyCharacter::RespawnInPlace, RespawnSeconds, false);
	}
	else
	{
		SetLifeSpan(RespawnSeconds);
	}
}

void ADBEnemyCharacter::RespawnInPlace()
{
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	CurrentTarget.Reset();
	Revive();
	UE_LOG(LogDBCombat, Log, TEXT("%s respawned"), *GetCombatDisplayName());
}

void ADBEnemyCharacter::PlayDeathPresentation()
{
	Super::PlayDeathPresentation();
	SetPlaceholderVisible(false);
	RefreshNameplate();
}

void ADBEnemyCharacter::PlayRevivePresentation()
{
	Super::PlayRevivePresentation();
	SetPlaceholderVisible(true);
	RefreshNameplate();
}

void ADBEnemyCharacter::OnHealthChanged(const FOnAttributeChangeData& /*Change*/)
{
	RefreshNameplate();
}

void ADBEnemyCharacter::RefreshNameplate()
{
	const float Health = AbilitySystem->GetNumericAttribute(UDBAttributeSet::GetHealthAttribute());
	const float Max = AbilitySystem->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute());
	Nameplate->SetText(FText::FromString(IsDead() ? FString::Printf(TEXT("%s (besiegt)"), *GetCombatDisplayName())
												   : FString::Printf(TEXT("%s  %.0f/%.0f"), *GetCombatDisplayName(), Health, Max)));
}

void ADBEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = Camera->GetCameraLocation() - Nameplate->GetComponentLocation();
		Nameplate->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
	}
}
