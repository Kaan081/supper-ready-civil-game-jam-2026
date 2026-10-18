#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SonHatEvacuationHUDWidget.generated.h"

class UTextBlock;
class UBorder;

/** Screen presentation only. Availability and positions are supplied by the round Director. */
struct FSonHatTerminalWaypoint
{
	FVector2D Position = FVector2D::ZeroVector;
	FString Title;
	FString Detail;
	FLinearColor Color = FLinearColor::White;
	bool bOffscreen = false;
	bool bPrimary = false;
};

/** Code-built HUD; works in packaged builds without a Widget Blueprint asset. */
UCLASS()
class UEINTROPROJECT_1_API USonHatEvacuationHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHudLines(TArray<FString>&& Lines, const TArray<FLinearColor>& LineColors);
	void SetInteractionPrompt(const FString& Text, const FLinearColor& Color);
	void SetTerminalWaypoints(TArray<FSonHatTerminalWaypoint>&& Waypoints);
	const TArray<FSonHatTerminalWaypoint>& GetTerminalWaypoints() const { return TerminalWaypoints; }
	/** Shared layout rule keeps guidance outside the aim/feedback area and persistent readout. */
	static FVector2D CalculateWaypointCardPosition(const FVector2D& Point, const FVector2D& Size,
		const FVector2D& CardSize, float ReadoutHeight);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InteractionPrompt;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> ReadoutBackground;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PromptBackground;
	TArray<FSonHatTerminalWaypoint> TerminalWaypoints;
	int32 VisibleReadoutLines = 0;
};
