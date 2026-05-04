// Fill out your copyright notice in the Description page of Project Settings.

#include "MTWeaponPickup.h"
#include "AMTCharacter.h"
#include "MTWeapon.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

AMTWeaponPickup::AMTWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	OverlapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OverlapSphere"));
	SetRootComponent(OverlapSphere);
	OverlapSphere->SetSphereRadius(80.0f);
	OverlapSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	OverlapSphere->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(OverlapSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
}

void AMTWeaponPickup::BeginPlay()
{
	Super::BeginPlay();

	ApplyVisualFromWeaponClass();

	if (HasAuthority())
	{
		OverlapSphere->OnComponentBeginOverlap.AddDynamic(this, &AMTWeaponPickup::OnSphereOverlap);
	}
}

void AMTWeaponPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Mesh && SpinDegreesPerSecond != 0.0f)
	{
		Mesh->AddLocalRotation(FRotator(0.0f, SpinDegreesPerSecond * DeltaTime, 0.0f));
	}
}

void AMTWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMTWeaponPickup, WeaponClass);
}

void AMTWeaponPickup::OnRep_WeaponClass()
{
	ApplyVisualFromWeaponClass();
}

void AMTWeaponPickup::ApplyVisualFromWeaponClass()
{
	if (!Mesh || !WeaponClass)
	{
		return;
	}

	const AMTWeapon* WeaponCDO = WeaponClass.GetDefaultObject();
	if (!WeaponCDO || !WeaponCDO->Mesh)
	{
		return;
	}

	if (UStaticMesh* SourceMesh = WeaponCDO->Mesh->GetStaticMesh())
	{
		Mesh->SetStaticMesh(SourceMesh);
	}
	// Pull the weapon mesh component's authored scale + rotation so the pickup matches the equipped weapon's size.
	// Without this we'd render the raw asset at scale 1, which is enormous because BP_WeaponUzi scales its Mesh down.
	Mesh->SetRelativeScale3D(WeaponCDO->Mesh->GetRelativeScale3D() * ScaleMultiplier);
	Mesh->SetRelativeRotation(WeaponCDO->Mesh->GetRelativeRotation());
}

void AMTWeaponPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !WeaponClass)
	{
		return;
	}

	AAMTCharacter* Picker = Cast<AAMTCharacter>(Other);
	if (!Picker || Picker->IsDead())
	{
		return;
	}

	if (Picker->TryEquipWeapon(WeaponClass))
	{
		Destroy();
	}
}
