// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MTCharacterDefinition.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;
class UTexture2D;

/**
 * One playable character: mesh + ABP + display info.
 * Authored as a DataAsset in editor. Registered in UMTCharacterRegistry (Project Settings → Multiplayer Test).
 */
UCLASS(BlueprintType)
class MULTIPLAYERTEST_API UMTCharacterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identifier — used for save data / debug. The replicated wire format is the registry index, not this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TSoftClassPtr<UAnimInstance> AnimBP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Mesh component relative location override. AAMTCharacter's default is (0,0,-90). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Mesh")
	FVector MeshRelativeLocation = FVector(0.0f, 0.0f, -90.0f);

	/** Mesh component relative rotation override. AAMTCharacter's default is (0,-90,0) so the mesh faces forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Mesh")
	FRotator MeshRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

	/** Uniform scale to apply to the mesh component. Use to compensate for FBX scale mismatches without re-exporting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Mesh", meta = (ClampMin = "0.01"))
	float MeshUniformScale = 1.0f;

	/** Emote / dance anim montage played when the player triggers the Emote input. Authored on the character's
	 *  own skeleton — each character has its own per-skeleton montage. Optional; if null, Emote does nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Emote")
	TSoftObjectPtr<UAnimMontage> EmoteMontage;
};
