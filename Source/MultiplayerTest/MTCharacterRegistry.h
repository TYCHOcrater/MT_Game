// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MTCharacterRegistry.generated.h"

class UMTCharacterDefinition;

/**
 * Project-wide list of playable characters.
 * Edit in Project Settings → Game → Multiplayer Test - Characters.
 * The array index is what gets replicated as CharacterDefIndex on AMTPlayerState.
 */
UCLASS(Config=Game, DefaultConfig, meta = (DisplayName = "Multiplayer Test - Characters"))
class MULTIPLAYERTEST_API UMTCharacterRegistry : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Ordered list of available characters. Index 0 = default fallback. */
	UPROPERTY(Config, EditAnywhere, Category = "Characters")
	TArray<TSoftObjectPtr<UMTCharacterDefinition>> Characters;

	static const UMTCharacterRegistry* Get();

	/** Sync-loads the definition at Index. Returns nullptr if out of range or asset missing. */
	UMTCharacterDefinition* LoadDefinition(uint8 Index) const;

	int32 Num() const { return Characters.Num(); }

	/** Returns the index of the entry whose definition has the given CharacterId, or INDEX_NONE. Loads to compare. */
	int32 FindIndexById(FName CharId) const;
};
