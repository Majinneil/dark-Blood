#include "Character/DBNpcCharacter.h"

#include "Components/TextRenderComponent.h"
#include "Dialogue/DBDialogueComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/DBPlayerController.h"

ADBNpcCharacter::ADBNpcCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	Team = EDBTeam::Neutral;
	KnockbackScale = 0.f;
	PlaceholderColor = FLinearColor(0.85f, 0.65f, 0.2f);

	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Nameplate->SetHorizontalAlignment(EHTA_Center);
	Nameplate->SetWorldSize(16.f);
	Nameplate->SetTextRenderColor(FColor(235, 200, 110));
}

void ADBNpcCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADBNpcCharacter, ReplicatedName);
}

void ADBNpcCharacter::Setup(FName InNpcId, const FText& InDisplayName, FName InDialogueId)
{
	NpcId = InNpcId;
	DisplayName = InDisplayName;
	DialogueId = InDialogueId;
	ReplicatedName = DisplayName;
	OnRep_Identity();
}

void ADBNpcCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && ReplicatedName.IsEmpty())
	{
		ReplicatedName = DisplayName.IsEmpty() ? FText::FromName(NpcId) : DisplayName;
	}
	OnRep_Identity();
}

void ADBNpcCharacter::OnRep_Identity()
{
	Nameplate->SetText(ReplicatedName);
}

FString ADBNpcCharacter::GetCombatDisplayName() const
{
	return ReplicatedName.IsEmpty() ? NpcId.ToString() : ReplicatedName.ToString();
}

FText ADBNpcCharacter::GetInteractionText() const
{
	return FText::Format(NSLOCTEXT("DarkBlood", "TalkTo", "Sprechen: {0}"), ReplicatedName);
}

bool ADBNpcCharacter::CanInteract(const APawn* User) const
{
	return User && !IsDead();
}

void ADBNpcCharacter::Interact(APlayerController* User)
{
	ADBPlayerController* Speaker = Cast<ADBPlayerController>(User);
	if (!Speaker || !HasAuthority())
	{
		return;
	}
	if (const APawn* Pawn = Speaker->GetPawn())
	{
		const FVector ToUser = Pawn->GetActorLocation() - GetActorLocation();
		SetActorRotation(FRotator(0.f, ToUser.Rotation().Yaw, 0.f));
	}
	Speaker->GetDialogue()->StartDialogue(this, NpcId, DialogueId);
}

void ADBNpcCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = Camera->GetCameraLocation() - Nameplate->GetComponentLocation();
		Nameplate->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
	}
}
