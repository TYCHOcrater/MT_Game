// Fill out your copyright notice in the Description page of Project Settings.

#include "MTGameState.h"
#include "MTHUD.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void AMTGameState::MulticastNotification_Implementation(const FString& Message)
{
	UWorld* World = GetWorld();
	if (!World || !GEngine)
	{
		return;
	}

	if (APlayerController* PC = GEngine->GetFirstLocalPlayerController(World))
	{
		if (AMTHUD* HUD = Cast<AMTHUD>(PC->GetHUD()))
		{
			HUD->PushNotification(Message);
		}
	}
}
