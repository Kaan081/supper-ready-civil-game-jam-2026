#include "SonHatEvacuationHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 MaxHudLines = 11;
	constexpr float LineHeight = 30.0f;
	constexpr float HudLeft = 24.0f;
	constexpr float HudTop = 22.0f;
}

TSharedRef<SWidget> USonHatEvacuationHUDWidget::RebuildWidget()
{
	if (WidgetTree->RootWidget) { return Super::RebuildWidget(); }
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SonHatHudRoot"));
	WidgetTree->RootWidget = RootCanvas;
	ReadoutBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ReadoutBackground"));
	ReadoutBackground->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.97f));
	if (UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(ReadoutBackground))
	{
		BackgroundSlot->SetOffsets(FMargin(HudLeft - 12, HudTop - 8, 790, 180));
	}
	LineWidgets.Reset();
	LineWidgets.Reserve(MaxHudLines);
	for (int32 Index = 0; Index < MaxHudLines; ++Index)
	{
		UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Line->SetAutoWrapText(false);
		Line->SetShadowOffset(FVector2D(2, 2));
		Line->SetClipping(EWidgetClipping::ClipToBounds);
		FSlateFontInfo Font = Line->GetFont();
		Font.Size = Index == 0 ? 26 : Index == 1 ? 22 : 20;
		Line->SetFont(Font);
		if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Line))
		{
			PanelSlot->SetOffsets(FMargin(HudLeft, HudTop + Index * LineHeight, 770, LineHeight + 4));
		}
		LineWidgets.Add(Line);
	}
	PromptBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptBackground"));
	PromptBackground->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.86f));
	if (UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(PromptBackground))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
		BackgroundSlot->SetAlignment(FVector2D(0.5f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f, -68.f, 784.f, 80.f));
	}
	PromptBackground->SetVisibility(ESlateVisibility::Collapsed);
	InteractionPrompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InteractionPrompt"));
	InteractionPrompt->SetAutoWrapText(true);
	InteractionPrompt->SetJustification(ETextJustify::Center);
	InteractionPrompt->SetShadowOffset(FVector2D(2, 2));
	FSlateFontInfo PromptFont = InteractionPrompt->GetFont();
	PromptFont.Size = 20;
	InteractionPrompt->SetFont(PromptFont);
	if (UCanvasPanelSlot* PromptSlot = RootCanvas->AddChildToCanvas(InteractionPrompt))
	{
		PromptSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
		PromptSlot->SetAlignment(FVector2D(0.5f, 1.f));
		PromptSlot->SetOffsets(FMargin(0.f, -76.f, 760.f, 66.f));
	}
	InteractionPrompt->SetVisibility(ESlateVisibility::Collapsed);
	return Super::RebuildWidget();
}

