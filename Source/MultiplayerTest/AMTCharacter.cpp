// Fill out your copyright notice in the Description page of Project Settings.


#include "AMTCharacter.h"
#include "MTHUD.h"
#include "MTGameInstance.h"
#include "MTGameState.h"
#include "MTPlayerState.h"
#include "MTCharacterDefinition.h"
#include "MTCharacterRegistry.h"
#include "MTWeapon.h"
#include "MTWeaponPickup.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"

// Sets default values
AAMTCharacter::AAMTCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// First-person camera attached to the character's head
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->FieldOfView = 100.0f;

	// Weapon mount — set ChildActorClass and tune the relative transform in the character BP viewport.
	WeaponChild = CreateDefaultSubobject<UChildActorComponent>(TEXT("WeaponChild"));
	WeaponChild->SetupAttachment(FirstPersonCamera);
	WeaponChild->SetRelativeLocation(FVector(20.0f, 12.0f, -10.0f));

	// Secondary mount — mirrored to the left. Empty by default; populated when the character picks up a 2nd gun.
	WeaponChildSecondary = CreateDefaultSubobject<UChildActorComponent>(TEXT("WeaponChildSecondary"));
	WeaponChildSecondary->SetupAttachment(FirstPersonCamera);
	WeaponChildSecondary->SetRelativeLocation(FVector(20.0f, -12.0f, -10.0f));
	WeaponChildSecondary->SetIsReplicated(true);
	WeaponChild->SetIsReplicated(true);

	// Third-person primary weapon mount — attached to the mesh's right-hand socket.
	WeaponChildThirdPerson = CreateDefaultSubobject<UChildActorComponent>(TEXT("WeaponChildThirdPerson"));
	WeaponChildThirdPerson->SetupAttachment(GetMesh(), TEXT("weapon_socket_r"));
	WeaponChildThirdPerson->SetIsReplicated(true);

	// Third-person secondary (akimbo) mount — attached to the mesh's left-hand socket.
	WeaponChildSecondaryThirdPerson = CreateDefaultSubobject<UChildActorComponent>(TEXT("WeaponChildSecondaryThirdPerson"));
	WeaponChildSecondaryThirdPerson->SetupAttachment(GetMesh(), TEXT("weapon_socket_l"));
	WeaponChildSecondaryThirdPerson->SetIsReplicated(true);

	// FPS-style rotation: camera follows controller yaw
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Move->MaxWalkSpeed = 750.0f;
	Move->JumpZVelocity = 600.0f;
	Move->AirControl = 0.8f;
	Move->GravityScale = 2.0f;
	Move->BrakingDecelerationWalking = 2048.0f;
	// Enable built-in crouch — used during slide to shrink the capsule so feet stay on ground
	// while the crouch-pose anim plays. Uncrouch on slide-end is smooth + blocks if ceiling above.
	Move->NavAgentProps.bCanCrouch = true;
	Move->SetCrouchedHalfHeight(60.0f);

	// Hide our own visible mesh from first-person view (still visible to other clients)
	GetMesh()->SetOwnerNoSee(true);

	// Standard "feet on ground, facing forward" transform for a UE-style skeleton.
	// SkeletalMesh asset + AnimClass are assigned in BP_MTCharacter.
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
}

// Called when the game starts or when spawned
void AAMTCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = MaxHealth;
	}

	// Capture whatever base walk speed the BP/CMC has tuned, so sprint can restore it cleanly
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		BaseWalkSpeed = CMC->MaxWalkSpeed;
	}

	// Mirror the primary weapon mount onto the secondary slot so an akimbo gun matches the primary's
	// scale and rotation (whatever the BP has tuned), just on the opposite side of the camera.
	if (WeaponChild && WeaponChildSecondary)
	{
		const FVector PrimaryLoc = WeaponChild->GetRelativeLocation();
		WeaponChildSecondary->SetRelativeLocation(FVector(PrimaryLoc.X, -PrimaryLoc.Y, PrimaryLoc.Z));
		WeaponChildSecondary->SetRelativeRotation(WeaponChild->GetRelativeRotation());
		WeaponChildSecondary->SetRelativeScale3D(WeaponChild->GetRelativeScale3D());
	}

	// Mirror WeaponChild's class onto the 3P mount (catches BP-default class case).
	if (WeaponChild && WeaponChildThirdPerson)
	{
		if (UClass* PrimaryClass = WeaponChild->GetChildActorClass())
		{
			if (!WeaponChildThirdPerson->GetChildActorClass())
			{
				WeaponChildThirdPerson->SetChildActorClass(PrimaryClass);
			}
		}
	}

	// Same mirror for secondary akimbo: BP-default secondary class → 3P left-hand mount.
	if (WeaponChildSecondary && WeaponChildSecondaryThirdPerson)
	{
		if (UClass* SecondaryClass = WeaponChildSecondary->GetChildActorClass())
		{
			if (!WeaponChildSecondaryThirdPerson->GetChildActorClass())
			{
				WeaponChildSecondaryThirdPerson->SetChildActorClass(SecondaryClass);
			}
		}
	}

	// Capture base mount transforms for weapon sway. Done after the mirror above so the secondary's base
	// reflects its mirrored position. Sway adds offsets on top of these without losing the BP-tuned origin.
	if (WeaponChild)
	{
		PrimaryBaseLocation = WeaponChild->GetRelativeLocation();
		PrimaryBaseRotation = WeaponChild->GetRelativeRotation();
	}
	if (WeaponChildSecondary)
	{
		SecondaryBaseLocation = WeaponChildSecondary->GetRelativeLocation();
		SecondaryBaseRotation = WeaponChildSecondary->GetRelativeRotation();
	}

	RefreshWeaponVisibility();
}

