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

UCLASS()
class MULTIPLAYERTEST_API AMTHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowMTHUD = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	float NotificationDuration = 5.0f;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleHUD() { bShowMTHUD = !bShowMTHUD; }

	void PushNotification(const FString& Text);

	virtual void DrawHUD() override;

protected:
	TArray<FMTNotification> Notifications;
};
