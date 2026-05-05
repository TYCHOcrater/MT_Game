// Fill out your copyright notice in the Description page of Project Settings.


#include "AMTCharacter.h"
#include "MTHUD.h"
#include "MTGameInstance.h"
#include "MTGameState.h"
#include "MTWeapon.h"
#include "MTWeaponPickup.h"
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

	// Capture base mount positions for weapon sway. Done after the mirror above so the secondary's base
	// reflects its mirrored position. Sway adds offsets on top of these without losing the BP-tuned origin.
	if (WeaponChild)
	{
		PrimaryBaseLocation = WeaponChild->GetRelativeLocation();
	}
	if (WeaponChildSecondary)
	{
		SecondaryBaseLocation = WeaponChildSecondary->GetRelativeLocation();
	}
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
		return true;
	}

	// Otherwise try secondary
	if (WeaponChildSecondary && !WeaponChildSecondary->GetChildActorClass())
	{
		WeaponChildSecondary->SetChildActorClass(NewWeaponClass);
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
}

// Called every frame
void AAMTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Anim state must update for ALL instances — including remote pawns we render in third-person — so other
	// players' AnimBPs receive Speed/AimPitch/etc. ACharacter replicates RemoteViewPitch automatically.
	UpdateAnimationState();

	// Weapon sway is purely a local first-person feel effect — skip on remote pawns and the server's view of clients.
	if (!IsLocallyControlled() || !FirstPersonCamera)
	{
		return;
	}

	// Speed-scaled walk bob: amplitude ramps up with movement speed, frequency speeds up too.
	const UCharacterMovementComponent* CMC = GetCharacterMovement();
	const float MaxSpeed = CMC ? CMC->MaxWalkSpeed : 600.0f;
	const float CurSpeed = GetVelocity().Size2D();
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

	// Look sway: lag behind controller rotation deltas, then ease back
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const FRotator CurRot = PC ? PC->GetControlRotation() : FRotator::ZeroRotator;
	const FRotator RotDelta = (CurRot - LastControlRotation).GetNormalized();
	LastControlRotation = CurRot;

	const FVector SwayTarget(0.0f, -RotDelta.Yaw * WeaponSwayLookAmount, -RotDelta.Pitch * WeaponSwayLookAmount);
	SwayCurrentOffset = FMath::VInterpTo(SwayCurrentOffset, SwayTarget, DeltaTime, WeaponSwayInterpSpeed);

	const FVector Offset(
		IdleX,
		BobH + IdleY + SwayCurrentOffset.Y,
		BobV + IdleZ + SwayCurrentOffset.Z);

	if (WeaponChild)
	{
		WeaponChild->SetRelativeLocation(PrimaryBaseLocation + Offset);
	}
	if (WeaponChildSecondary)
	{
		WeaponChildSecondary->SetRelativeLocation(SecondaryBaseLocation + Offset);
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
}

void AAMTCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || Axis.IsNearlyZero())
	{
		return;
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

	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector Direction = FirstPersonCamera->GetForwardVector();
	ServerFire(Start, Direction);
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

	// Both barrels point the same direction (camera forward). Single trace, both guns contribute damage independently.
	const float TraceRange = FMath::Max(
		Primary ? Primary->FireRange : 0.0f,
		Secondary ? Secondary->FireRange : 0.0f);
	const FVector End = (FVector)Start + (FVector)Direction * TraceRange;

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
	const FColor LineColor = bHit ? FColor::Red : FColor::Yellow;
	DrawDebugLine(GetWorld(), Start, End, LineColor, false, 0.5f, 0, 1.0f);
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
	}
}

void AAMTCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAMTCharacter, Health);
}

float AAMTCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || IsDead())
	{
		return 0.0f;
	}

	const float Applied = FMath::Min(Health, DamageAmount);
	Health -= Applied;

	if (Health <= 0.0f)
	{
		Health = 0.0f;
		HandleDeath(EventInstigator);
	}

	return Applied;
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