AMTWeapon* AAMTCharacter::GetCurrentWeapon() const
{
	return WeaponChild ? Cast<AMTWeapon>(WeaponChild->GetChildActor()) : nullptr;
}

AMTWeapon* AAMTCharacter::GetSecondaryWeapon() const
{
	return WeaponChildSecondary ? Cast<AMTWeapon>(WeaponChildSecondary->GetChildActor()) : nullptr;
}

bool AAMTCharacter::TryEquipWeapon(TSubclassOf<AMTWeapon> NewWeaponClass)
{
	if (!HasAuthority() || !NewWeaponClass)
	{
		return false;
	}

	// Primary slot first if it's empty
	if (WeaponChild && !WeaponChild->GetChildActorClass())
	{
		WeaponChild->SetChildActorClass(NewWeaponClass);
		if (WeaponChildThirdPerson)
		{
			WeaponChildThirdPerson->SetChildActorClass(NewWeaponClass);
		}
		return true;
	}

	// Otherwise try secondary
	if (WeaponChildSecondary && !WeaponChildSecondary->GetChildActorClass())
	{
		WeaponChildSecondary->SetChildActorClass(NewWeaponClass);
		if (WeaponChildSecondaryThirdPerson)
		{
			WeaponChildSecondaryThirdPerson->SetChildActorClass(NewWeaponClass);
		}
		return true;
	}

	return false;
}

