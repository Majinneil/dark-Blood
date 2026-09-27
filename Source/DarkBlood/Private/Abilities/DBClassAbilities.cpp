#include "Abilities/DBClassAbilities.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBCombatEffects.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/DBCharacterBase.h"
#include "Character/DBPlayerCharacter.h"
#include "Combat/DBCombatStatics.h"
#include "Combat/DBLockOnComponent.h"
#include "Combat/DBProjectile.h"
#include "Combat/DBWardingCircle.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Player/DBPlayerController.h"

#include "DarkBloodRules/Combat.h"

namespace R = DarkBlood::Rules;

namespace
{
	/** Server: hostile characters within Radius in front of Self (half angle in degrees). */
	TArray<ADBCharacterBase*> FindTargetsInArc(const ADBCharacterBase* Self, float Radius, float HalfAngle)
	{
		TArray<ADBCharacterBase*> Result;
		for (TActorIterator<ADBCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (!DBCombat::CanTarget(Self, *It))
			{
				continue;
			}
			const FVector Local = Self->GetActorTransform().InverseTransformPositionNoScale(It->GetActorLocation());
			if (FMath::Abs(Local.Z) < 250.f && R::IsInsideAttackArc(Local.X, Local.Y, Radius, HalfAngle, 40.f))
			{
				Result.Add(*It);
			}
		}
		return Result;
	}

	/** Target the player is focused on (lock-on) or the nearest enemy roughly ahead. */
	AActor* FindFocusOrNearest(const ADBCharacterBase* Self, float Range)
	{
		if (AActor* Focus = Self->GetCombatFocusTarget(); Focus && DBCombat::CanTarget(Self, Focus))
		{
			return Focus;
		}
		AActor* Best = nullptr;
		float BestScore = TNumericLimits<float>::Max();
		for (TActorIterator<ADBCharacterBase> It(Self->GetWorld()); It; ++It)
		{
			if (!DBCombat::CanTarget(Self, *It))
			{
				continue;
			}
			const FVector To = It->GetActorLocation() - Self->GetActorLocation();
			const float Distance = To.Size();
			const float Facing = FVector::DotProduct(Self->GetActorForwardVector(), To.GetSafeNormal2D());
			if (Distance <= Range && Facing > 0.2f)
			{
				const float Score = Distance * (2.f - Facing);
				if (Score < BestScore)
				{
					BestScore = Score;
					Best = *It;
				}
			}
		}
		return Best;
	}

	void Notify(const UGameplayAbility* Ability, const FText& Text)
	{
		const APawn* Pawn = Cast<APawn>(Ability->GetAvatarActorFromActorInfo());
		if (ADBPlayerController* Controller = Pawn ? Pawn->GetController<ADBPlayerController>() : nullptr)
		{
			Controller->ClientShowNotification(Text);
		}
	}
}

// ==== Krieger ========================================================================================

UDBAbility_IronStance::UDBAbility_IronStance()
{
	DisplayName = FText::FromString(TEXT("Eiserne Haltung"));
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Class));
	ActivationOwnedTags.AddTag(DBTags::State_IronStance);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	StaminaCost = 10.f;
}

void UDBAbility_IronStance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Character = GetDBCharacter();
	UDBAbilitySystemComponent* ASC = GetDBAbilitySystem();
	if (!Character || !ASC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const int32 Rank = FMath::Max(1, GetSkillRank(DBSkillNodes::IronStance));
	if (Rank >= 2)
	{
		ASC->AddLooseGameplayTag(DBTags::State_IronStanceUnshakable);
		bAddedUnshakable = true;
	}
	if (HasAuthority(&ActivationInfo))
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDBIronStanceEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_Magnitude, 40.f);
		StanceEffect = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	BaseWalkSpeed = Movement->MaxWalkSpeed;
	Movement->MaxWalkSpeed = BaseWalkSpeed * 0.8f;
	UE_LOG(LogDBCombat, Log, TEXT("%s: Eiserne Haltung an (Rang %d)"), *DBCombat::GetCombatName(Character), Rank);
}

