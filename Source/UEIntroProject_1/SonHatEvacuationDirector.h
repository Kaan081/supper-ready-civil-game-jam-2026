#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SonHatEvacuationManager.h"
#include "SonHatEvacuationDirector.generated.h"

class APlayerController;
class UMaterialInstanceDynamic;
class USonHatEvacuationHUDWidget;
class UAudioComponent;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundBase;

/** Terminal access only. Does not change the connection's authored bFloodSafe rating. */
USTRUCT(BlueprintType)
struct FSonHatTerminalFloodRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood")
	TObjectPtr<ASonHatEvacuationLink> Terminal;

	/** False protects this activation terminal, regardless of water height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flood")
	bool bVulnerable = true;
};

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

/** One logical rooftop group. Counts remain on PopulationNode; Beacon is a direct SAFE hub link. */
USTRUCT(BlueprintType)
struct FSonHatRescueGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rescue")
	FName GroupId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rescue")
	TObjectPtr<ASonHatEvacuationNode> PopulationNode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rescue")
	TObjectPtr<ASonHatEvacuationLink> Beacon;

	/** Verified feet-origin gathering point, with facing. Reuses the level's population anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rescue")
	TObjectPtr<AActor> PopulationAnchor;

	/** Safe local XY region around PopulationAnchor; figures outside it are omitted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rescue", meta = (Units = "cm"))
	FVector2D PopulationHalfExtents = FVector2D(200, 150);

	/** Verified walkable roof floor Z, not the actor pivot or standing capsule center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rescue", meta = (Units = "cm"))
	float FloodClosureElevation = 0.f;
};

/** Level integration only: uses the existing pawn/rooftops and externally placed nodes/terminals. */
UCLASS(Blueprintable)
class UEINTROPROJECT_1_API ASonHatEvacuationDirector : public AActor
{
	GENERATED_BODY()

public:
	ASonHatEvacuationDirector();
	virtual void Tick(float DeltaSeconds) override;

