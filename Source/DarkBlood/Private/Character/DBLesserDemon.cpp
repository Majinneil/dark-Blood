#include "Character/DBLesserDemon.h"

#include "AI/DBMeleeAIComponent.h"
#include "Abilities/DBMeleeAttackAbility.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/DBGameDataSubsystem.h"
#include "Visual/DBCharacterVisualComponent.h"

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
	Visuals->SetProfileId(TEXT("CV_Enemy_LesserDemon"));
}

void ADBLesserDemon::BeginPlay()
{
	// Body: the Paragon minions when available (elites the big "super" minion), else the placeholder demon.
	// Same choice on every machine (the name replicates with the spawn).
	if (const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this))
	{
		const bool bElite = DisplayName.ToString().StartsWith(TEXT("Elite"));
		const FName Body = bElite ? Data->PickCharacterVisual({TEXT("CV_ParagonMinions_Minion_Lane_Super_Dusk"), TEXT("CV_ParagonMinions_Minion_Lane_Super_Dawn")})
								  : Data->PickCharacterVisual({TEXT("CV_ParagonMinions_Minion_Lane_Melee_Dusk"), TEXT("CV_ParagonMinions_Minion_Lane_Melee_Dawn")});
		if (!Body.IsNone())
		{
			Visuals->SetProfileId(Body);
			// Blood-red demon mark (elites a little stronger); faded out beyond camp distance.
			Visuals->SetDemonAccent(FLinearColor(1.f, 0.07f, 0.04f), bElite ? 0.8f : 0.55f, 4000.f);
		}
	}
	Super::BeginPlay();
}