void UDBAbility_IronStance::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Pressing again leaves the stance.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDBAbility_IronStance::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystem())
	{
		if (StanceEffect.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(StanceEffect);
			StanceEffect.Invalidate();
		}
		if (bAddedUnshakable)
		{
			ASC->RemoveLooseGameplayTag(DBTags::State_IronStanceUnshakable);
			bAddedUnshakable = false;
		}
	}
	if (ADBCharacterBase* Character = GetDBCharacter(); Character && BaseWalkSpeed > 0.f)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
		BaseWalkSpeed = 0.f;
		UE_LOG(LogDBCombat, Log, TEXT("%s: Eiserne Haltung aus"), *DBCombat::GetCombatName(Character));
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UDBAbility_Breakthrough::UDBAbility_Breakthrough()
{
	DisplayName = FText::FromString(TEXT("Durchbruch"));
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Class));
	ActivationOwnedTags.AddTag(DBTags::State_Dodging); // owns the body during the charge
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Attack);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Block);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	ActivationBlockedTags.AddTag(DBTags::State_Dodging);
	StaminaCost = 30.f;
	CooldownSeconds = 6.f;
	CooldownTag = DBTags::Cooldown_Breakthrough;
}

float UDBAbility_Breakthrough::GetCooldownSeconds() const
{
	return GetSkillRank(DBSkillNodes::Breakthrough) >= 2 ? 4.f : CooldownSeconds;
}

void UDBAbility_Breakthrough::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Character = GetDBCharacter();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (const AActor* Focus = Character->GetCombatFocusTarget())
	{
		const FVector To = Focus->GetActorLocation() - Character->GetActorLocation();
		Character->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	}
	DashStart = Character->GetActorLocation();
	constexpr float Distance = 600.f;
	constexpr float Seconds = 0.35f;
	if (UAbilityTask_ApplyRootMotionConstantForce* Dash = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this,
			TEXT("Breakthrough"), Character->GetActorForwardVector(), Distance / Seconds, Seconds, false, nullptr,
			ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 100.f, true))
	{
		Dash->ReadyForActivation();
	}
	UE_LOG(LogDBCombat, Log, TEXT("%s: Durchbruch"), *DBCombat::GetCombatName(Character));
	if (UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, Seconds))
	{
		Wait->OnFinish.AddDynamic(this, &UDBAbility_Breakthrough::OnDashFinished);
		Wait->ReadyForActivation();
	}
}

void UDBAbility_Breakthrough::OnDashFinished()
{
	ADBCharacterBase* Character = GetDBCharacter();
	if (Character && HasAuthority(&CurrentActivationInfo))
	{
		// Everything along the charge path is hit once.
		TArray<FOverlapResult> Overlaps;
		const FVector End = Character->GetActorLocation();
		const FVector Mid = (DashStart + End) * 0.5f;
		const float HalfLength = FVector::Dist(DashStart, End) * 0.5f + 150.f;
		const FQuat Rotation = FRotationMatrix::MakeFromX(End - DashStart).ToQuat();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DBBreakthrough), false, Character);
		Character->GetWorld()->OverlapMultiByObjectType(Overlaps, Mid, Rotation, FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeBox(FVector(HalfLength, 150.f, 150.f)), Params);
		TSet<AActor*> Hit;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			ADBCharacterBase* Target = Cast<ADBCharacterBase>(Overlap.GetActor());
			if (Target && !Hit.Contains(Target) && DBCombat::CanTarget(Character, Target))
			{
				Hit.Add(Target);
				FDBHitParams Params2;
				Params2.BaseDamage = 30.f;
				Params2.PoiseDamage = 50.f;
				Params2.bKnockdown = true;
				Params2.AttackerLevel = Character->GetCombatLevel();
				DBCombat::ApplyHit(GetAbilitySystemComponentFromActorInfo(), Target->GetAbilitySystemComponent(), Params2);
			}
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ==== Schattenlaeufer ================================================================================

UDBAbility_ShadowMark::UDBAbility_ShadowMark()
{
	DisplayName = FText::FromString(TEXT("Schattenmal"));
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Class));
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	CooldownSeconds = 6.f;
	CooldownTag = DBTags::Cooldown_ShadowMark;
	ManaCost = 10.f;
}

