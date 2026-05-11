// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MTHUD.generated.h"

USTRUCT()
struct FMTNotification
{
	GENERATED_BODY()

	FString Text;
	float ExpireTime = 0.0f;
};

USTRUCT()
struct FMTKillEvent
{
	GENERATED_BODY()

	FString Text;
	float ExpireTime = 0.0f;
};

UCLASS()
class MULTIPLAYERTEST_API AMTHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowMTHUD = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	float NotificationDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|KillFeed")
	float KillFeedDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|HitMarker")
	float HitMarkerDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|DamageNumbers")
	bool bShowDamageNumbers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|DamageIndicator")
	float DamageIndicatorDuration = 1.2f;

	/** Radius (px) from screen center where the damage arc is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|DamageIndicator")
	float DamageIndicatorRadiusPx = 220.0f;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleHUD() { bShowMTHUD = !bShowMTHUD; }

	void PushNotification(const FString& Text);

	void PushKillEvent(const FString& Text);

	void ShowHitMarker(bool bIsCrit = false);

	/** Called by AMTCharacter::ClientShowDamageDirection. WorldDir points FROM victim TOWARD attacker. */
	void ShowDamageDirection(const FVector& WorldDir);

	virtual void DrawHUD() override;

protected:
	TArray<FMTNotification> Notifications;
	TArray<FMTKillEvent> KillFeed;
	float LastHitMarkerTime = -1000.0f;
	bool bLastHitWasCrit = false;

	FVector LastDamageDirWorld = FVector::ZeroVector;
	float LastDamageDirTime = -1000.0f;
};
