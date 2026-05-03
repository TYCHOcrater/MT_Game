// Fill out your copyright notice in the Description page of Project Settings.

#include "MTWeapon.h"
#include "Components/StaticMeshComponent.h"

AMTWeapon::AMTWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
}

FVector AMTWeapon::GetMuzzleLocation() const
{
	if (Mesh && MuzzleSocketName != NAME_None && Mesh->DoesSocketExist(MuzzleSocketName))
	{
		return Mesh->GetSocketLocation(MuzzleSocketName);
	}
	return Mesh ? Mesh->GetComponentLocation() : GetActorLocation();
}
