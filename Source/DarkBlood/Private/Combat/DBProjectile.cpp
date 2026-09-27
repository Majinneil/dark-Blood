#include "Combat/DBProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Character/DBCharacterBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DarkBlood.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Chain hits jump to the nearest other enemy within this range. */
	constexpr float ChainRange = 700.f;
	constexpr float ChainDamageFactor = 0.6f;
}

ADBProjectile::ADBProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 3.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(24.f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeScale3D(FVector(0.35f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Visual->SetStaticMesh(Sphere.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		Visual->SetMaterial(0, ShapeMaterial.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 2200.f;
	Movement->MaxSpeed = 4000.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
}

void ADBProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBProjectile, Color);
}

void ADBProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Collision->OnComponentBeginOverlap.AddDynamic(this, &ADBProjectile::OnOverlap);
		Collision->OnComponentHit.AddDynamic(this, &ADBProjectile::OnBlocked);
	}
	OnRep_Color();
}

void ADBProjectile::Launch(AActor* InShooter, const FDBHitParams& InHit, float Speed, int32 InChainCount, const FLinearColor& InColor)
{
	Shooter = InShooter;
	HitParams = InHit;
	ChainCount = InChainCount;
	Color = InColor;
	OnRep_Color();
	if (InShooter)
	{
		Collision->IgnoreActorWhenMoving(InShooter, true);
		AlreadyHit.Add(InShooter);
	}
	Movement->Velocity = GetActorForwardVector() * Speed;
}

void ADBProjectile::OnRep_Color()
{
	if (UMaterialInstanceDynamic* Material = Visual->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void ADBProjectile::OnOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!bSpent && OtherActor && !AlreadyHit.Contains(OtherActor) && DBCombat::CanTarget(Shooter.Get(), OtherActor))
	{
		HitTarget(OtherActor);
	}
}

void ADBProjectile::OnBlocked(UPrimitiveComponent* /*HitComponent*/, AActor* /*OtherActor*/, UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/, const FHitResult& /*Hit*/)
{
	if (!bSpent)
	{
		bSpent = true;
		Destroy();
	}
}

void ADBProjectile::HitTarget(AActor* Target)
{
	bSpent = true;
	AlreadyHit.Add(Target);
	const IAbilitySystemInterface* ShooterASC = Cast<IAbilitySystemInterface>(Shooter.Get());
	const IAbilitySystemInterface* TargetASC = Cast<IAbilitySystemInterface>(Target);
	if (ShooterASC && TargetASC)
	{
		DBCombat::ApplyHit(ShooterASC->GetAbilitySystemComponent(), TargetASC->GetAbilitySystemComponent(), HitParams);
	}

	// Chain lightning style: jump to the nearest other enemy with reduced damage.
	if (ChainCount > 0)
	{
		AActor* Next = nullptr;
		float BestDistance = ChainRange;
		for (TActorIterator<ADBCharacterBase> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), Target->GetActorLocation());
			if (!AlreadyHit.Contains(*It) && Distance < BestDistance && DBCombat::CanTarget(Shooter.Get(), *It))
			{
				BestDistance = Distance;
				Next = *It;
			}
		}
		if (Next)
		{
			FActorSpawnParameters Params;
			Params.Owner = Shooter.Get();
			const FVector Start = Target->GetActorLocation();
			const FRotator Aim = (Next->GetActorLocation() - Start).Rotation();
			if (ADBProjectile* Chain = GetWorld()->SpawnActor<ADBProjectile>(GetClass(), Start, Aim, Params))
			{
				FDBHitParams ChainHit = HitParams;
				ChainHit.BaseDamage *= ChainDamageFactor;
				Chain->AlreadyHit = AlreadyHit;
				Chain->Launch(Shooter.Get(), ChainHit, Movement->Velocity.Size(), ChainCount - 1, Color);
				Chain->Collision->IgnoreActorWhenMoving(Target, true);
				UE_LOG(LogDBCombat, Log, TEXT("Projectile chains to %s"), *DBCombat::GetCombatName(Next));
			}
		}
	}
	Destroy();
}
