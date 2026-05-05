// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AMTCharacter.generated.h"

class UCameraComponent;
class UChildActorComponent;
class UInputMappingContext;
class UInputAction;
class AMTWeapon;
class AMTWeaponPickup;
struct FInputActionValue;

UCLASS()
class MULTIPLAYERTEST_API AAMTCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAMTCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float MaxHealth = 100.0f;

	UFUNCTION(BlueprintPure, Category = "Combat")
	AMTWeapon* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	AMTWeapon* GetSecondaryWeapon() const;

	/** Class of pickup actor to spawn when this character drops its weapon. Set in BP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<AMTWeaponPickup> WeaponPickupClass;

	/** How far in front of the character (along their forward vector) the dropped pickup spawns, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float WeaponDropForwardDistance = 250.0f;

	/** Vertical offset relative to actor origin when spawning the pickup, in cm. Negative = below capsule center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float WeaponDropVerticalOffset = -30.0f;

	/** Peak amplitude of the walk bob (cm). Scales with current speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float WeaponBobAmplitude = 1.5f;

	/** Bob frequency in Hz at full sprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float WeaponBobFrequency = 8.0f;

	/** How much the weapon lags behind look rotation. Higher = more dramatic sway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float WeaponSwayLookAmount = 0.35f;

	/** How fast the weapon recovers from look sway. Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.1"))
	float WeaponSwayInterpSpeed = 6.0f;

	/** Idle sway amplitude (cm) — gentle motion when standing still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float WeaponIdleSwayAmplitude = 0.4f;

	/** Server-only. Tries to put the given weapon class into primary (if empty) then secondary. Returns true on success. */
	bool TryEquipWeapon(TSubclassOf<AMTWeapon> NewWeaponClass);

	/** Server-only. Drops the primary weapon as a pickup at the character's location and clears the slot. */
	void DropPrimaryWeapon();

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Combat")
	float Health = 100.0f;

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const { return Health <= 0.0f; }

	// === Animation State (read by AnimBP each frame; updated for both local and remote pawns) ===

	/** Unsigned 2D ground speed in cm/s. Drives Idle/Walk/Run/Sprint blend. */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float Speed = 0.0f;

	/** Signed local-space velocity along actor forward (+fwd / -back). For strafe blendspaces. */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float ForwardSpeed = 0.0f;

	/** Signed local-space velocity along actor right (+right / -left). For strafe blendspaces. */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float RightSpeed = 0.0f;

	/** Camera/aim pitch in degrees, clamped to [-90, 90]. Used for aim-offset poses. Replicated for remote pawns via ACharacter::RemoteViewPitch. */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	float AimPitch = 0.0f;

	/** True while the character is falling/jumping (CharacterMovement IsFalling). */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsInAir = false;

	/** True while sprint key is held (MaxWalkSpeed pushed above the captured base). */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bIsSprinting = false;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UChildActorComponent> WeaponChild;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UChildActorComponent> WeaponChildSecondary;

	/** Mapping context applied to the local player on possession. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ToggleHUDAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> ToggleMenuAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	/** MaxWalkSpeed while the sprint key is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 1100.0f;

	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartJump();
	void StopJump();
	void Fire();
	void ToggleHUD();
	void ToggleMenu();
	void StartSprint();
	void StopSprint();

	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bSprinting);

	/** Captured at BeginPlay so we can restore non-sprint speed without hardcoding. */
	float BaseWalkSpeed = 600.0f;

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireFX(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, bool bHitTarget, bool bWasCrit);

	UFUNCTION()
	void OnRep_Health();

	float LastFireTime = -1.0f;

	/** Server-only flag set by the shooter just before applying damage; read in HandleDeath to tag crit kills in the feed. */
	bool bLastIncomingDamageWasCrit = false;

	// Weapon sway state — local-only, captured from BP-overridden mount transforms in BeginPlay
	FVector PrimaryBaseLocation = FVector::ZeroVector;
	FVector SecondaryBaseLocation = FVector::ZeroVector;
	float BobTimeAccumulator = 0.0f;
	float IdleTimeAccumulator = 0.0f;
	FVector SwayCurrentOffset = FVector::ZeroVector;
	FRotator LastControlRotation = FRotator::ZeroRotator;

	/** Refresh BlueprintReadOnly anim-state vars (Speed, ForwardSpeed, AimPitch, bIsInAir, etc.). Runs on all instances. */
	void UpdateAnimationState();

	void HandleDeath(AController* Killer);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnDeath();

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