	/** Opt in only with a verified four-beacon map. False preserves legacy test levels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rescue")
	bool bRescueSignalMode = false;

	/** Exactly A/B/C/D, with populations 35/45/60/20. Existing Nodes/CandidateLinks must match. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Son Hat|Rescue", meta = (EditCondition = "bRescueSignalMode"))
	TArray<FSonHatRescueGroup> RescueGroups;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rescue", meta = (ClampMin = "1", Units = "s", EditCondition = "bRescueSignalMode"))
	float RescuePreparationDuration = 150.f;

	UFUNCTION(BlueprintPure, Category = "Son Hat|Rescue")
	FName GetRescueGroupId(const ASonHatEvacuationNode* Node) const;

	FString GetRescueBeaconTitle(const ASonHatEvacuationLink* Beacon) const;

	/** Discover loaded level actors once at round start. No nodes, links, terrain or pawns are generated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Level Network")
	bool bAutoDiscoverNetwork = true;

	/** Optional Actor Tag shared by the nodes AND links of this network. None discovers all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Level Network", meta = (EditCondition = "bAutoDiscoverNetwork"))
	FName NetworkTag = NAME_None;

	/** Optional startup guard for streamed content. Zero disables the count check.
	 * Keep the whole network loaded for the round; start manually after streaming completes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Level Network", meta = (ClampMin = "0"))
	int32 ExpectedNodeCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Level Network", meta = (ClampMin = "0"))
	int32 ExpectedLinkCount = 0;

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

	/** Explicit bounded orientation: clock/water hold, then the full challenge starts. First connection ends it early. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Onboarding", meta = (ClampMin = "0", ClampMax = "20", Units = "s"))
	float OrientationDuration = 0.f;

	/** Integration-owned world bounds, independent of the last rooftop height and the flood. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Recovery")
	bool bUseLevelRecoveryBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Recovery", meta = (Units = "cm"))
	float LevelRecoveryWorldZ = -900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Recovery", meta = (EditCondition = "bUseLevelRecoveryBounds"))
	bool bAutomaticallyRecoverBelowBounds = true;

	UFUNCTION(BlueprintPure, Category = "Son Hat|Onboarding")
	float GetOrientationSecondsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Son Hat|Onboarding")
	FString GetNextObjectiveText() const;

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

	/** Legacy reveal-only setting retained for old levels. V1 uses the absolute elevations below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Flood Reveal", meta = (ClampMin = "0.0", Units = "cm"))
	float FloodRiseDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood")
	bool bEnableRisingFlood = true;

	/** Absolute world Z in cm. Director Z no longer places the waterline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood", meta = (Units = "cm"))
	float FloodStartElevation = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood", meta = (Units = "cm"))
	float FloodFinalElevation = 1650.0f;

	/** Height = lerp(start, final, progress^exponent). 1 is linear; >1 accelerates late. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float FloodRiseExponent = 1.2f;

	/** Additional visual rise during the existing locked flood reveal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood", meta = (ClampMin = "0.0", Units = "cm"))
	float FloodRevealSurge = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood")
	bool bEnableTerminalFlooding = true;

	/** By default all terminals close at their actor-origin Z. Use overrides to protect specific terminals. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Son Hat|Rising Flood")
	TArray<FSonHatTerminalFloodRule> TerminalFloodRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood", meta = (ClampMin = "1.0", Units = "s"))
	float TerminalFloodWarningSeconds = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood")
	FLinearColor ColorSubmergedTerminal = FLinearColor(0.65f, 0.15f, 1.0f);

	/** Optional assets; no audio dependency or generated sound is required. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood|Audio")
	TObjectPtr<USoundBase> FloodWarningSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Rising Flood|Audio")
	TObjectPtr<USoundBase> FloodUrgentSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio")
	TObjectPtr<USoundBase> LinkActivationSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio")
	TObjectPtr<USoundBase> LinkActivationUnsafeSound;

	/** Optional loop during preparation (storm/rain). Assign in level or defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio")
	TObjectPtr<USoundBase> PreparationAmbienceSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreparationAmbienceVolume = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio")
	TObjectPtr<USoundBase> FloodSirenAmbienceSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FloodSirenAmbienceVolume = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio")
	TObjectPtr<USoundBase> FloodRumbleAmbienceSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FloodRumbleAmbienceVolume = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio")
	TObjectPtr<USoundBase> TerminalSubmergedSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Round|Audio")
	TObjectPtr<USoundBase> RoundResultsSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Flood Reveal")
	bool bFitFloodToNetwork = true;

	/** Used if automatic XY bounds are disabled. In centimeters, relative to director XY. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Flood Reveal", meta = (EditCondition = "!bFitFloodToNetwork", ClampMin = "100.0"))
	FVector2D FloodHalfSize = FVector2D(6000, 6000);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation", meta = (ClampMin = "10.0", Units = "cm"))
	float LabelWorldSize = 18.0f;

	/** Hides redundant node/link billboards during preparation; population presenter carries building state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation")
	bool bCompactWorldPresentation = true;

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
	float NodeMarkerRadius = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style", meta = (ClampMin = "5.0", Units = "cm"))
	float TerminalMarkerRadius = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Presentation|Style", meta = (ClampMin = "5.0", Units = "cm"))
	float FocusedTerminalRadius = 26.0f;

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

	/** Closest available safe terminal that extends access, then closest other available terminal.
	 * A navigation suggestion only; activation still uses focused LOS/range and manager authority. */
	UFUNCTION(BlueprintPure, Category = "Son Hat|Round")
	ASonHatEvacuationLink* GetSuggestedTerminal() const;

	UFUNCTION(BlueprintPure, Category = "Son Hat|Round")
	ASonHatEvacuationManager* GetRoundManager() const { return Manager; }

	UFUNCTION(BlueprintPure, Category = "Son Hat|Presentation")
	USonHatEvacuationHUDWidget* GetHudWidget() const { return HudWidget; }

	UFUNCTION(BlueprintPure, Category = "Son Hat|Round")
	float GetFloodRevealProgress() const;

	UFUNCTION(BlueprintPure, Category = "Son Hat|Rising Flood")
	float GetFloodHeight() const;

	UFUNCTION(BlueprintPure, Category = "Son Hat|Rising Flood")
	bool IsTerminalSubmerged(const ASonHatEvacuationLink* Terminal) const;

	/** -1 means it will not close during preparation, 0 means already submerged. */
	UFUNCTION(BlueprintPure, Category = "Son Hat|Rising Flood")
	float GetTerminalSecondsUntilFlood(const ASonHatEvacuationLink* Terminal) const;