void AAMTCharacter::DropPrimaryWeapon()
{
	if (!HasAuthority() || !WeaponChild)
	{
		return;
	}

	UClass* DroppedClass = WeaponChild->GetChildActorClass();
	if (!DroppedClass || !WeaponPickupClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Spawn the pickup ahead of the character, with the configurable forward + vertical offsets
	const FVector Forward = GetActorForwardVector();
	const FVector SpawnLoc = GetActorLocation() + Forward * WeaponDropForwardDistance + FVector(0.0f, 0.0f, WeaponDropVerticalOffset);
	const FRotator SpawnRot = FRotator::ZeroRotator;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = this;
	SpawnParams.bDeferConstruction = true;

	if (AMTWeaponPickup* Pickup = World->SpawnActor<AMTWeaponPickup>(WeaponPickupClass, SpawnLoc, SpawnRot, SpawnParams))
	{
		// Set the class BEFORE FinishSpawning so the server's BeginPlay sees it and applies the mesh.
		// (Without deferring, BeginPlay runs inside SpawnActor with WeaponClass still null, and the
		// server stays visually empty — only late-replicating clients see the mesh via OnRep.)
		Pickup->WeaponClass = DroppedClass;
		Pickup->FinishSpawning(FTransform(SpawnRot, SpawnLoc));
	}

	// Clear the slot — destroys the equipped weapon child actor
	WeaponChild->SetChildActorClass(nullptr);
	if (WeaponChildThirdPerson)
	{
		WeaponChildThirdPerson->SetChildActorClass(nullptr);
	}
}

void AAMTCharacter::RefreshWeaponVisibility()
{
	// UChildActorComponent doesn't reliably set the spawned actor's Owner to its outer actor — and
	// bOwnerNoSee / bOnlyOwnerSee are evaluated at render time using the primitive actor's Owner vs
	// the local PlayerController's controlled pawn. If Owner is wrong, the flags silently no-op.
	// Explicitly setting Owner = this character makes the engine see the FP/3P split correctly.
	auto Apply = [this](UChildActorComponent* Comp, bool bOnlyOwnerSee, bool bOwnerNoSee)
	{
		if (!Comp) return;
		AActor* Child = Comp->GetChildActor();
		if (!Child) return;
		if (Child->GetOwner() != this)
		{
			Child->SetOwner(this);
		}
		TArray<UPrimitiveComponent*> Prims;
		Child->GetComponents<UPrimitiveComponent>(Prims);
		for (UPrimitiveComponent* Prim : Prims)
		{
			Prim->SetOnlyOwnerSee(bOnlyOwnerSee);
			Prim->SetOwnerNoSee(bOwnerNoSee);
		}
	};

	// FP weapons: only owner sees (camera-attached). 3P weapons: only non-owner sees (body-attached).
	// During emote, hide FP weapons (camera goes to 3P, FP weapons would clip awkwardly) but show 3P
	// weapons to the owner too (they're now in 3P view of their own dancing character — gun-in-hand fits).
	const bool bEmoting = bIsEmoting;
	Apply(WeaponChild,                     /*bOnlyOwnerSee*/ true,  /*bOwnerNoSee*/ bEmoting);
	Apply(WeaponChildSecondary,            /*bOnlyOwnerSee*/ true,  /*bOwnerNoSee*/ bEmoting);
	Apply(WeaponChildThirdPerson,          /*bOnlyOwnerSee*/ false, /*bOwnerNoSee*/ !bEmoting);
	Apply(WeaponChildSecondaryThirdPerson, /*bOnlyOwnerSee*/ false, /*bOwnerNoSee*/ !bEmoting);
}

// Called every frame
void AAMTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Anim state must update for ALL instances — including remote pawns we render in third-person — so other
	// players' AnimBPs receive Speed/AimPitch/etc. ACharacter replicates RemoteViewPitch automatically.
	UpdateAnimationState();

	// Idempotent re-apply of weapon visibility (handles late-replicated child actors).
	RefreshWeaponVisibility();

	// Server-only spread decay — pulls CurrentSpreadDeg back toward the base angle when not firing.
	if (HasAuthority() && CurrentSpreadDeg > SpreadBaseAngleDeg)
	{
		CurrentSpreadDeg = FMath::Max(SpreadBaseAngleDeg, CurrentSpreadDeg - SpreadRecoverDegPerSec * DeltaTime);
	}

	// Smooth LeanCurrent toward replicated LeanInputTarget on EVERY instance (server + each client).
	// ABPs read LeanCurrent to drive TPP spine-bone bend — runs even on remote pawns who aren't locally controlled.
	LeanCurrent = FMath::FInterpTo(LeanCurrent, LeanInputTarget, DeltaTime, LeanInterpSpeed);

	// Server-only slide auto-end: exits when speed drops below threshold or max duration elapses.
	if (HasAuthority() && bIsSliding)
	{
		const float SlideElapsed = GetWorld()->GetTimeSeconds() - SlideStartTime;
		const float CurSpeed2D = GetVelocity().Size2D();
		if (SlideElapsed >= SlideMaxDuration || CurSpeed2D < SlideEndSpeedThreshold)
		{
			EndSlide();
		}
	}

	// Emote loop — runs on every instance (server + each client) since each plays its own local montage
	// via OnRep_IsEmoting. If the montage ends naturally and we're still in the emote state (because the
	// asset's section "Next Section" isn't self-looping), restart it locally. Cancelled by Move/Fire/Jump/G
	// via ServerStopEmote, which clears bIsEmoting and prevents re-trigger here.
	if (bIsEmoting && CachedEmoteMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			if (!AnimInstance->Montage_IsPlaying(CachedEmoteMontage))
			{
				PlayAnimMontage(CachedEmoteMontage);
			}
		}
	}

	// Weapon sway is purely a local first-person feel effect — skip on remote pawns and the server's view of clients.
	if (!IsLocallyControlled() || !FirstPersonCamera)
	{
		return;
	}

	// Emote camera: during emote, the camera ORBITS around the character (moves in a circle, always
	// looking at the body) based on controller yaw delta from the now-stationary capsule. Outside
	// emote, camera returns to normal FP position. Lerp covers both transitions smoothly.
	{
		const FVector NormalCamLoc(0.0f, 0.0f, 64.0f);
		FVector TargetCamLoc;
		if (bIsEmoting && Controller)
		{
			// Capsule isn't yawing during emote (bUseControllerRotationYaw was set false), so the
			// controller's yaw delta from the capsule's frozen yaw drives the orbit angle.
			const float CapsuleYaw = GetCapsuleComponent()->GetComponentRotation().Yaw;
			const float ControlYaw = Controller->GetControlRotation().Yaw;
			const FRotator OrbitRot(0.0f, ControlYaw - CapsuleYaw, 0.0f);
			const FVector OrbitOffset = OrbitRot.RotateVector(FVector(-EmoteCameraBackDistance, 0.0f, 0.0f));
			TargetCamLoc = OrbitOffset + FVector(0.0f, 0.0f, 64.0f + EmoteCameraUpOffset);
		}
		else
		{
			TargetCamLoc = NormalCamLoc;
		}
		const FVector CurCamLoc = FirstPersonCamera->GetRelativeLocation();
		FirstPersonCamera->SetRelativeLocation(FMath::VInterpTo(CurCamLoc, TargetCamLoc, DeltaTime, EmoteCameraInterpSpeed));
	}

	// FP lean: drive Roll on the controller's ControlRotation — the camera follows via
	// bUsePawnControlRotation, so it rolls with the controller. AddLocalRotation on the camera
	// wouldn't stick because the control-rotation override re-applies every frame.
	// LeanCurrent is smoothed above for all instances; this block just applies the roll for the local owner.
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			FRotator CtrlRot = PC->GetControlRotation();
			CtrlRot.Roll = LeanCurrent * LeanRollDegrees;
			PC->SetControlRotation(CtrlRot);
		}
	}

	// Speed-scaled walk bob: amplitude ramps up with movement speed, frequency speeds up too.
	const UCharacterMovementComponent* CMC = GetCharacterMovement();
	const float MaxSpeed = CMC ? CMC->MaxWalkSpeed : 600.0f;
	const FVector WorldVelocity = GetVelocity();
	const float CurSpeed = WorldVelocity.Size2D();
	const float SpeedRatio = FMath::Clamp(CurSpeed / FMath::Max(MaxSpeed, 1.0f), 0.0f, 1.0f);

	BobTimeAccumulator += DeltaTime * WeaponBobFrequency * (0.5f + 0.5f * SpeedRatio);
	IdleTimeAccumulator += DeltaTime;

	// Figure-8 bob: vertical at 2x frequency, horizontal at 1x — feels like footsteps
	const float BobV = FMath::Sin(BobTimeAccumulator * 2.0f * PI) * WeaponBobAmplitude * SpeedRatio;
	const float BobH = FMath::Sin(BobTimeAccumulator * PI) * WeaponBobAmplitude * 0.5f * SpeedRatio;

	// Idle sway: small triple-frequency drift, fades out as you move
	const float IdleAmp = WeaponIdleSwayAmplitude * (1.0f - SpeedRatio);
	const float IdleX = FMath::Sin(IdleTimeAccumulator * 1.2f) * IdleAmp * 0.5f;
	const float IdleY = FMath::Sin(IdleTimeAccumulator * 0.8f) * IdleAmp;
	const float IdleZ = FMath::Sin(IdleTimeAccumulator * 1.5f) * IdleAmp * 0.7f;

	// Spring-damper integrator: target = where we want, position+velocity advance physically.
	// Force = stiffness * (target - position) - damping * velocity. v += F*dt; p += v*dt.
	auto SpringStep = [](FVector& Position, FVector& Velocity, const FVector& Target, float Stiffness, float Damping, float dt)
	{
		const FVector Force = (Target - Position) * Stiffness - Velocity * Damping;
		Velocity += Force * dt;
		Position += Velocity * dt;
	};

	// === Velocity inertia: weapon trails behind acceleration, overshoots forward when stopping. ===
	// Local-space velocity (so forward/strafe are along character's facing, not world axes).
	const FVector LocalVelocity = GetActorTransform().InverseTransformVector(WorldVelocity);
	const FVector InertiaTarget = -LocalVelocity * WeaponInertiaScale;
	SpringStep(WeaponInertiaCurrent, WeaponInertiaVelocity, InertiaTarget, WeaponInertiaStiffness, WeaponInertiaDamping, DeltaTime);

	// === Look sway: spring-damper instead of simple lerp. Gives natural overshoot+settle on snap-aim. ===
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const FRotator CurRot = PC ? PC->GetControlRotation() : FRotator::ZeroRotator;
	const FRotator RotDelta = (CurRot - LastControlRotation).GetNormalized();
	LastControlRotation = CurRot;

	const FVector LookSwayTarget(0.0f, -RotDelta.Yaw * WeaponSwayLookAmount, -RotDelta.Pitch * WeaponSwayLookAmount);
	SpringStep(LookSwayCurrent, LookSwayVelocity, LookSwayTarget, LookSwayStiffness, LookSwayDamping, DeltaTime);

	// === Strafe roll: weapon tilts opposite strafe direction. ===
	const float StrafeRollDeg = -LocalVelocity.Y * StrafeRollFactor;

	// === Landing jolt: detect airborne→grounded transition, kick weapon down, decay. ===
	if (bWasInAirLastFrame && !bIsInAir)
	{
		LandingJoltCurrent = LandingJoltMagnitude;
	}
	LandingJoltCurrent = FMath::FInterpTo(LandingJoltCurrent, 0.0f, DeltaTime, LandingJoltDecaySpeed);
	bWasInAirLastFrame = bIsInAir;

	// Combine all sources into a single offset applied to the weapon mount.
	const FVector Offset(
		IdleX + WeaponInertiaCurrent.X,
		BobH + IdleY + LookSwayCurrent.Y + WeaponInertiaCurrent.Y,
		BobV + IdleZ + LookSwayCurrent.Z + WeaponInertiaCurrent.Z - LandingJoltCurrent);

	const FRotator RollDelta(0.0f, 0.0f, StrafeRollDeg);

	if (WeaponChild)
	{
		WeaponChild->SetRelativeLocation(PrimaryBaseLocation + Offset);
		WeaponChild->SetRelativeRotation(PrimaryBaseRotation + RollDelta);
	}
	if (WeaponChildSecondary)
	{
		WeaponChildSecondary->SetRelativeLocation(SecondaryBaseLocation + Offset);
		WeaponChildSecondary->SetRelativeRotation(SecondaryBaseRotation + RollDelta);
	}
}

