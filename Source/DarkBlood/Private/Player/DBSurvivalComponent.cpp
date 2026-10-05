#include "Player/DBSurvivalComponent.h"

#include "Abilities/DBAttributeSet.h"
#include "Abilities/DBCombatEffects.h"
#include "AbilitySystemComponent.h"
#include "Core/DBGameSettings.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Engine/World.h"
#include "Framework/DBGameState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "World/DBRealmDirector.h"
#include "World/DBRealmLayout.h"
#include "World/DBWorldStateComponent.h"

#define LOCTEXT_NAMESPACE "DarkBloodSurvival"

namespace R = DarkBlood::Rules;

namespace
{
	/** Stamina per second while swimming; an exhausted swimmer loses this share of max health per second. */
	constexpr float SwimStaminaPerSecond = 5.f;
	constexpr float ExhaustedHealthShare = 0.04f;

	float GetRegionCold(EDBRealmBiome Biome)
	{
		switch (Biome)
		{
		case EDBRealmBiome::IceWaste: return 0.8f;
		case EDBRealmBiome::MistMountains: return 0.4f;
		case EDBRealmBiome::VassalFortress: return 0.15f;
		default: return 0.f;
		}
	}
}

UDBSurvivalComponent::UDBSurvivalComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

void UDBSurvivalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDBSurvivalComponent, ReplicatedSatiety, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDBSurvivalComponent, ReplicatedWarmth, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDBSurvivalComponent, Status, COND_OwnerOnly);
}

void UDBSurvivalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	Accumulator += DeltaTime;
	if (Accumulator >= 1.f)
	{
		UpdateSurvival(Accumulator);
		UpdateSwimming(Accumulator);
		Accumulator = 0.f;
	}
}

void UDBSurvivalComponent::UpdateSurvival(float Seconds)
{
	const ADBPlayerState* PlayerState = Cast<ADBPlayerState>(GetOwner());
	const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	const float MinutesPerDay = FMath::Max(1.f, UDBGameSettings::Get().RealMinutesPerGameDay);
	const double GameHours = Seconds * 24.0 / (static_cast<double>(MinutesPerDay) * 60.0);

	// Exposure from the open world: region, height, night, wet clothes; settlements warm.
	float Exposure = 0.f;
	bool bWarming = false;
	const FVector Meters = Pawn->GetActorLocation() / 100.0;
	if (ADBRealmDirector::Get(GetWorld()) && DBRealm::IsInside(Meters.X, Meters.Y))
	{
		const TArray<FDBRealmRegion>& Regions = DBRealm::GetRegions();
		const int32 Region = DBRealm::FindRegionIndex(Meters.X, Meters.Y);
		const ADBGameState* GameState = GetWorld()->GetGameState<ADBGameState>();
		const bool bNight = GameState && GameState->GetWorldState() && GameState->GetWorldState()->IsNight();
		const UAbilitySystemComponent* ASC = PlayerState->GetAbilitySystemComponent();
		const float Protection = ASC ? FMath::Clamp(ASC->GetNumericAttribute(UDBAttributeSet::GetResistFrostAttribute()), 0.f, 0.9f) : 0.f;
		Exposure = R::ComputeColdExposure(Regions.IsValidIndex(Region) ? GetRegionCold(Regions[Region].Biome) : 0.f, Meters.Z, bNight, bSwimming, Protection);
		for (const FDBRealmSettlement& Site : DBRealm::GetSettlements())
		{
			if (FVector2D::Distance(FVector2D(Meters), Site.Center) < Site.Radius)
			{
				bWarming = true;
				break;
			}
		}
	}
	LastExposure = Exposure;
	R::AdvanceSurvival(State, GameHours, Exposure, bWarming);
	ReplicatedSatiety = State.Satiety;
	ReplicatedWarmth = State.Warmth;

	const R::FSurvivalModifiers Modifiers = R::GetSurvivalModifiers(State);
	const uint8 Gained = Modifiers.Status & ~Status;
	Status = Modifiers.Status;
	ApplyModifiers(Modifiers);
	if (Gained & R::ESurvivalStatus::Starving)
	{
		Notify(LOCTEXT("Starving", "Du verhungerst - iss etwas! (keine Heilung)"));
	}
	else if (Gained & R::ESurvivalStatus::Hungry)
	{
		Notify(LOCTEXT("Hungry", "Du bist hungrig (langsamere Regeneration)."));
	}
	else if (Gained & R::ESurvivalStatus::WellFed)
	{
		Notify(LOCTEXT("WellFed", "Gut gesaettigt (+10 % Regeneration)."));
	}
	if (Gained & R::ESurvivalStatus::Freezing)
	{
		Notify(LOCTEXT("Freezing", "Du erfrierst - such Waerme in einer Siedlung!"));
	}
	else if (Gained & R::ESurvivalStatus::Cold)
	{
		Notify(LOCTEXT("Cold", "Dir ist kalt (langsamere Ausdauer)."));
	}
}

