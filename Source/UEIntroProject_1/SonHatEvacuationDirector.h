#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SonHatEvacuationManager.h"
#include "SonHatEvacuationDirector.generated.h"

class APlayerController;
class UMaterialInstanceDynamic;
class USonHatEvacuationHUDWidget;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ESonHatEvacuationRoundPhase : uint8
{
	WaitingForPlayer,
	Preparing,
	FloodReveal,
	Results,
	ConfigurationError
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSonHatEvacuationPhaseChanged, ESonHatEvacuationRoundPhase, Phase);

/** Level integration only: uses the existing pawn/rooftops and externally placed nodes/terminals. */
UCLASS(Blueprintable)
class UEINTROPROJECT_1_API ASonHatEvacuationDirector : public AActor
{
	GENERATED_BODY()

public:
	ASonHatEvacuationDirector();
	virtual void Tick(float DeltaSeconds) override;

	/** Discover loaded level actors once at round start. No nodes, links, terrain or pawns are generated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Level Network")
	bool bAutoDiscoverNetwork = true;

	/** Optional Actor Tag shared by the nodes AND links of this network. None discovers all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Level Network", meta = (EditCondition = "bAutoDiscoverNetwork"))
	FName NetworkTag = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Son Hat|Level Network", meta = (EditCondition = "!bAutoDiscoverNetwork"))
	TArray<TObjectPtr<ASonHatEvacuationNode>> Nodes;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Son Hat|Level Network", meta = (EditCondition = "!bAutoDiscoverNetwork"))
	TArray<TObjectPtr<ASonHatEvacuationLink>> CandidateLinks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round", meta = (ClampMin = "1.0", Units = "s"))
	float PreparationDuration = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round", meta = (ClampMin = "0"))
	int32 ConnectionBudget = 3;

	/** Full 3D pawn-origin distance. Place terminal origins near standing capsule-center height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Interaction", meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionDistance = 275.0f;

	/** Half-angle from the camera forward vector; terminals outside this cone are not focused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Interaction", meta = (ClampMin = "5.0", ClampMax = "75.0", Units = "deg"))
	float TerminalAimHalfAngleDegrees = 28.0f;

	/** Visibility trace from the existing player's camera prevents activation through rooftops/walls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Interaction")
	bool bRequireLineOfSight = true;

	/** Optional development shortcut: F finalizes the manager early and starts the same flood reveal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round")
	bool bAllowEarlyFinishForTesting = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Flood Reveal", meta = (ClampMin = "0.0", Units = "s"))
	float FloodRevealDuration = 8.0f;

	/** Visual water rises to this director's world Z. This never determines graph safety. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Flood Reveal", meta = (ClampMin = "0.0", Units = "cm"))
	float FloodRiseDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Flood Reveal")
	bool bFitFloodToNetwork = true;

	/** Used if automatic XY bounds are disabled. In centimeters, relative to director XY. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Flood Reveal", meta = (EditCondition = "!bFitFloodToNetwork", ClampMin = "100.0"))
	FVector2D FloodHalfSize = FVector2D(6000, 6000);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation", meta = (ClampMin = "10.0", Units = "cm"))
	float LabelWorldSize = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Meshes")
	TObjectPtr<UStaticMesh> ConnectionBeamMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Meshes")
	TObjectPtr<UStaticMesh> MarkerSphereMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Meshes")
	TObjectPtr<UStaticMesh> FloodPlaneMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Materials")
	TObjectPtr<UMaterialInterface> ConnectionLineMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Materials")
	TObjectPtr<UMaterialInterface> MarkerMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Materials")
	TObjectPtr<UMaterialInterface> FloodSurfaceMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorHub = FLinearColor(0, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorSafeAccess = FLinearColor(0, 1, 0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorNoAccess = FLinearColor(0.75f, 0.75f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorFloodNoAccess = FLinearColor(1, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorSelectedSafe = FLinearColor(0, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorUnselectedSafe = FLinearColor(0.75f, 0.75f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorUnsafe = FLinearColor(1, 0.5f, 0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorFailed = FLinearColor(1, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorIntactNoHub = FLinearColor(1, 1, 0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style")
	FLinearColor ColorFocusRing = FLinearColor(1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style", meta = (ClampMin = "0.5", Units = "cm"))
	float ConnectionBeamRadius = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style", meta = (ClampMin = "10.0", Units = "cm"))
	float NodeMarkerRadius = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style", meta = (ClampMin = "5.0", Units = "cm"))
	float TerminalMarkerRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style", meta = (ClampMin = "5.0", Units = "cm"))
	float FocusedTerminalRadius = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Replay")
	bool bReturnPlayerToStartOnReplay = false;

	/** Optional explicit spawn; otherwise the first round-start pawn transform is captured. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Replay", meta = (EditCondition = "bReturnPlayerToStartOnReplay"))
	TObjectPtr<AActor> ReplaySpawnAnchor;

	/** Starts/replays using a new owned manager. Placed level actors and player traversal are preserved. */
	UFUNCTION(BlueprintCallable, Category = "Son Hat|Round")
	bool RestartRound();