AActor* UDBAbility_ShadowMark::FindMarkTarget() const
{
	const ADBCharacterBase* Self = GetDBCharacter();
	return Self ? FindFocusOrNearest(Self, 1800.f) : nullptr;
}

void UDBAbility_ShadowMark::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Self = GetDBCharacter();
	UDBAbilitySystemComponent* ASC = GetDBAbilitySystem();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Self || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Marked = MarkedTarget.Get();
	const bool bMarkValid = Marked && Now < MarkExpiresAt && DBCombat::CanTarget(Self, Marked) &&
		FVector::Dist(Marked->GetActorLocation(), Self->GetActorLocation()) < 2500.f;

	if (!bMarkValid)
	{
		// First use: place the mark (cheap, no cooldown).
		AActor* Target = FindMarkTarget();
		if (!Target || !CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		MarkedTarget = Target;
		MarkExpiresAt = Now + 8.0;
		UE_LOG(LogDBCombat, Log, TEXT("%s: Schattenmal auf %s"), *DBCombat::GetCombatName(Self), *DBCombat::GetCombatName(Target));
		Notify(this, FText::Format(NSLOCTEXT("DarkBlood", "ShadowMarked", "Schattenmal: {0}"), FText::FromString(DBCombat::GetCombatName(Target))));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Second use: step out of the shadow behind the marked enemy.
	const FVector Behind = Marked->GetActorLocation() - Marked->GetActorForwardVector() * 150.f;
	const FRotator Facing(0.f, (Marked->GetActorLocation() - Behind).Rotation().Yaw, 0.f);
	Self->TeleportTo(Behind, Facing);
	if (AController* Controller = Self->GetController())
	{
		Controller->ClientSetRotation(Facing);
	}
	ASC->AddTimedLooseTag(DBTags::State_Invulnerable, 0.3f);
	ASC->AddTimedLooseTag(DBTags::State_ShadowEmpowered, 2.f);
	ASC->AddTimedLooseTag(DBTags::Cooldown_ShadowMark, CooldownSeconds);
	if (GetSkillRank(DBSkillNodes::ShadowMark) >= 2)
	{
		FDBHitParams Arrival;
		Arrival.BaseDamage = 15.f;
		Arrival.PoiseDamage = 20.f;
		Arrival.DamageType = DBTags::Damage_Type_Shadow;
		Arrival.AttackerLevel = Self->GetCombatLevel();
		if (const ADBCharacterBase* Victim = Cast<ADBCharacterBase>(Marked))
		{
			DBCombat::ApplyHit(ASC, Victim->GetAbilitySystemComponent(), Arrival);
		}
	}
	UE_LOG(LogDBCombat, Log, TEXT("%s: Schattenteleport zu %s"), *DBCombat::GetCombatName(Self), *DBCombat::GetCombatName(Marked));
	MarkedTarget.Reset();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UDBAbility_SmokeVeil::UDBAbility_SmokeVeil()
{
	DisplayName = FText::FromString(TEXT("Rauchschleier"));
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{DBTags::Ability_Class, DBTags::Ability_Veil}));
	ActivationOwnedTags.AddTag(DBTags::State_Veiled);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	StaminaCost = 15.f;
	CooldownSeconds = 15.f;
	CooldownTag = DBTags::Cooldown_SmokeVeil;
}

void UDBAbility_SmokeVeil::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UE_LOG(LogDBCombat, Log, TEXT("%s: Rauchschleier"), *DBCombat::GetCombatName(GetDBCharacter()));
	if (UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, 4.f))
	{
		Wait->OnFinish.AddDynamic(this, &UDBAbility_SmokeVeil::OnVeilEnded);
		Wait->ReadyForActivation();
	}
}

void UDBAbility_SmokeVeil::OnVeilEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ==== Magier =========================================================================================

UDBAbility_ProjectileAttack::UDBAbility_ProjectileAttack()
{
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Attack));
	ActivationOwnedTags.AddTag(DBTags::State_Attacking);
	BlockAbilitiesWithTag.AddTag(DBTags::Ability_Attack);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Block);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Veil);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	ActivationBlockedTags.AddTag(DBTags::State_Dodging);
	DamageType = DBTags::Damage_Type_Fire;
}

