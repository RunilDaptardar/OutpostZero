// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutpostZeroCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OutpostZero.h"

AOutpostZeroCharacter::AOutpostZeroCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// Suit-mounted utility light. Attached to the camera so it follows view rotation and the crouch camera offset
	// without any per-frame updates. Relative location is in camera space (X forward, Y right, Z up).
	UtilityLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Utility Light"));
	UtilityLight->SetupAttachment(FirstPersonCameraComponent);
	UtilityLight->SetRelativeLocation(FVector(0.0f, 8.0f, -8.0f));
	UtilityLight->SetMobility(EComponentMobility::Movable);
	UtilityLight->IntensityUnits = ELightUnits::Candelas;
	UtilityLight->Intensity = 30.0f;
	UtilityLight->AttenuationRadius = 450.0f;
	UtilityLight->InnerConeAngle = 12.0f;
	UtilityLight->OuterConeAngle = 25.0f;
	UtilityLight->LightColor = FLinearColor(0.80f, 0.90f, 1.0f).ToFColor(true);
	UtilityLight->CastShadows = true;
	UtilityLight->VolumetricScatteringIntensity = 0.0f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	// Enable crouching. Crouched capsule: radius 34, half-height 40 (80 cm tall),
	// which fits the 130 cm vents; standing (192 cm) does not.
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(40.0f);

	// Stamina and movement speeds are updated every frame
	PrimaryActorTick.bCanEverTick = true;
}

void AOutpostZeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	// start with full stamina
	CurrentStamina = MaxSprintStamina;
	bStaminaExhausted = false;
	bIsSprinting = false;

	// utility light starting state (shown once a player possesses this character)
	bUtilityLightOn = bUtilityLightStartsOn;
	RefreshUtilityLight();
}

void AOutpostZeroCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	RefreshUtilityLight();
}

void AOutpostZeroCharacter::RefreshUtilityLight()
{
	if (UtilityLight)
	{
		// AI-controlled pawns and opted-out variants never show the light
		UtilityLight->SetVisibility(bEnableUtilityLight && bUtilityLightOn && IsPlayerControlled());
	}
}

void AOutpostZeroCharacter::ToggleUtilityLight()
{
	SetUtilityLightOn(!bUtilityLightOn);
}

void AOutpostZeroCharacter::SetUtilityLightOn(bool bOn)
{
	if (!bEnableUtilityLight || bUtilityLightOn == bOn)
	{
		return;
	}

	bUtilityLightOn = bOn;
	RefreshUtilityLight();
	OnUtilityLightToggled.Broadcast(bUtilityLightOn);
}

void AOutpostZeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ShouldUseStealthMovement())
	{
		UpdateStealthMovement(DeltaSeconds);
	}
}

bool AOutpostZeroCharacter::ShouldUseStealthMovement() const
{
	// only the player-controlled character uses these rules; AI pawns and opted-out variants keep their own movement
	return bUseStealthMovement && IsPlayerControlled();
}

void AOutpostZeroCharacter::UpdateStealthMovement(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	const float MaxStamina = FMath::Max(MaxSprintStamina, KINDA_SMALL_NUMBER);

	// sprinting requires being on the ground and actually moving under input
	const bool bOnGround = Move->IsMovingOnGround();
	const bool bMoving = GetVelocity().Size2D() > MinSprintMoveSpeed && !Move->GetCurrentAcceleration().IsNearlyZero();

	// any crouch state (held, transitioning or stuck crouched under a low ceiling such as a vent) blocks sprinting
	const bool bCrouchBlocksSprint = bIsCrouched || bCrouchInputHeld || Move->bWantsToCrouch;

	bIsSprinting = bSprintInputHeld && !bCrouchBlocksSprint && !bStaminaExhausted && CurrentStamina > 0.0f && bOnGround && bMoving;

	if (bIsSprinting)
	{
		// drain one second of stamina per second of sprinting
		CurrentStamina = FMath::Max(0.0f, CurrentStamina - DeltaSeconds);

		if (CurrentStamina <= 0.0f)
		{
			// out of stamina: drop back to walking and lock sprint until partially recovered
			bIsSprinting = false;
			bStaminaExhausted = true;
		}
	}
	else
	{
		// recover from empty to full over StaminaFullRecoveryTime seconds
		const float RecoveryRate = MaxStamina / FMath::Max(StaminaFullRecoveryTime, KINDA_SMALL_NUMBER);
		CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + RecoveryRate * DeltaSeconds);

		if (bStaminaExhausted && CurrentStamina >= MaxStamina * SprintReenableStaminaFraction)
		{
			bStaminaExhausted = false;
		}
	}

	Move->MaxWalkSpeed = bIsSprinting ? SprintingSpeed : DefaultWalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchingSpeed;
}

