// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTWeapon.generated.h"

class UStaticMeshComponent;

UCLASS()
class MULTIPLAYERTEST_API AMTWeapon : public AActor
{
	GENERATED_BODY()

public:
	AMTWeapon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Stats")
	float FireRange = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Stats", meta = (ClampMin = "0.0"))
	float FireDamageMin = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Stats", meta = (ClampMin = "0.0"))
	float FireDamageMax = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Stats")
	float FireInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Crit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CritChance = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Crit", meta = (ClampMin = "1.0"))
	float CritDamageMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FName MuzzleSocketName = TEXT("Muzzle");

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;
};