void UDBAbility_ProjectileAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Character = GetDBCharacter();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (const AActor* Focus = Character->GetCombatFocusTarget())
	{
		Character->SetActorRotation(FRotator(0.f, (Focus->GetActorLocation() - Character->GetActorLocation()).Rotation().Yaw, 0.f));
	}
	if (UAbilityTask_WaitDelay* Windup = UAbilityTask_WaitDelay::WaitDelay(this, WindupSeconds))
	{
		Windup->OnFinish.AddDynamic(this, &UDBAbility_ProjectileAttack::OnWindupFinished);
		Windup->ReadyForActivation();
	}
}

void UDBAbility_ProjectileAttack::OnWindupFinished()
{
	ADBCharacterBase* Character = GetDBCharacter();
	if (Character && HasAuthority(&CurrentActivationInfo))
	{
		const FVector Start = Character->GetActorLocation() + Character->GetActorForwardVector() * 70.f + FVector(0.f, 0.f, 40.f);
		FRotator Aim = Character->GetBaseAimRotation();
		if (const AActor* Focus = Character->GetCombatFocusTarget())
		{
			Aim = (Focus->GetActorLocation() - Start).Rotation();
		}
		else
		{
			Aim.Pitch = FMath::Clamp(Aim.Pitch, -20.f, 20.f);
		}
		FActorSpawnParameters Params;
		Params.Owner = Character;
		Params.Instigator = Character;
		if (ADBProjectile* Projectile = GetWorld()->SpawnActor<ADBProjectile>(ADBProjectile::StaticClass(), Start, Aim, Params))
		{
			FDBHitParams Hit;
			Hit.BaseDamage = Damage;
			Hit.PoiseDamage = PoiseDamage;
			Hit.DamageType = DamageType;
			Hit.AttackerLevel = Character->GetCombatLevel();
			const int32 Chains = ChainNodeId.IsNone() ? 0 : GetSkillRank(ChainNodeId);
			Projectile->Launch(Character, Hit, Speed, Chains, Color);
			UE_LOG(LogDBCombat, Log, TEXT("%s: %s"), *DBCombat::GetCombatName(Character), *DisplayName.ToString());
		}
	}
	if (UAbilityTask_WaitDelay* Recovery = UAbilityTask_WaitDelay::WaitDelay(this, RecoverySeconds))
	{
		Recovery->OnFinish.AddDynamic(this, &UDBAbility_ProjectileAttack::OnRecoveryFinished);
		Recovery->ReadyForActivation();
	}
}

void UDBAbility_ProjectileAttack::OnRecoveryFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

UDBAbility_MagicBolt::UDBAbility_MagicBolt()
{
	DisplayName = FText::FromString(TEXT("Magiegeschoss"));
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{DBTags::Ability_Attack, DBTags::Ability_Attack_Light}));
	Damage = 12.f;
	PoiseDamage = 8.f;
	ManaCost = 5.f;
	DamageType = DBTags::Damage_Type_Fire;
	ChainNodeId = DBSkillNodes::ChainBolt;
}

UDBAbility_FrostLance::UDBAbility_FrostLance()
{
	DisplayName = FText::FromString(TEXT("Frostlanze"));
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{DBTags::Ability_Attack, DBTags::Ability_Attack_Heavy}));
	Damage = 30.f;
	PoiseDamage = 30.f;
	ManaCost = 16.f;
	WindupSeconds = 0.55f;
	RecoverySeconds = 0.4f;
	Speed = 1800.f;
	DamageType = DBTags::Damage_Type_Frost;
	Color = FLinearColor(0.4f, 0.8f, 1.f);
}

UDBAbility_WardingCircle::UDBAbility_WardingCircle()
{
	DisplayName = FText::FromString(TEXT("Schutzkreis"));
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Class));
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	ManaCost = 35.f;
	CooldownSeconds = 20.f;
	CooldownTag = DBTags::Cooldown_WardingCircle;
}

