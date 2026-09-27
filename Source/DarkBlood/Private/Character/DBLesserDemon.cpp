#include "Character/DBLesserDemon.h"

#include "AI/DBMeleeAIComponent.h"
#include "Abilities/DBMeleeAttackAbility.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADBLesserDemon::ADBLesserDemon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EnemyId = TEXT("LesserDemon");
	DisplayName = FText::FromString(TEXT("Niederer Daemon [DEV]"));
	Level = 3;
	MaxHealth = 140.f;
	MaxPoise = 45.f;
	Armor = 20.f;
	AttackPower = 5.f;
	XpReward = 45;
	LootTableId = TEXT("LT_LesserDemon");
	AttackAbility = UDBAbility_DemonClaw::StaticClass();
	RespawnSeconds = 6.f; // corpse lifetime

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 380.f;
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 480.f, 0.f);

	// Slightly bulkier placeholder than the player.
	PlaceholderBody->SetRelativeScale3D(FVector(0.95f, 0.95f, 1.9f));

	MeleeAI = CreateDefaultSubobject<UDBMeleeAIComponent>(TEXT("MeleeAI"));
}