	/** E calls this automatically, with the same target the HUD highlights. */
	UFUNCTION(BlueprintCallable, Category = "Son Hat|Round")
	ESonHatEvacuationActivationResult Interact();

	UFUNCTION(BlueprintPure, Category = "Son Hat|Round")
	ASonHatEvacuationLink* GetFocusedLink() const;

	UFUNCTION(BlueprintPure, Category = "Son Hat|Round")
	ASonHatEvacuationManager* GetRoundManager() const { return Manager; }

	UFUNCTION(BlueprintPure, Category = "Son Hat|Round")
	float GetFloodRevealProgress() const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Son Hat|Round")
	ESonHatEvacuationRoundPhase Phase = ESonHatEvacuationRoundPhase::WaitingForPlayer;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Son Hat|Round")
	FString StatusMessage = TEXT("Waiting for the existing local player pawn. Use Play, not Simulate.");

	UPROPERTY(BlueprintAssignable, Category = "Son Hat|Round")
	FSonHatEvacuationPhaseChanged OnPhaseChanged;

	static ASonHatEvacuationLink* ResolveFocusedLinkForTests(
		const TArray<TObjectPtr<ASonHatEvacuationLink>>& Links,
		const FVector& PawnLocation,
		const FVector& ViewLocation,
		const FVector& ViewDirection,
		float InteractionDistance,
		float AimHalfAngleDegrees,
		TFunctionRef<bool(const ASonHatEvacuationLink*)> LineOfSightFn);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FLinkPresentation
	{
		TObjectPtr<USplineMeshComponent> SolidLine;
		TArray<TObjectPtr<USplineMeshComponent>> DashSegments;
		TObjectPtr<USplineMeshComponent> BrokenSegmentA;
		TObjectPtr<USplineMeshComponent> BrokenSegmentB;
		TObjectPtr<USplineMeshComponent> FailCrossA;
		TObjectPtr<USplineMeshComponent> FailCrossB;
		TObjectPtr<UStaticMeshComponent> TerminalMarker;
		TObjectPtr<UMaterialInstanceDynamic> TerminalMaterial;
		TObjectPtr<UMaterialInstanceDynamic> ConnectionMaterial;
	};

	bool ResolveAndValidateNetwork();
	void RefreshController();
	void SetPhase(ESonHatEvacuationRoundPhase NewPhase);
	void ConfigurationError(const FString& Message);
	void ClearPresentation();
	void CreatePresentation();
	void UpdatePresentation();
	void UpdateFloodVisual();
	void UpdateHUD();
	void ApplyPlayerReplayReset();
	UTextRenderComponent* CreateLabel();
	USplineMeshComponent* CreateSplineSegment(const FName& Name);
	UStaticMeshComponent* CreateMarker(const FName& Name, float Radius);
	void SetSplineSegment(USplineMeshComponent* Segment, TObjectPtr<UMaterialInstanceDynamic>& MaterialSlot, const FVector& Start, const FVector& End, const FLinearColor& Color, bool bVisible);
	bool HasLineOfSight(const ASonHatEvacuationLink* Link) const;
	ASonHatEvacuationLink* ResolveFocusedLink() const;
	void EnsureMarkerMaterial(TObjectPtr<UMaterialInstanceDynamic>& Slot, const FLinearColor& Color);
	void EnsureHudWidget();

	UFUNCTION()
	void HandleRoundEnded(int32 AccessPopulation, int32 TotalPopulation);

	UPROPERTY(Transient)
	TObjectPtr<ASonHatEvacuationManager> Manager;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASonHatEvacuationNode>> RoundNodes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASonHatEvacuationLink>> RoundLinks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> NodeLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> LinkLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> NodeMarkers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> NodeMarkerMaterials;

	TArray<FLinkPresentation> LinkPresentation;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FloodPlane;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FloodMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<USonHatEvacuationHUDWidget> HudWidget;

	TWeakObjectPtr<APlayerController> Controller;
	FVector FloodCenter = FVector::ZeroVector;
	FVector2D FloodExtents = FVector2D::ZeroVector;
	double RevealStartedAt = 0;
	float RoundRevealDuration = 8.0f;
	FColor StatusColor = FColor::White;
	bool bRoundRequested = false;
	bool bHasCapturedReplayTransform = false;
	FTransform CapturedReplayTransform = FTransform::Identity;
	TArray<FString> HudLines;
	TArray<FLinearColor> HudLineColors;
};
