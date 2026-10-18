#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "GrappleComponent.generated.h"

class ACharacter;
class APlayerController;
class UGrappleReticleWidget;
class UPrimitiveComponent;
class UCharacterMovementComponent;
class UMaterialInstanceDynamic;
class USoundBase;
class UAudioComponent;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESonHatGrappleTargetState : uint8
{
	None,
	Ready,
	Blocked,
	NoHit,
	TooClose
};

UENUM(BlueprintType)
enum class ESonHatGrappleMovementStyle : uint8
{
	SwingAndReel,
	LegacyPull
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UEINTROPROJECT_1_API UGrappleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrappleComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Grapple")
	bool TryStartGrapple();

	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void StopGrapple();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grapple")
	bool IsGrappling() const;

	UFUNCTION(BlueprintPure, Category = "Grapple|Targeting")
	ESonHatGrappleTargetState GetTargetState() const { return TargetState; }

	UFUNCTION(BlueprintPure, Category = "Grapple|Targeting")
	FVector GetTargetAimPoint() const { return TargetAimPoint; }

	UFUNCTION(BlueprintPure, Category = "Grapple|Targeting")
	FString GetTargetFeedback() const;

	UFUNCTION(BlueprintCallable, Category = "Grapple|Swing")
	void SetReeling(bool bReel) { bManualReelRequested = bReel; }

	UFUNCTION(BlueprintPure, Category = "Grapple|Swing")
	bool IsReeling() const;

	UFUNCTION(BlueprintPure, Category = "Grapple|Swing")
	float GetActiveRopeLength() const { return ActiveRopeLength; }
	/** Adds a bounded impulse through CMC velocity; never teleports or changes the rope. */
	UFUNCTION(BlueprintCallable, Category = "Grapple|Air Boost")
	bool TryAirBoost();

	UFUNCTION(BlueprintPure, Category = "Grapple|Air Boost")
	bool IsAirBoostAvailable() const { return bEnableAirBoost && !bAirBoostUsed && bRoundInputEnabled && !bAwaitingFallRetry; }

