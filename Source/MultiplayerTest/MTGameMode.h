// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MTGameMode.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYERTEST_API AMTGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMTGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
};