void AAMTCharacter::UpdateAnimationState()
{
	const UCharacterMovementComponent* CMC = GetCharacterMovement();

	const FVector Vel = GetVelocity();
	Speed = Vel.Size2D();

	// Project velocity into actor-local space. Yaw is driven by control rotation (bUseControllerRotationYaw),
	// so this gives proper signed forward/right components for strafe blendspaces.
	const FVector LocalVel = GetActorRotation().UnrotateVector(Vel);
	ForwardSpeed = LocalVel.X;
	RightSpeed = LocalVel.Y;

	bIsInAir = CMC && CMC->IsFalling();
	bIsSprinting = CMC && CMC->MaxWalkSpeed > BaseWalkSpeed + 1.0f;

	// AimPitch: locally controlled and the server use the live control rotation. Remote sim proxies read
	// the replicated, packed view pitch via APawn::GetRemoteViewPitch() (returns degrees, already unpacked).
	if (IsLocallyControlled() || HasAuthority())
	{
		if (const AController* C = GetController())
		{
			AimPitch = FRotator::NormalizeAxis(C->GetControlRotation().Pitch);
		}
	}
	else
	{
		AimPitch = FRotator::NormalizeAxis(GetRemoteViewPitch());
	}

	AimPitch = FMath::ClampAngle(AimPitch, -90.0f, 90.0f);
}

void AAMTCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	// Push our locally chosen character up to the server. PlayerState may not be valid yet on first
	// possess for a remote client; OnRep_PlayerState below also makes the call once it arrives.
	if (IsLocallyControlled())
	{
		if (AMTPlayerState* PS = GetPlayerState<AMTPlayerState>())
		{
			if (UMTGameInstance* GI = GetGameInstance<UMTGameInstance>())
			{
				PS->ServerRequestSetCharacterDefIndex(GI->PreferredCharacterIndex);
			}
		}
	}
}

void AAMTCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server: PS now exists for this pawn — apply whatever character index it carries.
	RefreshCharacterFromPlayerState();
}

void AAMTCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client: PS just became valid — apply the replicated character index.
	RefreshCharacterFromPlayerState();

	// Local client: also push our preference up now that PS is reachable.
	if (IsLocallyControlled())
	{
		if (AMTPlayerState* PS = GetPlayerState<AMTPlayerState>())
		{
			if (UMTGameInstance* GI = GetGameInstance<UMTGameInstance>())
			{
				PS->ServerRequestSetCharacterDefIndex(GI->PreferredCharacterIndex);
			}
		}
	}
}

