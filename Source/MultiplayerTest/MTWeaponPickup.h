// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MTWeaponPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AMTWeapon;

UCLASS()
class MULTIPLAYERTEST_API AMTWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	AMTWeaponPickup();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<USphereComponent> OverlapSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Which weapon class will be granted to whoever picks this up. Replicated so clients can render the right mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_WeaponClass, Category = "Pickup")
	TSubclassOf<AMTWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float SpinDegreesPerSecond = 60.0f;

	/** Multiplier applied on top of the weapon mesh's own scale when this pickup renders. 1.0 = same size as the equipped weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0.01"))
	float ScaleMultiplier = 1.0f;

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_WeaponClass();

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyVisualFromWeaponClass();
};
