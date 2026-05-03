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

void AMTHUD::PushKillEvent(const FString& Text)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	KillFeed.Add({Text, Now + KillFeedDuration});
}

void AMTHUD::ShowHitMarker(bool bIsCrit)
{
	const UWorld* World = GetWorld();
	LastHitMarkerTime = World ? World->GetTimeSeconds() : 0.0f;
	bLastHitWasCrit = bIsCrit;
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

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Hit marker — 4 short diagonals around the crosshair, fading over HitMarkerDuration.
	// Crits draw larger and gold so the shooter feels them.
	const float HitElapsed = Now - LastHitMarkerTime;
	if (HitMarkerDuration > 0.0f && HitElapsed >= 0.0f && HitElapsed < HitMarkerDuration)
	{
		const float Alpha = FMath::Clamp(1.0f - HitElapsed / HitMarkerDuration, 0.0f, 1.0f);
		const FLinearColor MarkerColor = bLastHitWasCrit
			? FLinearColor(1.0f, 0.84f, 0.2f, Alpha)
			: FLinearColor(1.0f, 1.0f, 1.0f, Alpha);
		const float Inner = bLastHitWasCrit ? 7.0f : 5.0f;
		const float Outer = bLastHitWasCrit ? 18.0f : 11.0f;
		const float Thickness = bLastHitWasCrit ? 2.5f : 1.5f;

		FCanvasLineItem TL(FVector2D(CX - Outer, CY - Outer), FVector2D(CX - Inner, CY - Inner));
		FCanvasLineItem TR(FVector2D(CX + Outer, CY - Outer), FVector2D(CX + Inner, CY - Inner));
		FCanvasLineItem BL(FVector2D(CX - Outer, CY + Outer), FVector2D(CX - Inner, CY + Inner));
		FCanvasLineItem BR(FVector2D(CX + Outer, CY + Outer), FVector2D(CX + Inner, CY + Inner));
		TL.SetColor(MarkerColor);
		TR.SetColor(MarkerColor);
		BL.SetColor(MarkerColor);
		BR.SetColor(MarkerColor);
		TL.LineThickness = Thickness;
		TR.LineThickness = Thickness;
		BL.LineThickness = Thickness;
		BR.LineThickness = Thickness;
		Canvas->DrawItem(TL);
		Canvas->DrawItem(TR);
		Canvas->DrawItem(BL);
		Canvas->DrawItem(BR);
	}

	// Notifications (top-left), expire by time
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

	// Kill feed (top-right), newest at top, fade alpha in last 1s
	KillFeed.RemoveAll([Now](const FMTKillEvent& K) { return K.ExpireTime <= Now; });

	float KillY = 20.0f;
	for (const FMTKillEvent& K : KillFeed)
	{
		const float Remaining = K.ExpireTime - Now;
		const float KAlpha = FMath::Clamp(Remaining / 1.0f, 0.0f, 1.0f);

		float TextW = 0.0f;
		float TextH = 0.0f;
		Canvas->StrLen(Font, K.Text, TextW, TextH);
		const float Scale = 1.1f;
		const float DrawW = TextW * Scale;
		const float DrawX = Canvas->SizeX - 20.0f - DrawW;

		FCanvasTextItem KillItem(FVector2D(DrawX, KillY), FText::FromString(K.Text), Font, FLinearColor(1.0f, 0.65f, 0.3f, KAlpha));
		KillItem.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, KAlpha));
		KillItem.Scale = FVector2D(Scale, Scale);
		Canvas->DrawItem(KillItem);
		KillY += 24.0f;
	}
}