void AAMTCharacter::RefreshCharacterFromPlayerState()
{
	const AMTPlayerState* PS = GetPlayerState<AMTPlayerState>();
	if (!PS)
	{
		return;
	}
	const UMTCharacterRegistry* Registry = UMTCharacterRegistry::Get();
	if (!Registry)
	{
		return;
	}
	if (UMTCharacterDefinition* Def = Registry->LoadDefinition(PS->CharacterDefIndex))
	{
		ApplyCharacterDefinition(Def);
	}
}

void AAMTCharacter::ApplyCharacterDefinition(UMTCharacterDefinition* Def)
{
	if (!Def)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	if (USkeletalMesh* SK = Def->Mesh.LoadSynchronous())
	{
		MeshComp->SetSkeletalMesh(SK);
	}
	if (UClass* AnimClass = Def->AnimBP.LoadSynchronous())
	{
		MeshComp->SetAnimInstanceClass(AnimClass);
	}

	MeshComp->SetRelativeLocation(Def->MeshRelativeLocation);
	MeshComp->SetRelativeRotation(Def->MeshRelativeRotation);
	MeshComp->SetRelativeScale3D(FVector(Def->MeshUniformScale));

	// Cache the per-character emote montage for ServerStartEmote to play. Synchronous load is fine here
	// — possession is a rare event and the montage is small.
	CachedEmoteMontage = Def->EmoteMontage.LoadSynchronous();
}

void AAMTCharacter::Emote()
{
	// G is a toggle: if already emoting, stop. Otherwise start.
	if (bIsEmoting)
	{
		ServerStopEmote();
	}
	else
	{
		ServerStartEmote();
	}
}

void AAMTCharacter::ServerStartEmote_Implementation()
{
	// Already emoting, or no montage configured for this character → nothing to do.
	if (bIsEmoting || !CachedEmoteMontage)
	{
		return;
	}

	bIsEmoting = true;
	// OnRep_IsEmoting fires automatically on clients via the ReplicatedUsing notify; manually invoke
	// it on server (or listen-server's local view) to drive the same camera/visibility/montage logic.
	OnRep_IsEmoting();
}

void AAMTCharacter::ServerStopEmote_Implementation()
{
	StopEmote();
}

void AAMTCharacter::StopEmote()
{
	if (!HasAuthority() || !bIsEmoting)
	{
		return;
	}
	bIsEmoting = false;
	OnRep_IsEmoting();
}

// === Lean ===
// Local-only feel effect (FP-side): camera rolls + offsets laterally based on held keys.
// LeanInputTarget is set discretely from key state (-1/0/+1); LeanCurrent is the smoothed value
// applied to the camera in Tick. TPP spine rotation would need an anim post-process layer in the
// ABP — not done yet, so others don't see the lean.

void AAMTCharacter::ApplyLeanInputChange(float NewTarget)
{
	LeanInputTarget = NewTarget;
	if (!HasAuthority())
	{
		ServerSetLeanInput(NewTarget);
	}
}

void AAMTCharacter::LeanLeftPressed()  { bLeanLeftHeld = true;  ApplyLeanInputChange(bLeanRightHeld ? 0.0f : -1.0f); }
void AAMTCharacter::LeanLeftReleased() { bLeanLeftHeld = false; ApplyLeanInputChange(bLeanRightHeld ?  1.0f :  0.0f); }
void AAMTCharacter::LeanRightPressed()  { bLeanRightHeld = true;  ApplyLeanInputChange(bLeanLeftHeld ? 0.0f :  1.0f); }
void AAMTCharacter::LeanRightReleased() { bLeanRightHeld = false; ApplyLeanInputChange(bLeanLeftHeld ? -1.0f :  0.0f); }

void AAMTCharacter::ServerSetLeanInput_Implementation(float NewTarget)
{
	LeanInputTarget = FMath::Clamp(NewTarget, -1.0f, 1.0f);  // server clamps for safety
}

// === Slide ===
// Detect on key press: if currently moving with non-trivial speed, kick a forward impulse and lower
// ground friction so the player coasts. Uses bIsSliding (replicated) so remote viewers eventually
// get a Crouch-style pose via ABP reading the flag (TBD wire up in ABP later).

void AAMTCharacter::StartSlide()
{
	if (bIsSliding || !GetCharacterMovement())
	{
		return;
	}
	const float CurSpeed2D = GetVelocity().Size2D();
	if (CurSpeed2D < 200.0f)  // need some forward momentum to slide
	{
		return;
	}
	ServerStartSlide();
}

void AAMTCharacter::ServerStartSlide_Implementation()
{
	if (bIsSliding || !GetCharacterMovement())
	{
		return;
	}
	UCharacterMovementComponent* CMC = GetCharacterMovement();

	// Remember pre-slide values to restore on end.
	PreSlideGroundFriction = CMC->GroundFriction;
	PreSlideBrakingDecel   = CMC->BrakingDecelerationWalking;

	CMC->GroundFriction = SlideGroundFriction;
	CMC->BrakingDecelerationWalking = 200.0f;  // small braking so slide actually decays

	// Forward impulse in the character's facing direction.
	const FVector Forward = GetActorForwardVector();
	CMC->Velocity = Forward * SlideImpulseSpeed + FVector(0, 0, CMC->Velocity.Z);

	// Crouch so the capsule shrinks — character drops to ground level and the crouch-pose anim no
	// longer leaves the feet hovering. CMC replicates crouch state automatically.
	Crouch();

	bIsSliding = true;
	OnRep_IsSliding();  // ensure local listen-server runs the OnRep logic too
	SlideStartTime = GetWorld()->GetTimeSeconds();
}

