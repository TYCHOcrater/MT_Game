// Fill out your copyright notice in the Description page of Project Settings.


#include "MTPlayerState.h"
#include "AMTCharacter.h"
#include "MTCharacterRegistry.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

AMTPlayerState::AMTPlayerState()
{
	bReplicates = true;
}

void AMTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMTPlayerState, CharacterDefIndex);
}

void AMTPlayerState::SetCharacterDefIndex(uint8 NewIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	const UMTCharacterRegistry* Registry = UMTCharacterRegistry::Get();
	if (!Registry || NewIndex >= Registry->Num())
	{
		return;
	}

	if (CharacterDefIndex == NewIndex)
	{
		return;
	}

	CharacterDefIndex = NewIndex;
	// Push to host's own pawn — OnRep doesn't fire on the authority that wrote the value.
	RefreshOwningPawnCharacter();
}

void AMTPlayerState::ServerRequestSetCharacterDefIndex_Implementation(uint8 NewIndex)
{
	SetCharacterDefIndex(NewIndex);
}

void AMTPlayerState::OnRep_CharacterDefIndex()
{
	RefreshOwningPawnCharacter();
}

void AMTPlayerState::RefreshOwningPawnCharacter()
{
	if (AAMTCharacter* Char = Cast<AAMTCharacter>(GetPawn()))
	{
		Char->RefreshCharacterFromPlayerState();
	}
}
