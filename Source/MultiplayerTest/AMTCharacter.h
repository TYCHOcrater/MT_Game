// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AMTCharacter.generated.h"

class UCameraComponent;
class UChildActorComponent;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;
class AMTWeapon;
class AMTWeaponPickup;
class UMTCharacterDefinition;
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

	// === Spring-damper polish (STRAFTAT/Cruelty Squad feel) ===
	// Look sway uses a critically-damped spring instead of simple lerp; velocity inertia trails movement;
	// strafe roll tilts on lateral velocity; landing jolt pushes weapon down on touchdown.

	/** How much the weapon trails behind movement velocity. Higher = more visible inertia. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float WeaponInertiaScale = 0.05f;

	/** Spring stiffness for the inertia trail. Higher = catches up to movement faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float WeaponInertiaStiffness = 30.0f;

	/** Spring damping for the inertia trail. Higher = less overshoot when stopping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float WeaponInertiaDamping = 8.0f;

	/** Spring stiffness for look sway (replaces simple lerp). Higher = snappier response, less swimmy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float LookSwayStiffness = 60.0f;

	/** Spring damping for look sway. Lower = more overshoot/oscillation; higher = smoother settle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float LookSwayDamping = 12.0f;

	/** Roll degrees per cm/s of strafe velocity. Tilts weapon opposite strafe direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float StrafeRollFactor = 0.005f;

	/** Downward kick magnitude (cm) on landing from airborne. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.0"))
	float LandingJoltMagnitude = 4.0f;

	/** How fast the landing jolt decays back to zero. Higher = quicker recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponSway", meta = (ClampMin = "0.1"))
	float LandingJoltDecaySpeed = 8.0f;

	// === Recoil + spread ===

	/** Camera pitch kick (degrees) on each shot. Pushes the view UP. No auto-recovery — player compensates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0.0"))
	float RecoilVerticalKick = 1.2f;

	/** Max camera yaw kick (degrees, ±) on each shot. Random per shot for unpredictability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil", meta = (ClampMin = "0.0"))
	float RecoilHorizontalKick = 0.3f;

	/** Base bullet spread cone half-angle (degrees) — minimum even when not firing. Higher = less accurate hipfire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0.0"))
	float SpreadBaseAngleDeg = 0.5f;

	/** Spread added per shot (degrees), accumulates with sustained fire up to SpreadMaxAngleDeg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0.0"))
	float SpreadPerShotDeg = 0.4f;

	/** Maximum spread cap (degrees). Sustained firing won't push spread past this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0.0"))
	float SpreadMaxAngleDeg = 3.5f;

	/** Spread recovery rate (degrees/sec) when not firing. Higher = quicker bloom-down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spread", meta = (ClampMin = "0.0"))
	float SpreadRecoverDegPerSec = 4.0f;

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

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** Apply mesh + AnimBP from a CharacterDefinition. Safe on any net role; no-op if Def is null. */
	void ApplyCharacterDefinition(UMTCharacterDefinition* Def);

	/** Re-pull CharacterDefIndex from this pawn's PlayerState and apply via the registry. */
	void RefreshCharacterFromPlayerState();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UChildActorComponent> WeaponChild;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UChildActorComponent> WeaponChildSecondary;

	/** Third-person primary weapon mount, attached to the mesh's right-hand socket so remote players see a weapon in our hands.
	 *  Mirrors WeaponChild's class but with reversed visibility flags (visible to non-owner only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UChildActorComponent> WeaponChildThirdPerson;

	/** Third-person secondary (akimbo) weapon mount, attached to the mesh's left-hand socket. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UChildActorComponent> WeaponChildSecondaryThirdPerson;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> EmoteAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LeanLeftAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> LeanRightAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputAction> SlideAction;

	// === Lean ===

	/** Camera roll (degrees) at full lean — both directions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lean", meta = (ClampMin = "0.0"))
	float LeanRollDegrees = 12.0f;

	/** Camera horizontal offset (cm) at full lean. Camera moves laterally to peek past cover. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lean", meta = (ClampMin = "0.0"))
	float LeanLateralOffset = 25.0f;

	/** How fast the lean lerps in/out. Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lean", meta = (ClampMin = "0.1"))
	float LeanInterpSpeed = 10.0f;

	// === Slide ===

	/** Forward impulse magnitude applied on slide start (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0.0"))
	float SlideImpulseSpeed = 1500.0f;

	/** Reduced ground friction during slide (vs default ~8). Lower = slides longer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0.0"))
	float SlideGroundFriction = 0.5f;

	/** Max slide duration before forced exit (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0.1"))
	float SlideMaxDuration = 1.0f;

	/** Speed (cm/s) below which the slide auto-ends. Friction decays the actual velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0.0"))
	float SlideEndSpeedThreshold = 350.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsSliding, BlueprintReadOnly, Category = "Slide")
	bool bIsSliding = false;

	// === Emote (Fortnite-style: triggers dance montage + switches camera to 3P for the duration) ===

	/** Camera back-offset (cm) from the head while emoting — pulls view behind the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote", meta = (ClampMin = "0.0"))
	float EmoteCameraBackDistance = 200.0f;

	/** Camera up-offset (cm) added on top of the back-offset while emoting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote")
	float EmoteCameraUpOffset = 80.0f;

	/** How fast the camera lerps between FP and 3P-emote positions. Higher = snappier transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emote", meta = (ClampMin = "0.1"))
	float EmoteCameraInterpSpeed = 6.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsEmoting, BlueprintReadOnly, Category = "Emote")
	bool bIsEmoting = false;

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

	void Emote();

	void LeanLeftPressed();
	void LeanLeftReleased();
	void LeanRightPressed();
	void LeanRightReleased();

	void StartSlide();

	UFUNCTION(Server, Reliable)
	void ServerStartSlide();

	UFUNCTION()
	void OnRep_IsSliding();

	UFUNCTION(Server, Reliable)
	void ServerStartEmote();

	UFUNCTION(Server, Reliable)
	void ServerStopEmote();

	UFUNCTION()
	void OnRep_IsEmoting();

	void StopEmote();

	/** Cached montage from the active CharacterDefinition. Set in ApplyCharacterDefinition. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CachedEmoteMontage = nullptr;

	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bSprinting);

	/** Captured at BeginPlay so we can restore non-sprint speed without hardcoding. */
	float BaseWalkSpeed = 600.0f;

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireFX(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, bool bHitTarget, bool bWasCrit);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShowDamageNumber(FVector_NetQuantize Loc, int32 Damage, bool bWasCrit);

	UFUNCTION(Client, Unreliable)
	void ClientShowDamageDirection(FVector_NetQuantizeNormal AttackerDirWorld);

	UFUNCTION()
	void OnRep_Health();

	float LastFireTime = -1.0f;

	/** Server-only flag set by the shooter just before applying damage; read in HandleDeath to tag crit kills in the feed. */
	bool bLastIncomingDamageWasCrit = false;

	// Weapon sway state — local-only, captured from BP-overridden mount transforms in BeginPlay
	FVector PrimaryBaseLocation = FVector::ZeroVector;
	FVector SecondaryBaseLocation = FVector::ZeroVector;
	FRotator PrimaryBaseRotation = FRotator::ZeroRotator;
	FRotator SecondaryBaseRotation = FRotator::ZeroRotator;
	float BobTimeAccumulator = 0.0f;
	float IdleTimeAccumulator = 0.0f;
	FVector SwayCurrentOffset = FVector::ZeroVector;
	FRotator LastControlRotation = FRotator::ZeroRotator;

	// Spring-damper integration state (position + velocity for each weighted offset)
	FVector WeaponInertiaCurrent = FVector::ZeroVector;
	FVector WeaponInertiaVelocity = FVector::ZeroVector;
	FVector LookSwayCurrent = FVector::ZeroVector;
	FVector LookSwayVelocity = FVector::ZeroVector;
	float LandingJoltCurrent = 0.0f;
	bool bWasInAirLastFrame = false;

	/** Current spread cone half-angle. Server-side: grows on fire (capped), decays toward base in Tick. */
	float CurrentSpreadDeg = 0.0f;

	// Lean: -1 (full left) to +1 (full right). Held-state inputs accumulate into LeanInput each frame.
	// LeanInputTarget is replicated so remote clients can drive a spine-bend in their ABP for TPP lean.
	// Owner client sends discrete -1/0/+1 to server via ServerSetLeanInput; server DOREPLIFETIMEs to others.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lean")
	float LeanInputTarget = 0.0f;

	/** Smoothed lean value (-1..+1) — local per-instance Tick interpolation toward LeanInputTarget.
	 *  ABP reads this to drive spine-bone roll for TPP body lean. */
	UPROPERTY(BlueprintReadOnly, Category = "Lean")
	float LeanCurrent = 0.0f;

	bool bLeanLeftHeld = false;
	bool bLeanRightHeld = false;

	UFUNCTION(Server, Unreliable)
	void ServerSetLeanInput(float NewTarget);

	/** Helper: sets local LeanInputTarget and forwards to server (so it replicates to other clients). */
	void ApplyLeanInputChange(float NewTarget);

	// Slide state
	float SlideStartTime = -1.0f;
	float PreSlideGroundFriction = 8.0f;
	float PreSlideBrakingDecel = 2048.0f;
	void EndSlide();

	/** Refresh BlueprintReadOnly anim-state vars (Speed, ForwardSpeed, AimPitch, bIsInAir, etc.). Runs on all instances. */
	void UpdateAnimationState();

	/** Set bOnlyOwnerSee / bOwnerNoSee on the spawned child actors of WeaponChild* so FP weapons are owner-only and
	 *  3P weapons are non-owner-only. Idempotent; safe to call every tick (handles late-replicated child actors). */
	void RefreshWeaponVisibility();

	void HandleDeath(AController* Killer);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnDeath();

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