void AAMTCharacter::EndSlide()
{
	if (!HasAuthority() || !bIsSliding)
	{
		return;
	}
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->GroundFriction = PreSlideGroundFriction;
		CMC->BrakingDecelerationWalking = PreSlideBrakingDecel;
	}
	// UnCrouch is smooth and self-blocks if there's a ceiling above — won't pop the character through geometry.
	UnCrouch();
	bIsSliding = false;
	OnRep_IsSliding();
}

void AAMTCharacter::OnRep_IsSliding()
{
	// Hook for ABP / camera height changes etc. — currently no visual side effect.
}

void AAMTCharacter::OnRep_IsEmoting()
{
	// Show our own body to ourselves while emoting (normally hidden by SetOwnerNoSee in the ctor).
	// Weapon visibility is handled in RefreshWeaponVisibility (which runs every tick and reads bIsEmoting).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetOwnerNoSee(!bIsEmoting);
	}

	// Decouple body yaw from controller during emote. The camera still rotates with controller (its own
	// bUsePawnControlRotation flag reads control rotation directly), so the player can orbit around their
	// stationary dancing character. Restored when emote ends — character will snap back to controller yaw.
	bUseControllerRotationYaw = !bIsEmoting;

	// Drive the montage from OnRep so every client (including the originator, who's skipped by
	// ACharacter's ReplicatedAnimMontage owner-skip) plays/stops the dance locally.
	if (bIsEmoting)
	{
		if (CachedEmoteMontage)
		{
			PlayAnimMontage(CachedEmoteMontage);
		}
	}
	else
	{
		StopAnimMontage(CachedEmoteMontage);
	}
}

void AAMTCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || Axis.IsNearlyZero())
	{
		return;
	}

	// Movement input cancels any active emote — player intent overrides the dance.
	if (bIsEmoting)
	{
		ServerStopEmote();
	}

	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AAMTCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AAMTCharacter::StartJump()
{
	if (bIsEmoting)
	{
		ServerStopEmote();
	}
	Jump();
}

void AAMTCharacter::StopJump()
{
	StopJumping();
}

void AAMTCharacter::Fire()
{
	if (!FirstPersonCamera)
	{
		return;
	}

	// Firing cancels any active emote — same intent-override as movement.
	if (bIsEmoting)
	{
		ServerStopEmote();
	}

	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector Direction = FirstPersonCamera->GetForwardVector();
	ServerFire(Start, Direction);

	// Local-only camera kick — instant, no auto-recovery. Player compensates by aiming back down.
	// Server doesn't need to know about this; it's purely a feel effect for the shooter's view.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator Ctrl = PC->GetControlRotation();
		Ctrl.Pitch -= RecoilVerticalKick;  // negative pitch = look up (UE convention)
		Ctrl.Yaw   += FMath::FRandRange(-RecoilHorizontalKick, RecoilHorizontalKick);
		PC->SetControlRotation(Ctrl);
	}
}

