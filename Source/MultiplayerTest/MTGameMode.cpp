// Fill out your copyright notice in the Description page of Project Settings.


#include "MTGameMode.h"
#include "AMTCharacter.h"
#include "MTHUD.h"
#include "MTGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

AMTGameMode::AMTGameMode()
{
    // set default pawn class to our character class
    DefaultPawnClass = AAMTCharacter::StaticClass();
    HUDClass = AMTHUD::StaticClass();
    GameStateClass = AMTGameState::StaticClass();
}

void AMTGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    FString Name = TEXT("Player");
    if (NewPlayer && NewPlayer->PlayerState)
    {
        const FString PlayerName = NewPlayer->PlayerState->GetPlayerName();
        if (!PlayerName.IsEmpty())
        {
            Name = PlayerName;
        }
    }

    if (AMTGameState* GS = GetGameState<AMTGameState>())
    {
        GS->MulticastNotification(FString::Printf(TEXT("%s joined"), *Name));
    }
}

void AMTGameMode::Logout(AController* Exiting)
{
    FString Name = TEXT("Player");
    if (Exiting && Exiting->PlayerState)
    {
        const FString PlayerName = Exiting->PlayerState->GetPlayerName();
        if (!PlayerName.IsEmpty())
        {
            Name = PlayerName;
        }
    }

    if (AMTGameState* GS = GetGameState<AMTGameState>())
    {
        GS->MulticastNotification(FString::Printf(TEXT("%s left"), *Name));
    }

    Super::Logout(Exiting);
}




