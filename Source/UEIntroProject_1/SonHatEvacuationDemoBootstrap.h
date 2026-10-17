#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SonHatEvacuationManager.h"
#include "SonHatEvacuationDemoBootstrap.generated.h"

class ACharacter;
class APawn;
class APlayerController;
class UCanvas;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Place ONE actor, press Play. Creates a temporary, entirely runtime evacuation demo. */
UCLASS(Blueprintable)
class UEINTROPROJECT_1_API ASonHatEvacuationDemoBootstrap : public AActor
{
	GENERATED_BODY()

public:
	ASonHatEvacuationDemoBootstrap();
	virtual void Tick(float DeltaSeconds) override;

	/** Starts after the local player is ready. Game time; pause and time dilation apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Demo", meta = (ClampMin = "1.0", ClampMax = "300.0", Units = "s"))
	float PreparationDuration = 45.0f;

	/** Clamped to 100-280 cm so neighboring 650 cm-spaced terminals cannot overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Demo", meta = (ClampMin = "100.0", ClampMax = "280.0", Units = "cm"))
	float InteractionDistance = 250.0f;

	/** Move the first local player onto the demo deck. Disable only if your spawn is already suitable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Demo")
	bool bMovePlayerToDemo = true;

	/** E calls this automatically. Range and budget are enforced by the existing manager. */
	UFUNCTION(BlueprintCallable, Category = "Son Hat|Demo")
	ESonHatEvacuationActivationResult Interact();

	/** R calls this automatically. Replaces only this demo's manager, releasing its owned links. */
	UFUNCTION(BlueprintCallable, Category = "Son Hat|Demo")
	void RestartDemo();

	UFUNCTION(BlueprintPure, Category = "Son Hat|Demo")
	ASonHatEvacuationManager* GetDemoManager() const { return Manager; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool CreateNetwork();
	bool CreateManager();
	bool PreparePlayer();
	void UpdateFallbackMovement(float DeltaSeconds);
	void UpdateVisuals();
	void DrawDemoHUD(UCanvas* Canvas, APlayerController* PlayerController);
	int32 FindNearestTerminal() const;
	FVector DemoPosition(const FVector& LocalPosition) const;
	UTextRenderComponent* AddLabel(const FVector& Position, float Size);
	void AddMarker(const FVector& Position, const FVector& Scale);

	UFUNCTION()
	void HandleRoundEnded(int32 SafePopulation, int32 TotalPopulation);

	UPROPERTY(VisibleAnywhere, Category = "Son Hat|Demo")
	TObjectPtr<UStaticMeshComponent> StageFloor;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<ASonHatEvacuationManager> Manager;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASonHatEvacuationNode>> DemoNodes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASonHatEvacuationLink>> DemoLinks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> NodeLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> TerminalLabels;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> FallbackCharacter;

	TWeakObjectPtr<APlayerController> DemoController;
	TWeakObjectPtr<APawn> OriginalPawn;
	FTransform OriginalPlayerTransform;
	FRotator OriginalControlRotation = FRotator::ZeroRotator;
	FDelegateHandle HUDDrawHandle;
	FString Feedback = TEXT("Waiting for the local player. Use Play, not Simulate.");
	FColor FeedbackColor = FColor::White;
	bool bNetworkCreated = false;
	bool bPlayerReady = false;
};