void AAMTCharacter::ServerFire_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction)
{
	AMTWeapon* Primary = GetCurrentWeapon();
	AMTWeapon* Secondary = GetSecondaryWeapon();

	// Need at least one equipped weapon to fire
	if (!Primary && !Secondary)
	{
		return;
	}

	// Use the shorter of the two FireIntervals for the cooldown — both barrels fire on the same trigger pull
	const float FireInterval = FMath::Min(
		Primary ? Primary->FireInterval : FLT_MAX,
		Secondary ? Secondary->FireInterval : FLT_MAX);

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < FireInterval)
	{
		return;
	}
	LastFireTime = Now;

	// Apply bullet spread: randomize the direction within a cone whose half-angle is CurrentSpreadDeg.
	// Server is authoritative for the spread roll — the client doesn't know which way the bullet veered.
	// Then grow spread for next shot (capped). Tick decays it back toward base when not firing.
	const float ConeRad = FMath::DegreesToRadians(FMath::Max(SpreadBaseAngleDeg, CurrentSpreadDeg));
	const FVector SpreadDir = (ConeRad > KINDA_SMALL_NUMBER) ? FMath::VRandCone((FVector)Direction, ConeRad).GetSafeNormal() : (FVector)Direction;
	CurrentSpreadDeg = FMath::Min(SpreadMaxAngleDeg, CurrentSpreadDeg + SpreadPerShotDeg);

	// Both barrels point the same (spread-adjusted) direction. Single trace, both guns contribute damage independently.
	const float TraceRange = FMath::Max(
		Primary ? Primary->FireRange : 0.0f,
		Secondary ? Secondary->FireRange : 0.0f);
	const FVector End = (FVector)Start + SpreadDir * TraceRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (Primary) Params.AddIgnoredActor(Primary);
	if (Secondary) Params.AddIgnoredActor(Secondary);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Params);
	const FVector ImpactEnd = bHit ? Hit.ImpactPoint : End;
	AAMTCharacter* Victim = bHit ? Cast<AAMTCharacter>(Hit.GetActor()) : nullptr;
	const bool bHitTarget = bHit && Hit.GetActor() && Hit.GetActor()->IsA<APawn>();

	// Roll crit + damage independently for each equipped barrel
	auto RollShot = [&](AMTWeapon* W) -> TPair<float, bool>
	{
		if (!W) return {0.0f, false};
		const float Base = FMath::FRandRange(W->FireDamageMin, W->FireDamageMax);
		const bool bCrit = FMath::FRand() < W->CritChance;
		return {Base * (bCrit ? W->CritDamageMultiplier : 1.0f), bCrit};
	};

	const TPair<float, bool> PrimaryShot = RollShot(Primary);
	const TPair<float, bool> SecondaryShot = RollShot(Secondary);
	const bool bAnyCrit = PrimaryShot.Value || SecondaryShot.Value;

	// Apply damage from each equipped barrel. Drop-on-crit fires before damage so the weapon is dropped
	// regardless of whether the shot is lethal. Drops at most once per ServerFire call.
	if (Victim)
	{
		if (bAnyCrit && Victim->GetCurrentWeapon())
		{
			Victim->DropPrimaryWeapon();
		}

		auto ApplyShot = [&](float Damage, bool bCrit)
		{
			if (Damage <= 0.0f || Victim->IsDead()) return;
			Victim->bLastIncomingDamageWasCrit = bCrit;
			FPointDamageEvent DamageEvent(Damage, Hit, Direction, nullptr);
			Victim->TakeDamage(Damage, DamageEvent, GetController(), this);
			// Show floating damage number at the hit point for everyone to see.
			MulticastShowDamageNumber(FVector_NetQuantize(Hit.ImpactPoint), FMath::RoundToInt(Damage), bCrit);
		};

		ApplyShot(PrimaryShot.Key, PrimaryShot.Value);
		ApplyShot(SecondaryShot.Key, SecondaryShot.Value);
	}

	// One FX broadcast per barrel — different muzzle origins so two tracer lines render
	if (Primary)
	{
		const FVector MuzzleLoc = Primary->GetMuzzleLocation();
		MulticastFireFX(FVector_NetQuantize(MuzzleLoc), FVector_NetQuantize(ImpactEnd), bHit, bHitTarget, PrimaryShot.Value);
	}
	if (Secondary)
	{
		const FVector MuzzleLoc = Secondary->GetMuzzleLocation();
		MulticastFireFX(FVector_NetQuantize(MuzzleLoc), FVector_NetQuantize(ImpactEnd), bHit, bHitTarget, SecondaryShot.Value);
	}
}

void AAMTCharacter::MulticastFireFX_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, bool bHitTarget, bool bWasCrit)
{
	// Server passes the FP weapon's muzzle as Start. That's at camera height (FP weapon is camera-attached),
	// which looks correct from the local owner's POV — they see their own FP weapon firing.
	// But remote viewers see the body-attached 3P weapon, so trace from FP muzzle would appear to come from
	// the character's face. Override with the 3P weapon's muzzle on remote clients.
	FVector RenderStart = Start;
	if (!IsLocallyControlled() && WeaponChildThirdPerson)
	{
		if (AMTWeapon* TpWeapon = Cast<AMTWeapon>(WeaponChildThirdPerson->GetChildActor()))
		{
			RenderStart = TpWeapon->GetMuzzleLocation();
		}
	}

	const FColor LineColor = bHit ? FColor::Red : FColor::Yellow;
	DrawDebugLine(GetWorld(), RenderStart, End, LineColor, false, 0.5f, 0, 1.0f);
	if (bHit)
	{
		DrawDebugSphere(GetWorld(), End, 8.0f, 8, FColor::Red, false, 0.5f);
	}

	if (bHitTarget && IsLocallyControlled())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD()))
			{
				HUD->ShowHitMarker(bWasCrit);
			}
		}
	}
}

void AAMTCharacter::MulticastShowDamageNumber_Implementation(FVector_NetQuantize Loc, int32 Damage, bool bWasCrit)
{
	// Per-client toggle: if this client's HUD has damage numbers disabled, skip drawing on this client.
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* LocalPC = World->GetFirstPlayerController())
		{
			if (AMTHUD* HUD = Cast<AMTHUD>(LocalPC->GetHUD()))
			{
				if (!HUD->bShowDamageNumbers)
				{
					return;
				}
			}
		}
	}

	// Float a damage number at the impact point. Crit = red + "!N!", normal = yellow + "N".
	// Uses DrawDebugString to match the existing MulticastFireFX debug-draw pattern; can be swapped for a
	// proper UMG world-space widget later.
	const FColor Color = bWasCrit ? FColor::Red : FColor::Yellow;
	const FString Text = bWasCrit ? FString::Printf(TEXT("!%d!"), Damage) : FString::Printf(TEXT("%d"), Damage);
	DrawDebugString(GetWorld(), (FVector)Loc + FVector(0.0f, 0.0f, 10.0f), Text, nullptr, Color, /*duration*/ 1.2f, /*bDrawShadow*/ true);
}

void AAMTCharacter::ToggleHUD()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD()))
		{
			HUD->ToggleHUD();
		}
	}
}

void AAMTCharacter::ToggleMenu()
{
	if (UMTGameInstance* GI = GetGameInstance<UMTGameInstance>())
	{
		GI->ToggleMainMenu();
	}
}

void AAMTCharacter::StartSprint()
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = SprintSpeed;
	}
	if (!HasAuthority())
	{
		ServerSetSprint(true);
	}
}

void AAMTCharacter::StopSprint()
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = BaseWalkSpeed;
	}
	if (!HasAuthority())
	{
		ServerSetSprint(false);
	}
}

void AAMTCharacter::ServerSetSprint_Implementation(bool bSprinting)
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = bSprinting ? SprintSpeed : BaseWalkSpeed;
	}
}

