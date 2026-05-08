// Fill out your copyright notice in the Description page of Project Settings.


#include "MTGameMode.h"
#include "AMTCharacter.h"
#include "MTGameInstance.h"
#include "MTHUD.h"
#include "MTGameState.h"
#include "MTPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

AMTGameMode::AMTGameMode()
{
    // set default pawn class to our character class
    DefaultPawnClass = AAMTCharacter::StaticClass();
    HUDClass = AMTHUD::StaticClass();
    GameStateClass = AMTGameState::StaticClass();
    PlayerStateClass = AMTPlayerState::StaticClass();
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

    // Host (local controller on the server) writes its own GameInstance preference into its PlayerState here.
    // Remote clients can't be read from the server side; they push their own preference via ServerRequestSetCharacterDefIndex
    // from AAMTCharacter::OnRep_PlayerState once the PS replicates back to them.
    if (NewPlayer && NewPlayer->IsLocalController())
    {
        if (UMTGameInstance* GI = GetGameInstance<UMTGameInstance>())
        {
            if (AMTPlayerState* PS = NewPlayer->GetPlayerState<AMTPlayerState>())
            {
                PS->SetCharacterDefIndex(GI->PreferredCharacterIndex);
            }
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