	/** Reversible opt-out. Ground jump remains owned by the existing Blueprint IA_Jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Air Boost")
	bool bEnableAirBoost = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Air Boost", meta = (ClampMin = "0.0", ClampMax = "1200.0", Units = "cm/s"))
	float AirBoostUpSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Air Boost", meta = (ClampMin = "0.0", ClampMax = "600.0", Units = "cm/s"))
	float AirBoostForwardSpeed = 250.f;

	/** LegacyPull preserves the pre-V3 movement/launch settings. Switching while attached detaches safely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing")
	ESonHatGrappleMovementStyle MovementStyle = ESonHatGrappleMovementStyle::SwingAndReel;

	/** Optional native fallback for the existing Blueprint attach/release events. Idempotent with them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Input")
	bool bReadAttachInputFromController = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Input")
	FKey AttachInputKey = EKeys::RightMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Input")
	bool bReadReelInputFromController = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Input")
	FKey ReelInputKey = EKeys::LeftMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "100.0", Units = "cm"))
	float MinimumSwingRopeLength = 350.f;

	/** Rope starts at actual attach distance plus slack, never at a shorter fixed radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0", Units = "cm"))
	float InitialRopeSlack = 10.f;

	/** Uses character-to-hit distance, not camera trace distance. Also stays above the reel floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Launch Assist", meta = (ClampMin = "100.0", Units = "cm"))
	float MinimumSwingAttachDistance = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Launch Assist")
	bool bEnableAttachLaunchAssist = true;

	/** One-time tangent speed floor at takeoff. Fast airborne reattachments receive no kick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Launch Assist", meta = (ClampMin = "0.0", Units = "cm/s"))
	float AttachTangentSpeed = 1400.f;

	/** Arcade motor floor along camera yaw; useful even when the tangent points steeply down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Launch Assist", meta = (ClampMin = "0.0", Units = "cm/s"))
	float AttachForwardSpeed = 900.f;

	/** Minimum takeoff Z velocity to clear a departure roof. CMC still sweeps all movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Launch Assist", meta = (ClampMin = "0.0", Units = "cm/s"))
	float AttachUpSpeed = 850.f;

	/** Below this total airborne speed an attachment can help restart a stalled swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Launch Assist", meta = (ClampMin = "0.0", Units = "cm/s"))
	float AirborneAssistSpeedThreshold = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "100.0", Units = "cm/s"))
	float SwingSpeedLimit = 2600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0"))
	float SwingSteeringAcceleration = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ReelSpeed = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0"))
	float ReelAcceleration = 2400.f;

	/** LMB-only inward speed target, including slack rope. Uses ReelAcceleration and
	 * preserves tangent/speed limits. Stops powering inside MinimumSwingRopeLength.
	 * Zero restores the previous tension-only winch for a simple A/B comparison. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ReelPullSpeed = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0"))
	float RopeConstraintAcceleration = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RopeRecoverySpeed = 1400.f;

	/** Post-move error recovery is swept against collision and bounded in both speed and distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RopePositionCorrectionSpeed = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxRopeCorrectionPerFrame = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "10.0", Units = "cm"))
	float MaximumRopeStretch = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing")
	bool bBreakWhenRopeObstructed = true;

	/** Brief roof occlusion before detaching; still breaks after the grace window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0", Units = "s"))
	float RopeObstructionGraceSeconds = 0.3f;

	/** Logs Display-level detach reasons (obstruction grace expiry, blocked correction, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing")
	bool bLogDetachReasons = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing")
	bool bDetachOnLanding = true;

	/** Optional earned-velocity-direction assist, not camera-forward. Zero means exact release continuity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Swing", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SwingReleaseAssist = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Targeting", meta = (ClampMin = "100.0"))
	float GrappleRange = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Targeting")
	FName GrappleAnchorTag = TEXT("GrappleAnchor");

	/** City surfaces enabled by default. First visible hit must be solid WorldStatic, block Pawn,
	 * have two sufficiently large bounds dimensions and not simulate physics. Tagged anchors still work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Targeting")
	bool bAllowSolidWorldSurfaces = true;

	/** Actor OR component tag excludes decorations/water even when explicitly anchor-tagged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Targeting")
	FName GrappleExcludedTag = TEXT("NoGrapple");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Targeting", meta = (ClampMin = "10.0", Units = "cm"))
	float MinimumSurfaceHalfExtent = 100.f;

	/** Ignore only contact with the attached component within this distance of the endpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Targeting", meta = (ClampMin = "0.0", ClampMax = "30.0", Units = "cm"))
	float RopeEndpointTolerance = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	bool bShowTargetingFeedback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	bool bShowActiveTether = true;

	/** Compatibility field only. World-space targeting balls are no longer created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	TObjectPtr<UStaticMesh> AimMarkerMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	TObjectPtr<UStaticMesh> TetherBeamMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	float AimMarkerRadius = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	FLinearColor ColorReady = FLinearColor(0.1f, 1.f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	FLinearColor ColorBlocked = FLinearColor(1.f, 0.45f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	FLinearColor ColorNoHit = FLinearColor(0.65f, 0.65f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	FLinearColor ColorTether = FLinearColor(0.58f, 0.56f, 0.52f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	FLinearColor ColorAttachedReticle = FLinearColor(0.95f, 0.78f, 0.32f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation", meta = (ClampMin = "0.2", Units = "cm"))
	float TetherRadiusCm = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation", meta = (ClampMin = "0.2", Units = "cm"))
	float HookTravelCableRadiusCm = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation|Meshes")
	TObjectPtr<UStaticMesh> HookHeadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Feedback")
	TObjectPtr<USoundBase> AttachSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Feedback")
	TObjectPtr<USoundBase> ReleaseSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Feedback")
	TObjectPtr<USoundBase> HookFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Feedback")
	TObjectPtr<USoundBase> ReelSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Feedback", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float GrappleSoundVolume = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReelLoopVolume = 0.28f;

	/** Cosmetic hook travel toward the anchor; attachment is immediate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation", meta = (ClampMin = "0.02", Units = "s"))
	float HookShotTravelSeconds = 0.09f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	bool bShowHookShotTravel = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation", meta = (ClampMin = "5.0", Units = "cm"))
	float ReadyAnchorRingRadius = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "0.0"))
	float PullAcceleration = 3200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "100.0"))
	float MaxGrappleSpeed = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "100.0"))
	float RopeLength = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "0.0"))
	float RopeStretchPullAcceleration = 4800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "0.0"))
	float AirSteeringAcceleration = 900.f;

	/** Adds forward launch along camera yaw on release; velocity is preserved first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Release", meta = (ClampMin = "0.0"))
	float ReleaseLaunchBoost = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Release", meta = (ClampMin = "0.0"))
	float ReleaseLaunchMaxSpeed = 2600.f;

	/** Softens downward speed when a walkable surface is close beneath the player while grappling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Landing Assist", meta = (ClampMin = "0.0", Units = "cm"))
	float LandingAssistTraceDepth = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Landing Assist", meta = (ClampMin = "0.0", Units = "cm"))
	float LandingAssistMaxHeight = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Landing Assist", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LandingAssistFallDamping = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Landing Assist", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LandingAssistUpBoost = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Landing Assist")
	bool bLandingAssistDuringSwing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Recovery")
	bool bEnableFallRecovery = true;

	/** When true, a big fall shows an overlay and waits for AcknowledgeFallRetry() instead of instant teleport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Recovery")
	bool bRequireFallRetryAcknowledgment = true;

	UFUNCTION(BlueprintPure, Category = "Grapple|Recovery")
	bool IsAwaitingFallRetry() const { return bAwaitingFallRetry; }

	UFUNCTION(BlueprintCallable, Category = "Grapple|Recovery")
	bool AcknowledgeFallRetry();

	/** Round boundaries only; does not change swing/reel tuning or release velocity. */
	void SetRoundInputEnabled(bool bEnabled);

