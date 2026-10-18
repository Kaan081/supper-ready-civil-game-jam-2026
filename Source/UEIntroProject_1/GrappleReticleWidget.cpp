#include "GrappleReticleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

TSharedRef<SWidget> UGrappleReticleWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(); }
	return Super::RebuildWidget();
}

void UGrappleReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ForceVolatile(true);
}

void UGrappleReticleWidget::SetIndicator(FLinearColor Color, bool bAttached, bool bReeling, const FString& Hint)
{
	IndicatorColor = Color;
	bIsAttached = bAttached;
	bIsReeling = bReeling;
	TargetHint = Hint;
}

void UGrappleReticleWidget::SetControlHint(const FString& Hint)
{
	ControlHint = Hint;
}

void UGrappleReticleWidget::SetFallOverlay(const FString& Message)
{
	FallOverlay = Message;
}

int32 UGrappleReticleWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, Geometry, CullingRect, DrawElements, LayerId, Style, bParentEnabled) + 1;
	const FVector2D Center = Geometry.GetLocalSize() * 0.5;
	const auto CenteredText = [&](const FString& Text, float BelowCenter, int32 FontSize, const FLinearColor& Color)
	{
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FontSize);
		const FVector2D Extent = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
		const FVector2D Origin = Center + FVector2D(-Extent.X * 0.5, BelowCenter);
		FSlateDrawElement::MakeBox(DrawElements, Layer, Geometry.ToPaintGeometry(Extent + FVector2D(16, 8),
			FSlateLayoutTransform(Origin - FVector2D(8, 4))), FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None, FLinearColor(0.015f, 0.025f, 0.04f, 0.85f));
		FSlateDrawElement::MakeText(DrawElements, Layer + 1, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(Origin)),
			Text, Font, ESlateDrawEffect::None, Color);
	};
	const auto Segment = [&](FVector2D A, FVector2D B)
	{
		TArray<FVector2D> Points{Center + A, Center + B};
		FSlateDrawElement::MakeLines(DrawElements, Layer, Geometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, IndicatorColor, true, 1.5f);
	};
	const float Arm = bIsAttached ? 7.0f : 8.0f;
	const float Inner = bIsAttached ? 2.5f : 4.0f;
	Segment(FVector2D(-Arm, 0), FVector2D(-Inner, 0));
	Segment(FVector2D(Inner, 0), FVector2D(Arm, 0));
	Segment(FVector2D(0, -Arm), FVector2D(0, -Inner));
	Segment(FVector2D(0, Inner), FVector2D(0, Arm));
	if (bIsReeling)
	{
		Segment(FVector2D(-4, 18), FVector2D(0, 14));
		Segment(FVector2D(0, 14), FVector2D(4, 18));
	}
	if (!TargetHint.IsEmpty())
	{
		CenteredText(TargetHint, 22, 16, IndicatorColor);
	}
	if (!ControlHint.IsEmpty())
	{
		FSlateDrawElement::MakeText(DrawElements, Layer, Geometry.ToPaintGeometry(FVector2D(520, 36),
			FSlateLayoutTransform(FVector2D(24, Geometry.GetLocalSize().Y - 48))), ControlHint,
			FCoreStyle::GetDefaultFontStyle("Regular", 16), ESlateDrawEffect::None, FLinearColor(0.92f, 0.92f, 0.92f, 0.92f));
	}
	if (!FallOverlay.IsEmpty())
	{
		CenteredText(FallOverlay, 58, 22, FLinearColor(1.f, 0.55f, 0.3f));
	}
	return Layer + 1;
}
