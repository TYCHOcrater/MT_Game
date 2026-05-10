// Fill out your copyright notice in the Description page of Project Settings.

#include "MTGameInstance.h"
#include "AMTCharacter.h"
#include "MTPlayerState.h"
#include "MTCharacterRegistry.h"
#include "MTCharacterDefinition.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/OnlineSessionNames.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"

static const FName MT_SESSION_NAME = NAME_GameSession;

void UMTGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogTemp, Display, TEXT("MTGameInstance::Init called"));

	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UMTGameInstance::OnSessionInviteAccepted));
	}

	// Defer menu creation until after first world tick so the viewport is guaranteed ready
	if (UWorld* World = GetWorld())
	{
		FTimerHandle Tmp;
		World->GetTimerManager().SetTimer(Tmp, FTimerDelegate::CreateUObject(this, &UMTGameInstance::ShowMainMenu), 0.1f, false);
	}
	else
	{
		ShowMainMenu();
	}
}

void UMTGameInstance::Shutdown()
{
	HideMainMenu();
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
	}
	Super::Shutdown();
}

IOnlineSessionPtr UMTGameInstance::GetSessions() const
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

void UMTGameInstance::HostSession(int32 NumPublicConnections)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		return;
	}

	if (Sessions->GetNamedSession(MT_SESSION_NAME))
	{
		PendingPublicConnections = NumPublicConnections;
		PostDestroyAction = EPostDestroy::Rehost;
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMTGameInstance::OnDestroySessionComplete));
		Sessions->DestroySession(MT_SESSION_NAME);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL";
	Settings.NumPublicConnections = NumPublicConnections;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bAllowInvites = true;
	Settings.Set(SETTING_MAPNAME, FString(TEXT("MultiplayerTest")), EOnlineDataAdvertisementType::ViaOnlineService);

	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMTGameInstance::OnCreateSessionComplete));

	const ULocalPlayer* LP = GetFirstGamePlayer();
	Sessions->CreateSession(*LP->GetPreferredUniqueNetId(), MT_SESSION_NAME, Settings);
}

void UMTGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}

	if (bWasSuccessful)
	{
		SetMenuStatus(TEXT("Hosting... travelling to map"));
		HideMainMenu();
		const FString Travel = LobbyMap + TEXT("?listen");
		GetWorld()->ServerTravel(Travel);
	}
	else
	{
		SetMenuStatus(TEXT("Failed to create session"));
	}
}

void UMTGameInstance::FindAndJoinFriendSession()
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL";
	SessionSearch->MaxSearchResults = 20;
	SessionSearch->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);

	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMTGameInstance::OnFindSessionsComplete));

	const ULocalPlayer* LP = GetFirstGamePlayer();
	Sessions->FindSessions(*LP->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());
}

void UMTGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}

	if (!bWasSuccessful || !SessionSearch.IsValid() || SessionSearch->SearchResults.Num() == 0)
	{
		SetMenuStatus(TEXT("No friend session found"));
		return;
	}

	SetMenuStatus(FString::Printf(TEXT("Found %d session(s), joining..."), SessionSearch->SearchResults.Num()));

	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMTGameInstance::OnJoinSessionComplete));

	const ULocalPlayer* LP = GetFirstGamePlayer();
	Sessions->JoinSession(*LP->GetPreferredUniqueNetId(), MT_SESSION_NAME, SessionSearch->SearchResults[0]);
}

void UMTGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		return;
	}
	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		SetMenuStatus(TEXT("Failed to join session"));
		return;
	}

	FString ConnectString;
	if (Sessions->GetResolvedConnectString(SessionName, ConnectString))
	{
		SetMenuStatus(TEXT("Connecting..."));
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			PC->ClientTravel(ConnectString, TRAVEL_Absolute);
		}
	}
	else
	{
		SetMenuStatus(TEXT("Could not resolve connect string"));
	}
}

void UMTGameInstance::DestroyCurrentSession()
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		return;
	}
	if (Sessions->GetNamedSession(MT_SESSION_NAME))
	{
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMTGameInstance::OnDestroySessionComplete));
		Sessions->DestroySession(MT_SESSION_NAME);
	}
}

void UMTGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}

	const EPostDestroy Action = PostDestroyAction;
	PostDestroyAction = EPostDestroy::None;

	if (!bWasSuccessful)
	{
		SetMenuStatus(TEXT("Failed to destroy session"));
		return;
	}

	switch (Action)
	{
	case EPostDestroy::Rehost:
		HostSession(PendingPublicConnections);
		break;
	case EPostDestroy::ReturnToMenu:
		// Reload the map standalone (no ?listen) so we tear down the netdriver, then re-show the menu after travel completes.
		PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMTGameInstance::OnPostLoadMapWithWorld);
		UGameplayStatics::OpenLevel(this, FName(*LobbyMap));
		break;
	case EPostDestroy::None:
	default:
		break;
	}
}

void UMTGameInstance::OnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	PostLoadMapHandle.Reset();
	ShowMainMenu();
}

