// Fill out your copyright notice in the Description page of Project Settings.

#include "MTHUD.h"
#include "AMTCharacter.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
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

void AMTHUD::ShowDamageDirection(const FVector& WorldDir)
{
	LastDamageDirWorld = WorldDir.GetSafeNormal();
	const UWorld* World = GetWorld();
	LastDamageDirTime = World ? World->GetTimeSeconds() : 0.0f;
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

	// Directional damage indicator — red wedge on the side of the screen pointing toward the attacker.
	// Position computed from the angle between camera forward and the world-space dir-to-attacker,
	// projected to the horizontal plane. Fades out over DamageIndicatorDuration.
	const float DmgElapsed = Now - LastDamageDirTime;
	if (DamageIndicatorDuration > 0.0f && DmgElapsed >= 0.0f && DmgElapsed < DamageIndicatorDuration && !LastDamageDirWorld.IsNearlyZero())
	{
		FRotator CamRot = FRotator::ZeroRotator;
		FVector CamLoc = FVector::ZeroVector;
		if (PC)
		{
			PC->GetPlayerViewPoint(CamLoc, CamRot);
		}
		FVector CamFwd = FRotationMatrix(FRotator(0.0f, CamRot.Yaw, 0.0f)).GetUnitAxis(EAxis::X);
		FVector CamRight = FRotationMatrix(FRotator(0.0f, CamRot.Yaw, 0.0f)).GetUnitAxis(EAxis::Y);

		FVector DirH = LastDamageDirWorld; DirH.Z = 0.0f; DirH.Normalize();
		const float Fwd = FVector::DotProduct(CamFwd, DirH);    // +1 attacker in front, -1 behind
		const float Right = FVector::DotProduct(CamRight, DirH); // +1 attacker right
		const float AngleRad = FMath::Atan2(Right, Fwd);          // -PI..PI, 0 = front, PI/2 = right, ±PI = behind

		// Fade with a steep curve (alpha² so it lingers strong then fades fast at the end).
		const float LinearAlpha = FMath::Clamp(1.0f - DmgElapsed / DamageIndicatorDuration, 0.0f, 1.0f);
		const float Alpha = LinearAlpha * LinearAlpha;

		// Draw a curved arc of N line segments, centered on the attacker angle. Looks more like a
		// "danger blip" than a hard triangle. Multiple parallel arcs at different radii = thick look.
		const int32 NumSegs = 14;
		const float ArcHalfRad = FMath::DegreesToRadians(22.0f);  // arc spans 44° total
		const FLinearColor DmgColor(0.95f, 0.15f, 0.15f, Alpha);

		auto DrawArc = [&](float Radius, float Thickness)
		{
			FVector2D Prev;
			for (int32 i = 0; i <= NumSegs; ++i)
			{
				const float T = (float)i / (float)NumSegs;
				const float SegAngle = AngleRad - ArcHalfRad + T * 2.0f * ArcHalfRad;
				const FVector2D P(CX + Radius * FMath::Sin(SegAngle), CY - Radius * FMath::Cos(SegAngle));
				if (i > 0)
				{
					FCanvasLineItem Seg(Prev, P);
					Seg.SetColor(DmgColor);
					Seg.LineThickness = Thickness;
					Canvas->DrawItem(Seg);
				}
				Prev = P;
			}
		};
		// Stack three arcs at slightly different radii for a "thick glow" look without needing a texture.
		DrawArc(DamageIndicatorRadiusPx,        4.0f);
		DrawArc(DamageIndicatorRadiusPx + 5.0f, 2.0f);
		DrawArc(DamageIndicatorRadiusPx - 5.0f, 2.0f);
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