// Called to bind functionality to input
void AAMTCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAMTCharacter::Move);
		}
		if (LookAction)
		{
			EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAMTCharacter::Look);
		}
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AAMTCharacter::StartJump);
			EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &AAMTCharacter::StopJump);
		}
		if (FireAction)
		{
			EIC->BindAction(FireAction, ETriggerEvent::Triggered, this, &AAMTCharacter::Fire);
		}
		if (ToggleHUDAction)
		{
			EIC->BindAction(ToggleHUDAction, ETriggerEvent::Started, this, &AAMTCharacter::ToggleHUD);
		}
		if (ToggleMenuAction)
		{
			EIC->BindAction(ToggleMenuAction, ETriggerEvent::Started, this, &AAMTCharacter::ToggleMenu);
		}
		if (SprintAction)
		{
			EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &AAMTCharacter::StartSprint);
			EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &AAMTCharacter::StopSprint);
		}
		if (EmoteAction)
		{
			EIC->BindAction(EmoteAction, ETriggerEvent::Started, this, &AAMTCharacter::Emote);
		}
		if (LeanLeftAction)
		{
			EIC->BindAction(LeanLeftAction, ETriggerEvent::Started,   this, &AAMTCharacter::LeanLeftPressed);
			EIC->BindAction(LeanLeftAction, ETriggerEvent::Completed, this, &AAMTCharacter::LeanLeftReleased);
		}
		if (LeanRightAction)
		{
			EIC->BindAction(LeanRightAction, ETriggerEvent::Started,   this, &AAMTCharacter::LeanRightPressed);
			EIC->BindAction(LeanRightAction, ETriggerEvent::Completed, this, &AAMTCharacter::LeanRightReleased);
		}
		if (SlideAction)
		{
			EIC->BindAction(SlideAction, ETriggerEvent::Started, this, &AAMTCharacter::StartSlide);
		}
	}
}

void AAMTCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAMTCharacter, Health);
	DOREPLIFETIME(AAMTCharacter, bIsEmoting);
	DOREPLIFETIME(AAMTCharacter, bIsSliding);
	DOREPLIFETIME(AAMTCharacter, LeanInputTarget);
}

float AAMTCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || IsDead())
	{
		return 0.0f;
	}

	const float Applied = FMath::Min(Health, DamageAmount);
	Health -= Applied;

	// Tell the victim's local client which direction the attacker is in — HUD draws a directional
	// damage indicator from this. Horizontal-plane direction is enough; ignore Z.
	if (DamageCauser && DamageCauser != this)
	{
		FVector DirToAttacker = DamageCauser->GetActorLocation() - GetActorLocation();
		DirToAttacker.Z = 0.0f;
		DirToAttacker = DirToAttacker.GetSafeNormal();
		if (!DirToAttacker.IsNearlyZero())
		{
			ClientShowDamageDirection(FVector_NetQuantizeNormal(DirToAttacker));
		}
	}

	if (Health <= 0.0f)
	{
		Health = 0.0f;
		HandleDeath(EventInstigator);
	}

	return Applied;
}

void AAMTCharacter::ClientShowDamageDirection_Implementation(FVector_NetQuantizeNormal AttackerDirWorld)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD()))
		{
			HUD->ShowDamageDirection((FVector)AttackerDirWorld);
		}
	}
}

void AAMTCharacter::OnRep_Health()
{
	// Hook for HUD updates etc.
}

void AAMTCharacter::HandleDeath(AController* Killer)
{
	MulticastOnDeath();

	UWorld* World = GetWorld();
	if (!World)
	{
		Destroy();
		return;
	}

	AController* DeadController = GetController();

	// Broadcast kill event to all clients
	if (AMTGameState* GS = World->GetGameState<AMTGameState>())
	{
		auto GetPlayerName = [](AController* C) -> FString
		{
			if (C && C->PlayerState)
			{
				const FString N = C->PlayerState->GetPlayerName();
				if (!N.IsEmpty()) return N;
			}
			return TEXT("Player");
		};

		const FString VictimName = GetPlayerName(DeadController);
		const FString CritSuffix = bLastIncomingDamageWasCrit ? TEXT(" [CRIT]") : TEXT("");
		FString Text;
		if (Killer && Killer != DeadController)
		{
			const FString KillerName = GetPlayerName(Killer);
			Text = FString::Printf(TEXT("%s -> %s%s"), *KillerName, *VictimName, *CritSuffix);
		}
		else
		{
			Text = FString::Printf(TEXT("%s died"), *VictimName);
		}
		GS->MulticastKillEvent(Text);
	}

	bLastIncomingDamageWasCrit = false;

	if (DeadController)
	{
		DeadController->UnPossess();
	}

	// Despawn the corpse after a short delay
	SetLifeSpan(2.0f);

	TWeakObjectPtr<AController> WeakController(DeadController);
	TWeakObjectPtr<UWorld> WeakWorld(World);

	FTimerHandle RespawnTimer;
	World->GetTimerManager().SetTimer(RespawnTimer, [WeakController, WeakWorld]()
	{
		if (!WeakWorld.IsValid())
		{
			return;
		}
		AGameModeBase* GM = WeakWorld->GetAuthGameMode();
		AController* PC = WeakController.Get();
		if (GM && PC)
		{
			GM->RestartPlayer(PC);
		}
	}, 2.0f, false);
}

void AAMTCharacter::MulticastOnDeath_Implementation()
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetSimulatePhysics(false);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->DisableInput(PC);
	}
}

