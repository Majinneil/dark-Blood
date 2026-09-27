#include "Character/DBPlayerCharacter.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/DBGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/DBInputConfig.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"

ADBPlayerCharacter::ADBPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->JumpZVelocity = 600.f;
	Movement->AirControl = 0.35f;
	Movement->MaxWalkSpeed = 500.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	// Double jump is unlocked through progression in Phase 2 (JumpMaxCount = 2).
	JumpMaxCount = 1;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Nameplate->SetHorizontalAlignment(EHTA_Center);
	Nameplate->SetWorldSize(18.f);
	Nameplate->SetTextRenderColor(FColor(230, 220, 200));
}

void ADBPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();
}

void ADBPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilityActorInfo();
}

void ADBPlayerCharacter::InitAbilityActorInfo()
{
	ADBPlayerState* DBPlayerState = GetPlayerState<ADBPlayerState>();
	if (!DBPlayerState)
	{
		return;
	}
	UDBAbilitySystemComponent* ASC = DBPlayerState->GetDBAbilitySystemComponent();
	ASC->InitAbilityActorInfo(DBPlayerState, this);
	CachedAbilitySystem = ASC;

	// A fresh body after respawn is alive again on every machine.
	ASC->SetLooseGameplayTagCount(DBTags::State_Dead, 0);

	BindToAttributeSet(ASC);
	DBPlayerState->OnProfileChanged.AddUniqueDynamic(this, &ADBPlayerCharacter::RefreshNameplateFromState);
	RefreshNameplate();
}

void ADBPlayerCharacter::RefreshNameplateFromState(ADBPlayerState* /*ChangedState*/)
{
	RefreshNameplate();
}

void ADBPlayerCharacter::RefreshNameplate()
{
	const ADBPlayerState* DBPlayerState = GetPlayerState<ADBPlayerState>();
	Nameplate->SetText(FText::FromString(DBPlayerState ? DBPlayerState->GetPlayerName() : FString()));
	// Your own name is not shown above your head.
	Nameplate->SetHiddenInGame(IsLocallyControlled());
}

void ADBPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Face the nameplate towards the local camera (only other players' nameplates are visible).
	if (!IsLocallyControlled())
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector ToCamera = Camera->GetCameraLocation() - Nameplate->GetComponentLocation();
			Nameplate->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
		}
	}
}

void ADBPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const ADBPlayerController* DBController = GetController<ADBPlayerController>();
	const UDBInputConfig* InputConfig = DBController ? DBController->GetInputConfig() : nullptr;
	if (!EnhancedInput || !InputConfig)
	{
		return;
	}

	if (const UInputAction* Move = InputConfig->FindNativeInputAction(DBTags::Input_Move))
	{
		EnhancedInput->BindAction(Move, ETriggerEvent::Triggered, this, &ADBPlayerCharacter::Input_Move);
	}
	if (const UInputAction* Look = InputConfig->FindNativeInputAction(DBTags::Input_Look))
	{
		EnhancedInput->BindAction(Look, ETriggerEvent::Triggered, this, &ADBPlayerCharacter::Input_Look);
	}
	if (const UInputAction* JumpAction = InputConfig->FindNativeInputAction(DBTags::Input_Jump))
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	for (const FDBInputAction& Binding : InputConfig->AbilityInputActions)
	{
		if (Binding.InputAction && Binding.InputTag.IsValid())
		{
			EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Started, this, &ADBPlayerCharacter::Input_AbilityPressed, Binding.InputTag);
			EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Completed, this, &ADBPlayerCharacter::Input_AbilityReleased, Binding.InputTag);
		}
	}
}

void ADBPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || IsDead())
	{
		return;
	}
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
}

void ADBPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ADBPlayerCharacter::Input_AbilityPressed(FGameplayTag InputTag)
{
	if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ADBPlayerCharacter::Input_AbilityReleased(FGameplayTag InputTag)
{
	if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}