bool UMTGameInstance::IsInSession() const
{
	IOnlineSessionPtr Sessions = GetSessions();
	return Sessions.IsValid() && Sessions->GetNamedSession(MT_SESSION_NAME) != nullptr;
}

FString UMTGameInstance::GetCurrentSessionIdString() const
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		return FString();
	}
	FNamedOnlineSession* Session = Sessions->GetNamedSession(MT_SESSION_NAME);
	if (!Session || !Session->SessionInfo.IsValid())
	{
		return FString();
	}
	return Session->SessionInfo->GetSessionId().ToString();
}

void UMTGameInstance::LeaveSession()
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;

	if (NetMode == NM_Client)
	{
		// Client: disconnect via ClientTravel; the engine tears down the connection and loads GameDefaultMap.
		PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMTGameInstance::OnPostLoadMapWithWorld);
		if (Sessions->GetNamedSession(MT_SESSION_NAME))
		{
			Sessions->DestroySession(MT_SESSION_NAME);
		}
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			PC->ClientTravel(TEXT("?closed"), TRAVEL_Absolute);
		}
		return;
	}

	// Listen server (or standalone with a session entry): destroy session, then reload map standalone.
	if (Sessions->GetNamedSession(MT_SESSION_NAME))
	{
		PostDestroyAction = EPostDestroy::ReturnToMenu;
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMTGameInstance::OnDestroySessionComplete));
		Sessions->DestroySession(MT_SESSION_NAME);
	}
	else
	{
		ShowMainMenu();
	}
}

void UMTGameInstance::OnSessionInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !UserId.IsValid())
	{
		return;
	}

	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		return;
	}

	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMTGameInstance::OnJoinSessionComplete));

	Sessions->JoinSession(*UserId, MT_SESSION_NAME, InviteResult);
}

void UMTGameInstance::ShowMainMenu()
{
	UE_LOG(LogTemp, Display, TEXT("MTGameInstance::ShowMainMenu called (Viewport=%s)"), (GEngine && GEngine->GameViewport) ? TEXT("yes") : TEXT("no"));

	if (MenuWidget.IsValid())
	{
		return;
	}
	if (!GEngine || !GEngine->GameViewport)
	{
		// Try again next tick
		if (UWorld* World = GetWorld())
		{
			FTimerHandle Tmp;
			World->GetTimerManager().SetTimer(Tmp, FTimerDelegate::CreateUObject(this, &UMTGameInstance::ShowMainMenu), 0.1f, false);
		}
		return;
	}

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 28);
	const FSlateFontInfo BodyFont  = FCoreStyle::GetDefaultFontStyle("Regular", 16);

	auto MakeButton = [&](const FString& Label, TFunction<FReply()> OnClick) -> TSharedRef<SButton>
	{
		return SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked_Lambda([OnClick]() { return OnClick(); })
			[
				SNew(STextBlock)
				.Font(BodyFont)
				.Text(FText::FromString(Label))
			];
	};

	TSharedRef<STextBlock> StatusText = SNew(STextBlock)
		.Font(BodyFont)
		.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
		.Text(FText::FromString(TEXT("")));
	MenuStatusText = StatusText;

	FString SteamName;
	if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
	{
		if (IOnlineIdentityPtr Identity = OSS->GetIdentityInterface())
		{
			SteamName = Identity->GetPlayerNickname(0);
		}
	}
	const FString SignedInLine = SteamName.IsEmpty()
		? FString(TEXT("Steam: not signed in"))
		: FString::Printf(TEXT("Signed in as: %s"), *SteamName);

	TSharedRef<SVerticalBox> ButtonStack = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(STextBlock).Font(TitleFont).Text(FText::FromString(TEXT("MultiplayerTest")))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(8, 0, 8, 8))
		[
			SNew(STextBlock)
			.Font(BodyFont)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.85f, 0.7f)))
			.Text(FText::FromString(SignedInLine))
		];

	if (IsInSession())
	{
		const FString SessionId = GetCurrentSessionIdString();
		const FString DisplayId = SessionId.IsEmpty() ? FString(TEXT("(unavailable)")) : SessionId;

		ButtonStack->AddSlot().AutoHeight().Padding(FMargin(8, 4, 8, 4))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(BodyFont)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.85f)))
				.Text(FText::FromString(FString::Printf(TEXT("Lobby ID: %s"), *DisplayId)))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8, 0, 0, 0)).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				.IsEnabled(!SessionId.IsEmpty())
				.OnClicked_Lambda([this, SessionId]()
				{
					if (!SessionId.IsEmpty())
					{
						FPlatformApplicationMisc::ClipboardCopy(*SessionId);
						SetMenuStatus(TEXT("Lobby ID copied to clipboard"));
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Font(BodyFont).Text(FText::FromString(TEXT("Copy")))
				]
			]
		];

		ButtonStack->AddSlot().AutoHeight().Padding(8).HAlign(HAlign_Fill)
		[
			MakeButton(TEXT("Invite Friends (Steam Overlay)"), [this]()
			{
				OpenSteamInviteOverlay();
				return FReply::Handled();
			})
		];
		ButtonStack->AddSlot().AutoHeight().Padding(8).HAlign(HAlign_Fill)
		[
			MakeButton(TEXT("Leave Session"), [this]()
			{
				SetMenuStatus(TEXT("Leaving session..."));
				LeaveSession();
				return FReply::Handled();
			})
		];
	}
	else
	{
		ButtonStack->AddSlot().AutoHeight().Padding(8).HAlign(HAlign_Fill)
		[
			MakeButton(TEXT("Host Session"), [this]()
			{
				SetMenuStatus(TEXT("Creating session..."));
				HostSession(4);
				return FReply::Handled();
			})
		];
		ButtonStack->AddSlot().AutoHeight().Padding(8).HAlign(HAlign_Fill)
		[
			MakeButton(TEXT("Join Friend's Session"), [this]()
			{
				SetMenuStatus(TEXT("Searching for friend's session..."));
				FindAndJoinFriendSession();
				return FReply::Handled();
			})
		];
	}

	// Character cycle button — only show when registry has more than 1 character.
	if (const UMTCharacterRegistry* Registry = UMTCharacterRegistry::Get())
	{
		if (Registry->Num() > 1)
		{
			FString CurrentName = TEXT("?");
			if (UMTCharacterDefinition* CurrentDef = Registry->LoadDefinition(PreferredCharacterIndex))
			{
				CurrentName = CurrentDef->DisplayName.ToString();
				if (CurrentName.IsEmpty())
				{
					CurrentName = CurrentDef->CharacterId.ToString();
				}
			}

			ButtonStack->AddSlot().AutoHeight().Padding(8).HAlign(HAlign_Fill)
			[
				MakeButton(FString::Printf(TEXT("Character: %s  (click to cycle)"), *CurrentName), [this]()
				{
					const UMTCharacterRegistry* Reg = UMTCharacterRegistry::Get();
					if (!Reg || Reg->Num() <= 1)
					{
						return FReply::Handled();
					}
					const uint8 NextIndex = (PreferredCharacterIndex + 1) % Reg->Num();
					SetPreferredCharacterIndex(NextIndex);
					// Rebuild the menu so the button label reflects the new character name.
					HideMainMenu();
					ShowMainMenu();
					return FReply::Handled();
				})
			];
		}
	}

	ButtonStack->AddSlot().AutoHeight().Padding(8).HAlign(HAlign_Fill)
	[
		MakeButton(TEXT("Quit"), [this]()
		{
			if (APlayerController* PC = GetFirstLocalPlayerController())
			{
				PC->ConsoleCommand(TEXT("quit"));
			}
			return FReply::Handled();
		})
	];

	ButtonStack->AddSlot().AutoHeight().Padding(8)
	[
		StatusText
	];

	TSharedRef<SWidget> Root = SNew(SOverlay)
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill).VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0,0,0,0.6f))
	]
	+ SOverlay::Slot()
	.HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(480.f)
		[
			ButtonStack
		]
	];

	MenuWidget = Root;
	GEngine->GameViewport->AddViewportWidgetContent(Root, 100);

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(Root);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
}

