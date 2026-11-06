// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "OutpostZeroCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class USpotLightComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/** Broadcast when the suit-mounted utility light is switched on or off */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUtilityLightToggledDelegate, bool, bIsOn);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AOutpostZeroCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Suit-mounted utility light, attached to the first-person camera.
	 *  Intensity, radius, cone angles, colour and shadow casting are tuned on this component in Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpotLightComponent* UtilityLight;

protected:

	/** Toggle Utility Light Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ToggleLightAction;

	/** If false, this character never shows the utility light (template variants opt out in their constructors) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Utility Light")
	bool bEnableUtilityLight = true;

	/** Whether the utility light is on when play begins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Utility Light")
	bool bUtilityLightStartsOn = true;

	/** Current on/off state requested by the player */
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Utility Light")
	bool bUtilityLightOn = false;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Sprint Input Action (hold) */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* SprintInputAction;

	/** Crouch Input Action (hold) */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* CrouchInputAction;

	/** If true, this character uses the Outpost Zero walk / sprint / stamina / crouch rules while player controlled.
	 *  Template variants (Horror, Shooter) opt out in their constructors; AI-controlled pawns never use it. */
	UPROPERTY(EditAnywhere, Category="Movement|Stealth")
	bool bUseStealthMovement = true;

	/** Ground speed while walking */
	UPROPERTY(EditAnywhere, Category="Movement|Stealth", meta = (ClampMin = 0, Units = "cm/s"))
	float DefaultWalkSpeed = 250.0f;

	/** Ground speed while sprinting */
	UPROPERTY(EditAnywhere, Category="Movement|Stealth", meta = (ClampMin = 0, Units = "cm/s"))
	float SprintingSpeed = 500.0f;

	/** Ground speed while crouched */
	UPROPERTY(EditAnywhere, Category="Movement|Stealth", meta = (ClampMin = 0, Units = "cm/s"))
	float CrouchingSpeed = 150.0f;

	/** If false, jumping is disabled while stealth movement is active */
	UPROPERTY(EditAnywhere, Category="Movement|Stealth")
	bool bAllowJumping = false;

	/** Seconds of continuous sprinting available from full stamina */
	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = 0.1, Units = "s"))
	float MaxSprintStamina = 5.0f;

	/** Seconds to recover from empty to full stamina while not sprinting */
	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = 0.1, Units = "s"))
	float StaminaFullRecoveryTime = 8.0f;

	/** After stamina is fully depleted, sprinting stays locked until stamina recovers to this fraction of max */
	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = 0, ClampMax = 1))
	float SprintReenableStaminaFraction = 0.3f;

	/** Horizontal speed below which the character is not considered moving (no sprint, no stamina drain) */
	UPROPERTY(EditAnywhere, Category="Movement|Stamina", meta = (ClampMin = 0, Units = "cm/s"))
	float MinSprintMoveSpeed = 10.0f;

	/** Current sprint stamina, in seconds of sprint remaining */
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Movement|Stamina")
	float CurrentStamina = 0.0f;

	/** True while actually sprinting this frame */
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Movement|Stamina")
	bool bIsSprinting = false;

	/** True after stamina hit zero, until it recovers past SprintReenableStaminaFraction */
	UPROPERTY(VisibleInstanceOnly, Transient, Category="Movement|Stamina")
	bool bStaminaExhausted = false;

	/** Raw input state */
	bool bSprintInputHeld = false;
	bool bCrouchInputHeld = false;

public:
	AOutpostZeroCharacter();

	/** Per-frame stamina and speed update */
	virtual void Tick(float DeltaSeconds) override;

	/** Returns sprint stamina as a 0..1 fraction (for a future HUD) */
	UFUNCTION(BlueprintPure, Category="Movement|Stamina")
	float GetStaminaPercent() const;

	/** Returns true while the character is actually sprinting */
	UFUNCTION(BlueprintPure, Category="Movement|Stamina")
	bool IsSprinting() const { return bIsSprinting; }

	/** Called when the utility light is switched on or off (for future audio / UI) */
	UPROPERTY(BlueprintAssignable, Category="Utility Light")
	FUtilityLightToggledDelegate OnUtilityLightToggled;

	/** Switches the utility light on or off */
	UFUNCTION(BlueprintCallable, Category="Utility Light")
	void ToggleUtilityLight();

	/** Sets the utility light state explicitly */
	UFUNCTION(BlueprintCallable, Category="Utility Light")
	void SetUtilityLightOn(bool bOn);

	/** Returns true while the utility light is on */
	UFUNCTION(BlueprintPure, Category="Utility Light")
	bool IsUtilityLightOn() const { return bUtilityLightOn; }

	/** Returns the utility light component */
	USpotLightComponent* GetUtilityLight() const { return UtilityLight; }

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Blocks jumping while stealth movement is active and jumping is disallowed */
	virtual bool CanJumpInternal_Implementation() const override;

	/** Lowers the first-person mesh (and the camera on its head socket) when the capsule crouches */
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	/** Restores the first-person mesh to its standing position */
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	/** Re-evaluates utility light visibility when the pawn is possessed or unpossessed */
	virtual void NotifyControllerChanged() override;

	/** Applies the on/off state to the light; only player-controlled characters show it */
	void RefreshUtilityLight();

	/** Handles sprint press from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSprintStart();

	/** Handles sprint release from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSprintEnd();

	/** Handles crouch press from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoCrouchStart();

	/** Handles crouch release from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoCrouchEnd();

	/** True if the stealth movement rules apply to this character right now */
	bool ShouldUseStealthMovement() const;

	/** Updates stamina, sprint state and movement speeds (frame-rate independent) */
	void UpdateStealthMovement(float DeltaSeconds);

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

