// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MTGameInstance.generated.h"

class SWidget;

UCLASS()
class MULTIPLAYERTEST_API UMTGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	/** Map (short or long path) to load when hosting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MT|Networking")
	FString LobbyMap = TEXT("/Game/Test_MT");

	UFUNCTION(BlueprintCallable, Category = "MT|Networking")
	void HostSession(int32 NumPublicConnections = 4);

	UFUNCTION(BlueprintCallable, Category = "MT|Networking")
	void FindAndJoinFriendSession();

	UFUNCTION(BlueprintCallable, Category = "MT|Networking")
	void DestroyCurrentSession();

	UFUNCTION(BlueprintCallable, Category = "MT|Networking")
	void LeaveSession();

	UFUNCTION(BlueprintPure, Category = "MT|Networking")
	bool IsInSession() const;

	UFUNCTION(BlueprintPure, Category = "MT|Networking")
	FString GetCurrentSessionIdString() const;

	UFUNCTION(BlueprintCallable, Category = "MT|UI")
	void ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "MT|UI")
	void HideMainMenu();

	UFUNCTION(BlueprintCallable, Category = "MT|UI")
	void ToggleMainMenu();

	UFUNCTION(BlueprintCallable, Category = "MT|UI")
	void OpenSteamInviteOverlay();

	UFUNCTION(BlueprintCallable, Category = "MT|UI")
	void SetMenuStatus(const FString& Text);

protected:
	IOnlineSessionPtr GetSessions() const;

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnSessionInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void OnPostLoadMapWithWorld(UWorld* LoadedWorld);

	enum class EPostDestroy : uint8 { None, Rehost, ReturnToMenu };
	EPostDestroy PostDestroyAction = EPostDestroy::None;

	TSharedPtr<class FOnlineSessionSearch> SessionSearch;

	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle InviteHandle;
	FDelegateHandle PostLoadMapHandle;

	int32 PendingPublicConnections = 4;

	TSharedPtr<SWidget> MenuWidget;
	TSharedPtr<class STextBlock> MenuStatusText;
};