void UMTGameInstance::HideMainMenu()
{
	if (MenuWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
	}
	MenuWidget.Reset();
	MenuStatusText.Reset();

	if (APlayerController* PC = GetFirstLocalPlayerController())
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
	}
}

void UMTGameInstance::SetMenuStatus(const FString& Text)
{
	if (MenuStatusText.IsValid())
	{
		MenuStatusText->SetText(FText::FromString(Text));
	}
}

void UMTGameInstance::SetPreferredCharacterIndex(uint8 NewIndex)
{
	const UMTCharacterRegistry* Registry = UMTCharacterRegistry::Get();
	if (Registry && NewIndex >= Registry->Num())
	{
		return;
	}

	PreferredCharacterIndex = NewIndex;

	// If we already have a pawn in-world (mid-match swap), forward immediately.
	// Otherwise the value will be picked up by AAMTCharacter on next possess.
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AAMTCharacter* Pawn = Cast<AAMTCharacter>(PC->GetPawn()))
			{
				if (AMTPlayerState* PS = Pawn->GetPlayerState<AMTPlayerState>())
				{
					PS->ServerRequestSetCharacterDefIndex(NewIndex);
				}
			}
		}
	}
}

void UMTGameInstance::OpenSteamInviteOverlay()
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (!OSS)
	{
		SetMenuStatus(TEXT("No online subsystem"));
		return;
	}
	IOnlineExternalUIPtr UI = OSS->GetExternalUIInterface();
	if (!UI.IsValid())
	{
		SetMenuStatus(TEXT("No external UI (Steam not running?)"));
		return;
	}
	if (!GetSessions() || !GetSessions()->GetNamedSession(MT_SESSION_NAME))
	{
		SetMenuStatus(TEXT("Host a session first"));
		return;
	}
	UI->ShowInviteUI(0, MT_SESSION_NAME);
}

void UMTGameInstance::ToggleMainMenu()
{
	if (MenuWidget.IsValid())
	{
		HideMainMenu();
	}
	else
	{
		ShowMainMenu();
	}
}