	static float CalculateFloodHeight(float Progress, float Start, float Final, float Exponent);

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
		TFunctionRef<bool(const ASonHatEvacuationLink*)> LineOfSightFn,
		TFunction<FVector(const ASonHatEvacuationLink*)> AimPositionFn = {});

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
		TArray<TObjectPtr<UStaticMeshComponent>> ConsoleParts;
		TObjectPtr<UMaterialInstanceDynamic> TerminalMaterial;
		TObjectPtr<UMaterialInstanceDynamic> ConnectionMaterial;
		FVector TerminalGroundOffset = FVector(0, 0, -45);
		double RegisteredAt = -1;
	};

	bool ResolveAndValidateNetwork();
	bool ValidateRescueNetwork();
	float GetTerminalClosureHeight(const ASonHatEvacuationLink* Terminal) const;
	void RefreshController();
	void SetPhase(ESonHatEvacuationRoundPhase NewPhase);
	void ConfigurationError(const FString& Message);
	void ClearPresentation();
	void CreatePresentation();
	void UpdatePresentation();
	void UpdateFloodVisual();
	void UpdateTerminalFlooding();
	bool IsTerminalFloodVulnerable(const ASonHatEvacuationLink* Terminal) const;
	bool IsTerminalFloodWarning(const ASonHatEvacuationLink* Terminal) const;
	void UpdateHUD();
	void UpdateOrientationClock();
	bool ApplyPlayerReplayReset();
	UTextRenderComponent* CreateLabel();
	USplineMeshComponent* CreateSplineSegment(const FName& Name);
	UStaticMeshComponent* CreateMarker(const FName& Name, float Radius);
	void SetSplineSegment(USplineMeshComponent* Segment, TObjectPtr<UMaterialInstanceDynamic>& MaterialSlot, const FVector& Start, const FVector& End, const FLinearColor& Color, bool bVisible);
	bool HasLineOfSight(const ASonHatEvacuationLink* Link) const;
	FVector GetTerminalDisplayPosition(const ASonHatEvacuationLink* Link) const;
	ASonHatEvacuationLink* ResolveFocusedLink() const;
	void EnsureMarkerMaterial(TObjectPtr<UMaterialInstanceDynamic>& Slot, const FLinearColor& Color);
	void EnsureHudWidget();
	void UpdateTerminalWayfinding();

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
	UPROPERTY()
	TObjectPtr<UStaticMesh> TerminalConsoleMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ConsoleHousingMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ConsoleTrimMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ConsoleDisplayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PreparationAmbience;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FloodSirenAmbience;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FloodRumbleAmbience;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RoundFeedbackCue;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RoundAlertCue;

	void ResolveDefaultRoundSounds();
	void SuppressGrappleAnchorCosmetics();
	void StopRoundAmbience();
	void StartRoundAmbience();
	void StopRoundCues();
	void PlayRoundCue(USoundBase* Sound, float Volume = 0.65f, bool bAlert = false);

	TWeakObjectPtr<APlayerController> Controller;
	FVector FloodCenter = FVector::ZeroVector;
	FVector2D FloodExtents = FVector2D::ZeroVector;
	double RevealStartedAt = 0;
	double PreparationStartedAt = 0;
	double OrientationEndsAt = 0;
	double OrientationLastUpdatedAt = 0;
	double StatusUntil = 0;
	double NextSubmergedCueAt = 0;
	float RoundPreparationDuration = 90;
	float RoundFloodStart = 200;
	float RoundFloodFinal = 1650;
	float RoundFloodExponent = 1.2f;
	float RoundFloodSurge = 100;
	float RevealStartFloodHeight = 1650;
	bool bPlayedUrgentFloodSound = false;
	TSet<TWeakObjectPtr<ASonHatEvacuationLink>> SubmergedTerminals;
	float RoundRevealDuration = 8.0f;
	FColor StatusColor = FColor::White;
	bool bRoundRequested = false;
	bool bHasCapturedReplayTransform = false;
	FTransform CapturedReplayTransform = FTransform::Identity;
	TArray<FString> HudLines;
	TArray<FLinearColor> HudLineColors;
};
