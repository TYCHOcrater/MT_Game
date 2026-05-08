// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MTPlayerState.generated.h"

UCLASS()
class MULTIPLAYERTEST_API AMTPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AMTPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Replicated index into UMTCharacterRegistry::Characters. 0 = default. */
	UPROPERTY(ReplicatedUsing = OnRep_CharacterDefIndex, BlueprintReadOnly, Category = "MT|Character")
	uint8 CharacterDefIndex = 0;

	/** Server-side: validate against registry and write. Triggers OnRep on all clients. No-op if invalid. */
	void SetCharacterDefIndex(uint8 NewIndex);

	/** Owning client → server. Used by the locally controlled pawn to push GameInstance.PreferredCharacterIndex up. */
	UFUNCTION(Server, Reliable)
	void ServerRequestSetCharacterDefIndex(uint8 NewIndex);

protected:
	UFUNCTION()
	void OnRep_CharacterDefIndex();

	/** Find the character pawn this PS owns (if any) and tell it to re-pull its character def. */
	void RefreshOwningPawnCharacter();
};
