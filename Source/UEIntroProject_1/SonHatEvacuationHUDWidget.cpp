#include "SonHatEvacuationHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace
{
	constexpr int32 MaxHudLines = 12;
	constexpr float LineHeight = 26.0f;
	constexpr float HudLeft = 24.0f;
	constexpr float HudTop = 22.0f;
}

void USonHatEvacuationHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SonHatHudRoot"));
	WidgetTree->RootWidget = RootCanvas;
	LineWidgets.Reserve(MaxHudLines);
	for (int32 Index = 0; Index < MaxHudLines; ++Index)
	{
		UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Line->SetAutoWrapText(true);
		Line->SetShadowOffset(FVector2D(1, 1));
		if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Line))
		{
			PanelSlot->SetPosition(FVector2D(HudLeft, HudTop + Index * LineHeight));
			PanelSlot->SetSize(FVector2D(900, LineHeight + 4));
		}
		LineWidgets.Add(Line);
	}
}

void USonHatEvacuationHUDWidget::SetHudLines(TArray<FString>&& Lines, const TArray<FLinearColor>& LineColors)
{
	for (int32 Index = 0; Index < LineWidgets.Num(); ++Index)
	{
		UTextBlock* Line = LineWidgets[Index];
		if (!Line) { continue; }
		if (Lines.IsValidIndex(Index) && !Lines[Index].IsEmpty())
		{
			Line->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (Line->GetText().ToString() != Lines[Index]) { Line->SetText(FText::FromString(Lines[Index])); }
			const FLinearColor Color = LineColors.IsValidIndex(Index) ? LineColors[Index] : FLinearColor::White;
			Line->SetColorAndOpacity(FSlateColor(Color));
		}
		else
		{
			Line->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
