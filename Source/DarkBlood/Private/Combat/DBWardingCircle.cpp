#include "Combat/DBWardingCircle.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Abilities/DBCombatEffects.h"
#include "Character/DBCharacterBase.h"
#include "Combat/DBCombatStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADBWardingCircle::ADBWardingCircle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	RootComponent = Disc;
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Disc->SetStaticMesh(Cylinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Disc->SetMaterial(0, ShapeMaterial.Object);
	}
}

void ADBWardingCircle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBWardingCircle, Radius);
}

void ADBWardingCircle::Setup(AActor* InCaster, float InRadius, float InDuration, float InDamagePerSecond, float InHealPerSecond)
{
	Caster = InCaster;
	Radius = InRadius;
	DamagePerSecond = InDamagePerSecond;
	HealPerSecond = InHealPerSecond;
	SetLifeSpan(InDuration);
	OnRep_Radius();
}

void ADBWardingCircle::OnRep_Radius()
{
	// Flat golden disc at ground level (engine cylinder is 100 units wide and tall, centered).
	Disc->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.02f));
	if (UMaterialInstanceDynamic* Material = Disc->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.75f, 0.3f));
	}
}

void ADBWardingCircle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	PulseTimer -= DeltaSeconds;
	if (PulseTimer > 0.f)
	{
		return;
	}
	PulseTimer = PulseInterval;

	const AActor* CasterActor = Caster.Get();
	const ADBCharacterBase* CasterCharacter = Cast<ADBCharacterBase>(CasterActor);
	UAbilitySystemComponent* CasterASC = CasterCharacter ? CasterCharacter->GetAbilitySystemComponent() : nullptr;
	const FVector Center = GetActorLocation();

	for (TActorIterator<ADBCharacterBase> It(GetWorld()); It; ++It)
	{
		ADBCharacterBase* Character = *It;
		const FVector Offset = Character->GetActorLocation() - Center;
		if (Character->IsDead() || Offset.Size2D() > Radius || FMath::Abs(Offset.Z) > 300.f)
		{
			continue;
		}
		UDBAbilitySystemComponent* ASC = Character->GetDBAbilitySystemComponent();
		if (!ASC)
		{
			continue;
		}
		if (CasterCharacter && DBCombat::CanTarget(CasterCharacter, Character))
		{
			// Demons burn and are pushed out; heavy enemies (KnockbackScale 0) break through.
			FDBHitParams Burn;
			Burn.BaseDamage = DamagePerSecond * PulseInterval;
			Burn.DamageType = DBTags::Damage_Type_Spirit;
			Burn.bUnblockable = true;
			Burn.AttackerLevel = CasterCharacter->GetCombatLevel();
			DBCombat::ApplyHit(CasterASC, ASC, Burn);
			if (Character->GetKnockbackScale() > 0.f)
			{
				const FVector Out = Offset.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Offset.GetSafeNormal2D();
				Character->LaunchCharacter(Out * 900.f * Character->GetKnockbackScale() + FVector(0.f, 0.f, 150.f), true, true);
			}
		}
		else if (CasterCharacter && Character->GetTeam() == CasterCharacter->GetTeam())
		{
			ASC->AddTimedLooseTag(DBTags::State_Warded, PulseInterval + 0.1f);
			if (HealPerSecond > 0.f)
			{
				const FGameplayEffectSpecHandle Heal = ASC->MakeOutgoingSpec(UDBHealEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
				if (Heal.IsValid())
				{
					Heal.Data->SetSetByCallerMagnitude(DBTags::SetByCaller_Magnitude, HealPerSecond * PulseInterval);
					ASC->ApplyGameplayEffectSpecToSelf(*Heal.Data);
				}
			}
		}
	}
}
