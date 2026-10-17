#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SonHatEvacuationHUDWidget.generated.h"

class UTextBlock;

/** Code-built HUD; works in packaged builds without a Widget Blueprint asset. */
UCLASS()
class UEINTROPROJECT_1_API USonHatEvacuationHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHudLines(TArray<FString>&& Lines, const TArray<FLinearColor>& LineColors);

protected:
	virtual void NativeConstruct() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineWidgets;
};