	/** Relative recovery works above or below world zero. Absolute Z is an explicit legacy opt-in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Recovery")
	bool bUseAbsoluteFallRecoveryZ = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Recovery", meta = (ClampMin = "300.0", Units = "cm"))
	float FallRecoveryDropDistance = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Recovery", meta = (Units = "cm"))
	float FallRecoveryWorldZ = 200.f;

	/** Clear the old roof checkpoint after replay/teleport; next supported landing establishes a new one. */
	UFUNCTION(BlueprintCallable, Category = "Grapple|Recovery")
	void ResetFallRecoveryCheckpoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation")
	bool bShowFirstUseControlHint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Presentation", meta = (ClampMin = "1.0", Units = "s"))
	float ControlHintDurationSeconds = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Debug")
	bool bDrawDebug = false;

	/** Event logs and throttled 4 Hz motion samples; off by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Debug")
	bool bLogGrappleTelemetry = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Grapple|Debug")
	float PeakSwingSpeed = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Grapple|Debug")
	FName LastDetachReason = NAME_None;

protected:
	bool PerformGrappleTrace(FHitResult& OutHit) const;
	bool IsValidGrappleSurface(const FHitResult& Hit) const;
	void EvaluateTarget();
	void UpdatePresentation();
	void ApplyGrappleMovement(float DeltaTime);
	void ApplyLegacyGrappleMovement(float DeltaTime);
	void RefreshInputController();
	bool RefreshAnchorPoint(float DeltaSeconds);
	void UpdateFallRecovery(float DeltaSeconds);
	void UpdateLastSafeGround();
	void SaveAndOverrideAirControl(UCharacterMovementComponent* Movement);
	void RestoreAirControl(bool bPreserveReleaseMomentum = false);
	void UpdateReticle();

	UFUNCTION()
	void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	void ApplyLandingAssist(UCharacterMovementComponent* MoveComp, FVector& InOutVelocity) const;
	void PlayFeedbackSound(USoundBase* Sound) const;
	ACharacter* GetOwningCharacter() const;
	void ClearGrappleState(FName Reason = TEXT("Reset"));
	bool IsSwingTargetTooClose(const FVector& HitPoint) const;
	FVector ApplyAttachLaunchAssist(UCharacterMovementComponent* Movement);
	void UpdateGrappleTelemetry();
	void EnsurePresentation();
	void BeginHookShotVisual(const FVector& EndWorld);
	void UpdateHookShotVisual(float DeltaSeconds);
	void FinishHookShotVisual();
	void SuppressObsoleteOwnerSplineVisuals();
	void ConfigureFirstPersonOwnerVisibility();
	void ResolveDefaultFeedbackSounds();
	void HideHookCosmetics();
	FVector GetHookLaunchOriginWorld() const;
	FVector GetCableOriginWorld() const;
	void ApplyCableSpline(USplineMeshComponent* Spline, const FVector& Start, const FVector& End, float RadiusCm) const;
	void UpdateReelLoopAudio(bool bShouldPlay);
	void UpdateAirBoostInput();

	UPROPERTY(Transient)
	TObjectPtr<AActor> GrappleAnchorActor;

	FVector GrappleAnchorWorldPoint = FVector::ZeroVector;
	FVector TargetAimPoint = FVector::ZeroVector;
	ESonHatGrappleTargetState TargetState = ESonHatGrappleTargetState::None;
	bool bAwaitingFallRetry = false;
	bool bRoundInputEnabled = true;
	bool bIsGrapplingActive = false;
	bool bAirBoostUsed = false;
	bool bAirBoostSpaceWasDown = false;
	bool bSpaceJumpRequested = false;
	float AirBoostFeedbackRemaining = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> AimMarker;

	UPROPERTY(Transient)
	TObjectPtr<USplineMeshComponent> TetherBeam;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AimMarkerMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TetherMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UGrappleReticleWidget> Reticle;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HookBolt;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HookMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ReelLoopAudio;

	bool bHookImpactSoundPlayed = false;
	float HookImpactFlashRemaining = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USplineMeshComponent> HookTravelRope;

	float HookShotTimeRemaining = 0.f;
	FVector HookShotStartWorld = FVector::ZeroVector;
	FVector HookShotEndWorld = FVector::ZeroVector;

	TWeakObjectPtr<APlayerController> InputController;
	TWeakObjectPtr<UPrimitiveComponent> AnchorComponent;
	TWeakObjectPtr<UCharacterMovementComponent> OverriddenMovement;
	FVector AnchorLocalPoint = FVector::ZeroVector;
	float ActiveRopeLength = 0;
	float CurrentWinchSpeed = 0;
	float SavedAirControl = 0;
	float SavedAirControlBoost = 0;
	float SavedFallingFriction = 0;
	float SavedFallingBraking = 0;
	ESonHatGrappleMovementStyle AttachedStyle = ESonHatGrappleMovementStyle::SwingAndReel;
	bool bManualReelRequested = false;
	bool bAnchorUsesComponent = false;
	bool bHasBeenAirborne = false;
	bool bHandlingConstraint = false;
	bool bLastReelInput = false;
	bool bPreservingReleaseMomentum = false;
	float RopeObstructionElapsed = 0.f;
	/** Throttled motion samples only when existing event telemetry is explicitly enabled. */
	double NextMotionTelemetryTime = 0;
	/** Negative until the first-use hint is armed once a local controller exists. */
	float ControlHintRemaining = -1.f;
	bool bHasLastSafeGround = false;
	FVector LastSafeGroundLocation = FVector::ZeroVector;
	FRotator LastSafeGroundRotation = FRotator::ZeroRotator;
};