void UDBAbility_WardingCircle::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Character = GetDBCharacter();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (HasAuthority(&ActivationInfo))
	{
		const FVector Ground = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight() - 2.f);
		if (ADBWardingCircle* Circle = GetWorld()->SpawnActor<ADBWardingCircle>(ADBWardingCircle::StaticClass(), Ground, FRotator::ZeroRotator))
		{
			const float Heal = GetSkillRank(DBSkillNodes::WardingCircle) >= 2 ? 5.f : 0.f;
			Circle->Setup(Character, 800.f, 10.f, 8.f, Heal);
			UE_LOG(LogDBCombat, Log, TEXT("%s: Schutzkreis (Heilung %.0f/s)"), *DBCombat::GetCombatName(Character), Heal);
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UDBAbility_Flight::UDBAbility_Flight()
{
	DisplayName = FText::FromString(TEXT("Flug"));
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Class));
	ActivationOwnedTags.AddTag(DBTags::State_Flying);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	ManaCost = 10.f;
}

void UDBAbility_Flight::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Character = GetDBCharacter();
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->MaxFlySpeed = FlySpeed;
	Movement->BrakingDecelerationFlying = 1500.f;
	Movement->SetMovementMode(MOVE_Flying);
	UE_LOG(LogDBCombat, Log, TEXT("%s: Flug"), *DBCombat::GetCombatName(Character));
	ScheduleDrain();
}

void UDBAbility_Flight::ScheduleDrain()
{
	if (UAbilityTask_WaitDelay* Tick = UAbilityTask_WaitDelay::WaitDelay(this, 0.25f))
	{
		Tick->OnFinish.AddDynamic(this, &UDBAbility_Flight::OnDrainTick);
		Tick->ReadyForActivation();
	}
}

void UDBAbility_Flight::OnDrainTick()
{
	ChangeMana(-ManaPerSecond * 0.25f); // server only
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || ASC->GetNumericAttribute(UDBAttributeSet::GetManaAttribute()) <= 0.f)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	ScheduleDrain();
}

void UDBAbility_Flight::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDBAbility_Flight::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ADBCharacterBase* Character = GetDBCharacter(); Character && Character->GetCharacterMovement()->IsFlying())
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		UE_LOG(LogDBCombat, Log, TEXT("%s: Landung"), *DBCombat::GetCombatName(Character));
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ==== Moench =========================================================================================

UDBAbility_CounterStance::UDBAbility_CounterStance()
{
	DisplayName = FText::FromString(TEXT("Konterhaltung"));
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Class));
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	ActivationBlockedTags.AddTag(DBTags::State_Attacking);
	StaminaCost = 15.f;
	CooldownSeconds = 4.f;
	CooldownTag = DBTags::Cooldown_CounterStance;
}

void UDBAbility_CounterStance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UDBAbilitySystemComponent* ASC = GetDBAbilitySystem();
	if (!ASC || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	constexpr float StanceSeconds = 0.6f;
	ASC->AddTimedLooseTag(DBTags::State_CounterStance, StanceSeconds);
	UE_LOG(LogDBCombat, Log, TEXT("%s: Konterhaltung"), *DBCombat::GetCombatName(GetDBCharacter()));

	if (UAbilityTask_WaitGameplayEvent* Wait = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DBTags::Event_Combat_CounterTriggered, nullptr, true))
	{
		Wait->EventReceived.AddDynamic(this, &UDBAbility_CounterStance::OnCounterTriggered);
		Wait->ReadyForActivation();
	}
	if (UAbilityTask_WaitDelay* Delay = UAbilityTask_WaitDelay::WaitDelay(this, StanceSeconds + 0.2f))
	{
		Delay->OnFinish.AddDynamic(this, &UDBAbility_CounterStance::OnStanceEnded);
		Delay->ReadyForActivation();
	}
}