void UDBSurvivalComponent::UpdateSwimming(float Seconds)
{
	ADBPlayerState* PlayerState = Cast<ADBPlayerState>(GetOwner());
	const ACharacter* Character = PlayerState ? PlayerState->GetPawn<ACharacter>() : nullptr;
	UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	const bool bNowSwimming = Character && ASC && Character->GetCharacterMovement()->IsSwimming();
	if (bNowSwimming != bSwimming && ASC)
	{
		bSwimming = bNowSwimming;
		bExhaustedWarned = false;
		if (bSwimming)
		{
			ASC->AddLooseGameplayTag(DBTags::State_Swimming);
		}
		else
		{
			ASC->RemoveLooseGameplayTag(DBTags::State_Swimming);
		}
	}
	if (!bSwimming)
	{
		return;
	}
	if (ASC->GetNumericAttribute(UDBAttributeSet::GetStaminaAttribute()) > 0.5f)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDBStaminaCostEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_StaminaCost, -SwimStaminaPerSecond * Seconds);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		return;
	}
	// Exhausted: the water pulls the character under. Damage goes through the damage meta attribute (death handling).
	if (!bExhaustedWarned)
	{
		bExhaustedWarned = true;
		Notify(LOCTEXT("Exhausted", "Erschoepft - du gehst unter! Schwimm ans Ufer."));
	}
	if (!DrownEffect)
	{
		DrownEffect = NewObject<UGameplayEffect>(this, TEXT("DBDrowning"));
		DrownEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UDBAttributeSet::GetIncomingDamageAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = DBTags::SetByCaller_Magnitude;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
		DrownEffect->Modifiers.Add(Modifier);
	}
	FGameplayEffectSpec Spec(DrownEffect, ASC->MakeEffectContext(), 1.f);
	Spec.SetSetByCallerMagnitude(DBTags::SetByCaller_Magnitude, ASC->GetNumericAttribute(UDBAttributeSet::GetMaxHealthAttribute()) * ExhaustedHealthShare * Seconds);
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
}

void UDBSurvivalComponent::ApplyModifiers(const R::FSurvivalModifiers& Modifiers)
{
	ADBPlayerState* PlayerState = Cast<ADBPlayerState>(GetOwner());
	UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ASC || (FMath::IsNearlyEqual(AppliedStamina, Modifiers.StaminaRegen) && FMath::IsNearlyEqual(AppliedHealth, Modifiers.HealthRegen) && SurvivalEffect.IsValid())
		|| (!SurvivalEffect.IsValid() && FMath::IsNearlyEqual(Modifiers.StaminaRegen, 1.f) && FMath::IsNearlyEqual(Modifiers.HealthRegen, 1.f)))
	{
		return;
	}
	if (SurvivalEffect.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(SurvivalEffect);
		SurvivalEffect.Invalidate();
	}
	AppliedStamina = Modifiers.StaminaRegen;
	AppliedHealth = Modifiers.HealthRegen;
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDBSurvivalEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
	Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_SurvivalStamina, AppliedStamina);
	Spec.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_SurvivalHealth, AppliedHealth);
	SurvivalEffect = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}

void UDBSurvivalComponent::Eat(float Satiety)
{
	if (Satiety <= 0.f || !GetOwner()->HasAuthority())
	{
		return;
	}
	R::EatFood(State, Satiety);
	ReplicatedSatiety = State.Satiety;
	UpdateSurvival(0.f);
}

void UDBSurvivalComponent::SetValues(float Satiety, float Warmth)
{
	State.Satiety = FMath::Clamp(Satiety, 0.f, 100.f);
	State.Warmth = FMath::Clamp(Warmth, 0.f, 100.f);
	ReplicatedSatiety = State.Satiety;
	ReplicatedWarmth = State.Warmth;
	UpdateSurvival(0.f);
}

void UDBSurvivalComponent::Notify(const FText& Text) const
{
	const ADBPlayerState* PlayerState = Cast<ADBPlayerState>(GetOwner());
	if (ADBPlayerController* Controller = PlayerState ? Cast<ADBPlayerController>(PlayerState->GetPlayerController()) : nullptr)
	{
		Controller->ClientShowNotification(Text);
	}
	UE_LOG(LogDarkBlood, Log, TEXT("%s: %s"), PlayerState ? *PlayerState->GetPlayerName() : TEXT("?"), *Text.ToString());
}

#undef LOCTEXT_NAMESPACE
