// Fill out your copyright notice in the Description page of Project Settings.


#include "MTCharacterRegistry.h"
#include "MTCharacterDefinition.h"

const UMTCharacterRegistry* UMTCharacterRegistry::Get()
{
	return GetDefault<UMTCharacterRegistry>();
}

UMTCharacterDefinition* UMTCharacterRegistry::LoadDefinition(uint8 Index) const
{
	if (!Characters.IsValidIndex(Index))
	{
		return nullptr;
	}
	return Characters[Index].LoadSynchronous();
}

int32 UMTCharacterRegistry::FindIndexById(FName CharId) const
{
	for (int32 i = 0; i < Characters.Num(); ++i)
	{
		if (UMTCharacterDefinition* Def = Characters[i].LoadSynchronous())
		{
			if (Def->CharacterId == CharId)
			{
				return i;
			}
		}
	}
	return INDEX_NONE;
}
