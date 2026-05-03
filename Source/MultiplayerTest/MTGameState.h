// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MTGameState.generated.h"

UCLASS()
class MULTIPLAYERTEST_API AMTGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastNotification(const FString& Message);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastKillEvent(const FString& Message);
};
