#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GrappleReticleWidget.generated.h"

/** Tiny, non-interactive native reticle; no debug drawing or Widget Blueprint required. */
UCLASS()
class UEINTROPROJECT_1_API UGrappleReticleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetIndicator(FLinearColor Color, bool bAttached, bool bReeling, const FString& Hint = FString());
	void SetControlHint(const FString& Hint);
	void SetFallOverlay(const FString& Message);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	FLinearColor IndicatorColor = FLinearColor::Gray;
	bool bIsAttached = false;
	bool bIsReeling = false;
	FString TargetHint;
	FString ControlHint;
	FString FallOverlay;
};