float AOutpostZeroCharacter::GetStaminaPercent() const
{
	return MaxSprintStamina > 0.0f ? FMath::Clamp(CurrentStamina / MaxSprintStamina, 0.0f, 1.0f) : 0.0f;
}

bool AOutpostZeroCharacter::CanJumpInternal_Implementation() const
{
	if (ShouldUseStealthMovement() && !bAllowJumping)
	{
		return false;
	}

	return Super::CanJumpInternal_Implementation();
}

void AOutpostZeroCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// ACharacter raises the body mesh by HalfHeightAdjust so it stays planted while the capsule shrinks.
	// The camera rides the first-person mesh's head socket, so without this the view stays at standing height.
	// Lower the first-person mesh by the capsule change plus the standing/crouched eye-height difference.
	if (FirstPersonMesh)
	{
		const AOutpostZeroCharacter* DefaultChar = GetDefault<AOutpostZeroCharacter>(GetClass());
		FVector CrouchedLocation = DefaultChar->GetFirstPersonMesh()->GetRelativeLocation();
		CrouchedLocation.Z -= HalfHeightAdjust + (DefaultChar->BaseEyeHeight - DefaultChar->CrouchedEyeHeight);
		FirstPersonMesh->SetRelativeLocation(CrouchedLocation);
	}
}

void AOutpostZeroCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// back to the standing first-person position
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetRelativeLocation(GetDefault<AOutpostZeroCharacter>(GetClass())->GetFirstPersonMesh()->GetRelativeLocation());
	}
}

void AOutpostZeroCharacter::DoSprintStart()
{
	bSprintInputHeld = true;
}

void AOutpostZeroCharacter::DoSprintEnd()
{
	bSprintInputHeld = false;
}

void AOutpostZeroCharacter::DoCrouchStart()
{
	bCrouchInputHeld = true;

	if (ShouldUseStealthMovement())
	{
		Crouch();
	}
}

void AOutpostZeroCharacter::DoCrouchEnd()
{
	bCrouchInputHeld = false;

	if (ShouldUseStealthMovement())
	{
		// request to stand; the movement component keeps the character crouched
		// (and retries every frame) while the standing capsule would be blocked by a ceiling
		UnCrouch();
	}
}

void AOutpostZeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AOutpostZeroCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AOutpostZeroCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOutpostZeroCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOutpostZeroCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AOutpostZeroCharacter::LookInput);

		// Sprinting (hold)
		if (SprintInputAction)
		{
			EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Started, this, &AOutpostZeroCharacter::DoSprintStart);
			EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Completed, this, &AOutpostZeroCharacter::DoSprintEnd);
			EnhancedInputComponent->BindAction(SprintInputAction, ETriggerEvent::Canceled, this, &AOutpostZeroCharacter::DoSprintEnd);
		}

		// Utility light (press to toggle)
		if (ToggleLightAction)
		{
			EnhancedInputComponent->BindAction(ToggleLightAction, ETriggerEvent::Started, this, &AOutpostZeroCharacter::ToggleUtilityLight);
		}

		// Crouching (hold)
		if (CrouchInputAction)
		{
			EnhancedInputComponent->BindAction(CrouchInputAction, ETriggerEvent::Started, this, &AOutpostZeroCharacter::DoCrouchStart);
			EnhancedInputComponent->BindAction(CrouchInputAction, ETriggerEvent::Completed, this, &AOutpostZeroCharacter::DoCrouchEnd);
			EnhancedInputComponent->BindAction(CrouchInputAction, ETriggerEvent::Canceled, this, &AOutpostZeroCharacter::DoCrouchEnd);
		}
	}
	else
	{
		UE_LOG(LogOutpostZero, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AOutpostZeroCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AOutpostZeroCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AOutpostZeroCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AOutpostZeroCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AOutpostZeroCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void AOutpostZeroCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
