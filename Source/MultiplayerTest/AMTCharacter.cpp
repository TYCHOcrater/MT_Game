// Fill out your copyright notice in the Description page of Project Settings.


#include "AMTCharacter.h"
#include "MTHUD.h"
#include "MTGameInstance.h"
#include "MTWeapon.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
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

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
	if (MeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshAsset.Object);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		GetMesh()->SetRelativeScale3D(FVector(2.0f));
	}
}

// Called when the game starts or when spawned
void AAMTCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = MaxHealth;
	}
}

AMTWeapon* AAMTCharacter::GetCurrentWeapon() const
{
	return WeaponChild ? Cast<AMTWeapon>(WeaponChild->GetChildActor()) : nullptr;
}

// Called every frame
void AAMTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
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

void AAMTCharacter::ServerFire_Implementation(FVector_NetQuantize Start, FVector_NetQuantize Direction)
{
	AMTWeapon* Weapon = GetCurrentWeapon();
	if (!Weapon)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < Weapon->FireInterval)
	{
		return;
	}
	LastFireTime = Now;

	const FVector End = (FVector)Start + (FVector)Direction * Weapon->FireRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Weapon);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Params);
	const FVector ImpactEnd = bHit ? Hit.ImpactPoint : End;

	if (bHit && Hit.GetActor())
	{
		FPointDamageEvent DamageEvent(Weapon->FireDamage, Hit, Direction, nullptr);
		Hit.GetActor()->TakeDamage(Weapon->FireDamage, DamageEvent, GetController(), this);
	}

	const FVector MuzzleLoc = Weapon->GetMuzzleLocation();
	MulticastFireFX(FVector_NetQuantize(MuzzleLoc), FVector_NetQuantize(ImpactEnd), bHit);
}

void AAMTCharacter::MulticastFireFX_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit)
{
	const FColor LineColor = bHit ? FColor::Red : FColor::Yellow;
	DrawDebugLine(GetWorld(), Start, End, LineColor, false, 0.5f, 0, 1.0f);
	if (bHit)
	{
		DrawDebugSphere(GetWorld(), End, 8.0f, 8, FColor::Red, false, 0.5f);
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
		HandleDeath();
	}

	return Applied;
}

void AAMTCharacter::OnRep_Health()
{
	// Hook for HUD updates etc.
}

void AAMTCharacter::HandleDeath()
{
	MulticastOnDeath();

	UWorld* World = GetWorld();
	if (!World)
	{
		Destroy();
		return;
	}

	AController* DeadController = GetController();
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