void UDBAbility_CounterStance::OnCounterTriggered(FGameplayEventData Payload)
{
	ADBCharacterBase* Self = GetDBCharacter();
	const ADBCharacterBase* Attacker = Cast<ADBCharacterBase>(Payload.Instigator.Get());
	if (Self && Attacker && HasAuthority(&CurrentActivationInfo) && DBCombat::CanTarget(Self, Attacker))
	{
		const int32 Rank = FMath::Max(1, GetSkillRank(DBSkillNodes::CounterStance));
		FDBHitParams Counter;
		Counter.BaseDamage = 25.f * (1.f + 0.5f * static_cast<float>(Rank - 1));
		Counter.PoiseDamage = 40.f;
		Counter.DamageType = DBTags::Damage_Type_Spirit;
		Counter.bUnparryable = true;
		Counter.AttackerLevel = Self->GetCombatLevel();
		Self->SetActorRotation(FRotator(0.f, (Attacker->GetActorLocation() - Self->GetActorLocation()).Rotation().Yaw, 0.f));
		UE_LOG(LogDBCombat, Log, TEXT("%s: Konter gegen %s"), *DBCombat::GetCombatName(Self), *DBCombat::GetCombatName(Attacker));
		DBCombat::ApplyHit(GetAbilitySystemComponentFromActorInfo(), Attacker->GetAbilitySystemComponent(), Counter);
	}
	OnStanceEnded();
}

void UDBAbility_CounterStance::OnStanceEnded()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

UDBAbility_SkyKick::UDBAbility_SkyKick()
{
	DisplayName = FText::FromString(TEXT("Himmelstritt"));
	SetAssetTags(FGameplayTagContainer(DBTags::Ability_Class));
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(DBTags::Ability_Block);
	ActivationBlockedTags.AddTag(DBTags::State_Staggered);
	ActivationBlockedTags.AddTag(DBTags::State_KnockedDown);
	ActivationBlockedTags.AddTag(DBTags::State_Attacking);
	StaminaCost = 20.f;
	CooldownSeconds = 5.f;
	CooldownTag = DBTags::Cooldown_SkyKick;
}

void UDBAbility_SkyKick::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ADBCharacterBase* Self = GetDBCharacter();
	if (!Self || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (HasAuthority(&ActivationInfo))
	{
		for (ADBCharacterBase* Target : FindTargetsInArc(Self, 260.f, 70.f))
		{
			FDBHitParams Kick;
			Kick.BaseDamage = 18.f;
			Kick.PoiseDamage = 30.f;
			Kick.AttackerLevel = Self->GetCombatLevel();
			DBCombat::ApplyHit(GetAbilitySystemComponentFromActorInfo(), Target->GetAbilitySystemComponent(), Kick);
			if (Target->GetKnockbackScale() > 0.f)
			{
				Target->LaunchCharacter(FVector(0.f, 0.f, 750.f * Target->GetKnockbackScale()), false, true);
			}
		}
	}
	Self->LaunchCharacter(FVector(0.f, 0.f, 850.f), false, true);
	UE_LOG(LogDBCombat, Log, TEXT("%s: Himmelstritt"), *DBCombat::GetCombatName(Self));
	if (UAbilityTask_WaitDelay* Wait = UAbilityTask_WaitDelay::WaitDelay(this, 0.3f))
	{
		Wait->OnFinish.AddDynamic(this, &UDBAbility_SkyKick::OnKickFinished);
		Wait->ReadyForActivation();
	}
}

void UDBAbility_SkyKick::OnKickFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// ==== Passives =======================================================================================

UDBAbility_PassiveBonus::UDBAbility_PassiveBonus()
{
	ActivationPolicy = EDBAbilityActivationPolicy::OnSpawn;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationBlockedTags.RemoveTag(DBTags::State_Dead);
}

void UDBAbility_PassiveBonus::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !Effect)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// The spec level is the node rank (see ADBPlayerState::GrantSkillAbility).
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Effect, 1.f, ASC->MakeEffectContext());
	Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_Magnitude, MagnitudePerRank * static_cast<float>(FMath::Max(1, GetAbilityLevel())));
	Applied = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}

void UDBAbility_PassiveBonus::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); ASC && Applied.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(Applied);
		Applied.Invalidate();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UDBAbility_ManaFlow::UDBAbility_ManaFlow()
{
	DisplayName = FText::FromString(TEXT("Manafluss"));
	Effect = UDBManaFlowEffect::StaticClass();
	MagnitudePerRank = 1.5f;
}

UDBAbility_IronBody::UDBAbility_IronBody()
{
	DisplayName = FText::FromString(TEXT("Eisenkoerper"));
	Effect = UDBIronBodyEffect::StaticClass();
	MagnitudePerRank = 20.f;
}
