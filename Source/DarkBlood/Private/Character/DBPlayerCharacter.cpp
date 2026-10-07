#include "Character/DBPlayerCharacter.h"

#include "Abilities/DBAbilitySystemComponent.h"
#include "Data/DBGameDataSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Combat/DBLockOnComponent.h"
#include "Dialogue/DBDialogueComponent.h"
#include "Interaction/DBInteractionComponent.h"
#include "Inventory/DBInventoryComponent.h"
#include "World/DBShip.h"
#include "Character/DBHorse.h"
#include "UI/DBGameHUD.h"
#include "Components/CapsuleComponent.h"
#include "Visual/DBCharacterVisualComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/DBGameplayTags.h"
#include "DarkBlood.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/DBInputConfig.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DBPlayerController.h"
#include "Player/DBPlayerState.h"
#include "Player/DBProgressionComponent.h"

ADBPlayerCharacter::ADBPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Team = EDBTeam::Players;
	PlaceholderColor = FLinearColor(0.2f, 0.35f, 0.75f);
	Visuals->SetProfileId(TEXT("CV_Player_TypeA"));

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

	LockOn = CreateDefaultSubobject<UDBLockOnComponent>(TEXT("LockOn"));
	Interaction = CreateDefaultSubobject<UDBInteractionComponent>(TEXT("Interaction"));

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
	ASC->RegisterGameplayTagEvent(DBTags::Movement_DoubleJump).AddUObject(this, &ADBPlayerCharacter::OnDoubleJumpTagChanged);
	OnDoubleJumpTagChanged(DBTags::Movement_DoubleJump, ASC->GetTagCount(DBTags::Movement_DoubleJump));
	DBPlayerState->OnProfileChanged.AddUniqueDynamic(this, &ADBPlayerCharacter::RefreshNameplateFromState);
	if (UDBInventoryComponent* Inventory = DBPlayerState->GetInventory())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ADBPlayerCharacter::RefreshEquippedWeapon);
	}
	RefreshNameplate();
	ApplyPlayerVisuals();
}

void ADBPlayerCharacter::RefreshEquippedWeapon()
{
	const ADBPlayerState* DBPlayerState = GetPlayerState<ADBPlayerState>();
	const UDBInventoryComponent* Inventory = DBPlayerState ? DBPlayerState->GetInventory() : nullptr;
	Visuals->SetWeaponItem(Inventory ? Inventory->GetEquipped(EDBEquipSlot::MainHand).ItemId : NAME_None);
}

void ADBPlayerCharacter::RefreshNameplateFromState(ADBPlayerState* /*ChangedState*/)
{
	RefreshNameplate();
	ApplyPlayerVisuals();
}

void ADBPlayerCharacter::ApplyPlayerVisuals()
{
	const ADBPlayerState* DBPlayerState = GetPlayerState<ADBPlayerState>();
	if (!DBPlayerState)
	{
		return;
	}
	// Only replicated ids travel over the network; every machine builds the same visuals from them.
	const FDBCharacterProfile& Profile = DBPlayerState->GetProfile();
	FName Body = Profile.Appearance.BodyType == EDBBodyType::TypeB ? FName(TEXT("CV_Player_TypeB")) : FName(TEXT("CV_Player_TypeA"));
	// Body type A is the hero, Akaza Kurosaki, once his authored body is imported (Tools/UE58/db_import_hyper3d.py).
	const UDBGameDataSubsystem* Data = UDBGameDataSubsystem::Get(this);
	if (Profile.Appearance.BodyType != EDBBodyType::TypeB && Data && !Data->PickCharacterVisual({TEXT("CV_Hyper3D_Akaza")}).IsNone())
	{
		Body = TEXT("CV_Hyper3D_Akaza");
	}
	Visuals->SetAppearance(Profile.Appearance, Profile.ClassId, Body);
	RefreshEquippedWeapon();
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
		EnhancedInput->BindActionValueLambda(Move, ETriggerEvent::Completed, [this](const FInputActionValue&)
		{
			if (!ShipSteering.IsZero() && ADBShip::FindSteeredBy(this))
			{
				ShipSteering = FVector2D::ZeroVector;
				ServerSteerShip(FVector2D::ZeroVector);
			}
			if (ADBHorse::FindRiddenBy(this))
			{
				ServerSteerHorse(FVector2D::ZeroVector, 0.f);
			}
		});
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

void ADBPlayerCharacter::ServerSteerShip_Implementation(FVector2D Input)
{
	if (ADBShip* Ship = ADBShip::FindSteeredBy(this))
	{
		Ship->SetSteering(Input);
	}
}

void ADBPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (ADBShip::FindSteeredBy(this))
	{
		ShipSteering = Axis;
		ServerSteerShip(Axis);
		return;
	}
	if (ADBHorse::FindRiddenBy(this))
	{
		ServerSteerHorse(Axis, Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw);
		return;
	}
	// Attacks, dodges and hit reactions own the movement while they run.
	if (!Controller || IsMovementInputBlocked())
	{
		return;
	}
	// Flying follows the camera pitch; on the ground only the yaw matters.
	const FRotator Rotation = GetCharacterMovement()->IsFlying() ? Controller->GetControlRotation()
																 : FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Rotation).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y), Axis.X);
}

void ADBPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (LockOn->IsLockedOn())
	{
		// The camera follows the target; a horizontal flick switches to the next one.
		LockOn->AddSwitchInput(Axis.X);
		return;
	}
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ADBPlayerCharacter::Input_AbilityPressed(FGameplayTag InputTag)
{
	// In a conversation: 1-4 pick options, E continues; combat input is ignored.
	if (const ADBPlayerController* DBController = GetController<ADBPlayerController>(); DBController && DBController->GetDialogue()->IsDialogueOpen())
	{
		UDBDialogueComponent* Dialogue = DBController->GetDialogue();
		const FGameplayTag Options[] = {DBTags::Input_Ability1, DBTags::Input_Ability2, DBTags::Input_Ability3, DBTags::Input_Ability4};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Options); ++Index)
		{
			if (InputTag == Options[Index])
			{
				Dialogue->Choose(Index);
			}
		}
		if (InputTag == DBTags::Input_Interact && Dialogue->GetView().bEnds)
		{
			Dialogue->Choose(0);
		}
		return;
	}
	if (InputTag == DBTags::Input_Interact)
	{
		Interaction->TryInteract();
		return;
	}
	if (InputTag == DBTags::Input_CallHorse)
	{
		ServerCallHorse();
		return;
	}
	// In the saddle: Sprint gallops; no attacks, dodges or abilities (windows and interaction still work).
	if (ADBHorse::FindRiddenBy(this))
	{
		if (InputTag == DBTags::Input_Sprint)
		{
			ServerSetGallop(true);
		}
		const bool bWindow = InputTag == DBTags::Input_UI_SkillTree || InputTag == DBTags::Input_UI_Inventory || InputTag == DBTags::Input_UI_Settings
			|| InputTag == DBTags::Input_UI_Map;
		if (!bWindow)
		{
			return;
		}
	}
	if (InputTag == DBTags::Input_UI_SkillTree || InputTag == DBTags::Input_UI_Inventory || InputTag == DBTags::Input_UI_Settings
		|| InputTag == DBTags::Input_UI_Map)
	{
		if (const APlayerController* PC = GetController<APlayerController>())
		{
			if (ADBGameHUD* GameHUD = PC->GetHUD<ADBGameHUD>())
			{
				if (InputTag == DBTags::Input_UI_Settings)
				{
					GameHUD->ToggleSettings();
				}
				else if (InputTag == DBTags::Input_UI_Map)
				{
					GameHUD->ToggleMap();
				}
				else
				{
					InputTag == DBTags::Input_UI_SkillTree ? GameHUD->ToggleSkillTree() : GameHUD->ToggleInventory();
				}
			}
		}
		return;
	}
	// Lock-on is a camera/targeting feature, not an ability.
	if (InputTag == DBTags::Input_LockOn)
	{
		LockOn->ToggleLockOn();
		return;
	}
	if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ADBPlayerCharacter::ServerSteerHorse_Implementation(FVector2D Input, float CameraYaw)
{
	if (ADBHorse* Horse = ADBHorse::FindRiddenBy(this))
	{
		Horse->SetSteering(Input, CameraYaw);
	}
}

void ADBPlayerCharacter::ServerSetGallop_Implementation(bool bGallop)
{
	if (ADBHorse* Horse = ADBHorse::FindRiddenBy(this))
	{
		Horse->SetGallop(bGallop);
	}
}

void ADBPlayerCharacter::ServerCallHorse_Implementation()
{
	if (ADBHorse::CallHorse(this))
	{
		if (ADBPlayerController* DBController = GetController<ADBPlayerController>())
		{
			DBController->ClientShowNotification(NSLOCTEXT("DarkBlood", "HorseCalled", "Dein Pferd kommt."));
		}
	}
}

void ADBPlayerCharacter::Input_AbilityReleased(FGameplayTag InputTag)
{
	if (InputTag == DBTags::Input_Sprint && ADBHorse::FindRiddenBy(this))
	{
		ServerSetGallop(false);
		return;
	}
	if (UDBAbilitySystemComponent* ASC = GetDBAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

FString ADBPlayerCharacter::GetCombatDisplayName() const
{
	const ADBPlayerState* DBPlayerState = GetPlayerState<ADBPlayerState>();
	return DBPlayerState ? DBPlayerState->GetPlayerName() : Super::GetCombatDisplayName();
}

int32 ADBPlayerCharacter::GetCombatLevel() const
{
	const ADBPlayerState* DBPlayerState = GetPlayerState<ADBPlayerState>();
	return DBPlayerState ? DBPlayerState->GetProgression()->GetLevel() : 1;
}

AActor* ADBPlayerCharacter::GetCombatFocusTarget() const
{
	return LockOn->GetLockTarget();
}

void ADBPlayerCharacter::OnDoubleJumpTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	JumpMaxCount = NewCount > 0 ? 2 : 1;
}

void ADBPlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	UE_LOG(LogDBCombat, Verbose, TEXT("%s jump %d/%d"), *GetCombatDisplayName(), JumpCurrentCount, JumpMaxCount);
}