void USonHatEvacuationHUDWidget::SetInteractionPrompt(const FString& Text, const FLinearColor& Color)
{
	if (!InteractionPrompt) { return; }
	if (Text.IsEmpty())
	{
		InteractionPrompt->SetVisibility(ESlateVisibility::Collapsed);
		if (PromptBackground) { PromptBackground->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}
	InteractionPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (PromptBackground) { PromptBackground->SetVisibility(ESlateVisibility::HitTestInvisible); }
	if (!InteractionPrompt->GetText().ToString().Equals(Text, ESearchCase::CaseSensitive)) { InteractionPrompt->SetText(FText::FromString(Text)); }
	InteractionPrompt->SetColorAndOpacity(FSlateColor(Color));
}

void USonHatEvacuationHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USonHatEvacuationHUDWidget::SetHudLines(TArray<FString>&& Lines, const TArray<FLinearColor>& LineColors)
{
	VisibleReadoutLines = FMath::Min(Lines.Num(), MaxHudLines);
	if (ReadoutBackground)
	{
		if (UCanvasPanelSlot* BackgroundSlot = Cast<UCanvasPanelSlot>(ReadoutBackground->Slot))
		{
			BackgroundSlot->SetSize(FVector2D(790, FMath::Min(Lines.Num(), MaxHudLines) * LineHeight + 16));
		}
		ReadoutBackground->SetVisibility(Lines.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	for (int32 Index = 0; Index < LineWidgets.Num(); ++Index)
	{
		UTextBlock* Line = LineWidgets[Index];
		if (!Line) { continue; }
		if (Lines.IsValidIndex(Index) && !Lines[Index].IsEmpty())
		{
			Line->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (!Line->GetText().ToString().Equals(Lines[Index], ESearchCase::CaseSensitive)) { Line->SetText(FText::FromString(Lines[Index])); }
			const FLinearColor Color = LineColors.IsValidIndex(Index) ? LineColors[Index] : FLinearColor::White;
			Line->SetColorAndOpacity(FSlateColor(Color));
		}
		else
		{
			Line->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USonHatEvacuationHUDWidget::SetTerminalWaypoints(TArray<FSonHatTerminalWaypoint>&& Waypoints)
{
	TerminalWaypoints = MoveTemp(Waypoints);
}

FVector2D USonHatEvacuationHUDWidget::CalculateWaypointCardPosition(const FVector2D& Point, const FVector2D& Size,
	const FVector2D& CardSize, float ReadoutHeight)
{
	FVector2D Card(Point.X - CardSize.X * 0.5, Point.Y + 15);
	// Grapple's crosshair AND attached/valid-target text need an unobstructed central area.
	const FBox2D Aim(Size * 0.5 - FVector2D(175, 100), Size * 0.5 + FVector2D(175, 100));
	const auto Overlaps = [&Card, &CardSize](const FBox2D& Area) { return FBox2D(Card, Card + CardSize).Intersect(Area); };
	if (Overlaps(Aim)) { Card.X = Aim.Max.X + 18; Card.Y = Point.Y - CardSize.Y * 0.5; }
	Card.X = FMath::Clamp(Card.X, 12.0, FMath::Max(12.0, Size.X - CardSize.X - 12));
	Card.Y = FMath::Clamp(Card.Y, 12.0, FMath::Max(12.0, Size.Y - CardSize.Y - 170));
	if (Overlaps(FBox2D(FVector2D(0, 0), FVector2D(810, ReadoutHeight)))) { Card.Y = ReadoutHeight + 12; }
	if (Overlaps(Aim)) { Card.Y = Aim.Max.Y + 16; }
	return Card;
}

int32 USonHatEvacuationHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, Geometry, CullingRect, DrawElements, LayerId, Style, bParentEnabled) + 1;
	const FVector2D Size = Geometry.GetLocalSize();
	TArray<FBox2D> DrawnCards;
	for (const FSonHatTerminalWaypoint& Waypoint : TerminalWaypoints)
	{
		const FVector2D Point = Waypoint.Position * Size;
		const float Radius = Waypoint.bPrimary ? 10.f : 6.f;
		TArray<FVector2D> Diamond{Point + FVector2D(0, -Radius), Point + FVector2D(Radius, 0),
			Point + FVector2D(0, Radius), Point + FVector2D(-Radius, 0), Point + FVector2D(0, -Radius)};
		FSlateDrawElement::MakeLines(DrawElements, Layer, Geometry.ToPaintGeometry(), Diamond,
			ESlateDrawEffect::None, Waypoint.Color, true, Waypoint.bPrimary ? 2.5f : 1.5f);
		const FVector2D CardSize(330, Waypoint.bPrimary ? 58 : 32);
		// Keep the console, crosshair and grapple feedback readable while aiming at a terminal.
		const FVector2D Card = CalculateWaypointCardPosition(Point, Size, CardSize, HudTop + VisibleReadoutLines * LineHeight + 16);
		const FBox2D Bounds(Card, Card + CardSize);
		if (!Waypoint.bPrimary && DrawnCards.ContainsByPredicate([&Bounds](const FBox2D& Other) { return Other.Intersect(Bounds); })) { continue; }
		DrawnCards.Add(Bounds);
		FSlateDrawElement::MakeBox(DrawElements, Layer, Geometry.ToPaintGeometry(CardSize, FSlateLayoutTransform(Card)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.015f, 0.025f, 0.04f, 0.86f));
		FSlateDrawElement::MakeText(DrawElements, Layer + 1, Geometry.ToPaintGeometry(CardSize, FSlateLayoutTransform(Card + FVector2D(8, 3))),
			Waypoint.Title, FCoreStyle::GetDefaultFontStyle("Bold", 18), ESlateDrawEffect::None, Waypoint.Color);
		if (Waypoint.bPrimary)
		{
			FSlateDrawElement::MakeText(DrawElements, Layer + 1, Geometry.ToPaintGeometry(CardSize, FSlateLayoutTransform(Card + FVector2D(8, 29))),
				Waypoint.Detail, FCoreStyle::GetDefaultFontStyle("Regular", 16), ESlateDrawEffect::None, FLinearColor::White);
		}
		if (Waypoint.bOffscreen)
		{
			const FVector2D Direction = (Point - Size * 0.5).GetSafeNormal();
			const FVector2D Side(-Direction.Y, Direction.X);
			const FVector2D Tip = Point + Direction * (Radius + 11);
			TArray<FVector2D> Arrow{Tip - Direction * 7 + Side * 5, Tip, Tip - Direction * 7 - Side * 5};
			FSlateDrawElement::MakeLines(DrawElements, Layer + 1, Geometry.ToPaintGeometry(), Arrow,
				ESlateDrawEffect::None, Waypoint.Color, true, 2.f);
		}
	}
	return Layer + 1;
}
