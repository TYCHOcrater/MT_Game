// Fill out your copyright notice in the Description page of Project Settings.

#include "MTHUD.h"
#include "AMTCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

void AMTHUD::PushNotification(const FString& Text)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	Notifications.Add({Text, Now + NotificationDuration});
}

void AMTHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!bShowMTHUD || !Canvas)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	AAMTCharacter* Char = PC ? Cast<AAMTCharacter>(PC->GetPawn()) : nullptr;

	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const float HealthValue = Char ? Char->Health : 0.0f;
	const FString Text = FString::Printf(TEXT("HP: %.0f"), HealthValue);

	FCanvasTextItem Item(FVector2D(20.0f, Canvas->SizeY - 60.0f), FText::FromString(Text), Font, FLinearColor::White);
	Item.EnableShadow(FLinearColor::Black);
	Item.Scale = FVector2D(1.5f, 1.5f);
	Canvas->DrawItem(Item);

	// Crosshair
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float S = 8.0f;
	FCanvasLineItem H(FVector2D(CX - S, CY), FVector2D(CX + S, CY));
	FCanvasLineItem V(FVector2D(CX, CY - S), FVector2D(CX, CY + S));
	H.SetColor(FLinearColor::White);
	V.SetColor(FLinearColor::White);
	Canvas->DrawItem(H);
	Canvas->DrawItem(V);

	// Notifications (top-left), expire by time
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Notifications.RemoveAll([Now](const FMTNotification& N) { return N.ExpireTime <= Now; });

	float YOffset = 20.0f;
	for (const FMTNotification& N : Notifications)
	{
		FCanvasTextItem NotifyItem(FVector2D(20.0f, YOffset), FText::FromString(N.Text), Font, FLinearColor(0.85f, 0.85f, 1.0f));
		NotifyItem.EnableShadow(FLinearColor::Black);
		NotifyItem.Scale = FVector2D(1.1f, 1.1f);
		Canvas->DrawItem(NotifyItem);
		YOffset += 24.0f;
	}
}
