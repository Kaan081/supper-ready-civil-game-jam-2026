#include "GrappleComponent.h"
#include "GrappleReticleWidget.h"
#include "GrappleSwingPhysics.h"

#include "Components/AudioComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSonHatGrapple, Log, All);

namespace
{
	float GrappleSetting(float Value, float Fallback, float Minimum = 0)
	{
		return FMath::IsFinite(Value) ? FMath::Max(Minimum, Value) : Fallback;
	}
}

UGrappleComponent::UGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded()) { TetherBeamMesh = CylinderMesh.Object; }
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded()) { HookHeadMesh = ConeMesh.Object; }
}

void UGrappleComponent::ResolveDefaultFeedbackSounds()
{
	auto LoadIfNull = [](TObjectPtr<USoundBase>& Slot, const TCHAR* Path)
	{
		if (!Slot) { Slot = LoadObject<USoundBase>(nullptr, Path); }
	};
	LoadIfNull(HookFireSound, TEXT("/Game/Jam/A/Audio/SFX_GrappleFire.SFX_GrappleFire"));
	LoadIfNull(AttachSound, TEXT("/Game/Jam/A/Audio/SFX_GrappleAttach.SFX_GrappleAttach"));
	LoadIfNull(ReleaseSound, TEXT("/Game/Jam/A/Audio/SFX_GrappleRelease.SFX_GrappleRelease"));
	LoadIfNull(ReelSound, TEXT("/Game/Jam/A/Audio/SFX_GrappleReelLoop.SFX_GrappleReelLoop"));
	if (USoundWave* ReelWave = Cast<USoundWave>(ReelSound.Get())) { ReelWave->bLooping = true; }
}

FVector UGrappleComponent::GetHookLaunchOriginWorld() const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character) { return FVector::ZeroVector; }
	if (const AController* Controller = Character->GetController())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		return ViewLocation + ViewRotation.RotateVector(FVector(28.f, 18.f, -8.f));
	}
	return Character->GetActorLocation() + Character->GetActorForwardVector() * 45.f + FVector(0, 0, 55.f);
}

FVector UGrappleComponent::GetCableOriginWorld() const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character) { return FVector::ZeroVector; }
	return Character->GetActorLocation() + Character->GetActorForwardVector() * 35.f + FVector(0, 0, 58.f);
}

void UGrappleComponent::ApplyCableSpline(USplineMeshComponent* Spline, const FVector& Start, const FVector& End, float RadiusCm) const
{
	if (!Spline) { return; }
	const FVector Tangent = End - Start;
	Spline->SetWorldLocation(FVector::ZeroVector);
	Spline->SetWorldRotation(FRotator::ZeroRotator);
	Spline->SetStartAndEnd(Start, Tangent, End, Tangent);
	const float Scale = RadiusCm / 50.f;
	Spline->SetStartScale(FVector2D(Scale, Scale));
	Spline->SetEndScale(FVector2D(Scale, Scale));
}

void UGrappleComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsurePresentation();
	SuppressObsoleteOwnerSplineVisuals();
	ConfigureFirstPersonOwnerVisibility();
	ResolveDefaultFeedbackSounds();

	if (ACharacter* Character = GetOwningCharacter())
	{
		Character->OnCharacterMovementUpdated.AddDynamic(this, &UGrappleComponent::HandleCharacterMovementUpdated);
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
		}
	}
}

void UGrappleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearGrappleState(TEXT("EndPlay"));
	if (ACharacter* Character = GetOwningCharacter())
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &UGrappleComponent::HandleCharacterMovementUpdated);
		Character->GetCharacterMovement()->PrimaryComponentTick.RemovePrerequisite(this, PrimaryComponentTick);
	}
	if (InputController.IsValid()) { RemoveTickPrerequisiteActor(InputController.Get()); }
	if (Reticle) { Reticle->RemoveFromParent(); Reticle = nullptr; }
	if (IsValid(AimMarker))
	{
		AimMarker->DestroyComponent();
		AimMarker = nullptr;
	}
	if (IsValid(TetherBeam))
	{
		TetherBeam->DestroyComponent();
		TetherBeam = nullptr;
	}
	if (IsValid(HookTravelRope))
	{
		HookTravelRope->DestroyComponent();
		HookTravelRope = nullptr;
	}
	if (IsValid(HookBolt))
	{
		HookBolt->DestroyComponent();
		HookBolt = nullptr;
	}
	if (IsValid(ReelLoopAudio))
	{
		ReelLoopAudio->Stop();
		ReelLoopAudio->DestroyComponent();
		ReelLoopAudio = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UGrappleComponent::ConfigureFirstPersonOwnerVisibility()
{
	ACharacter* Character = GetOwningCharacter();
	if (!Character || !Character->IsLocallyControlled()) { return; }
	if (USkeletalMeshComponent* Body = Character->GetMesh())
	{
		Body->SetOwnerNoSee(true);
		Body->SetCastHiddenShadow(false);
	}
	TArray<USkeletalMeshComponent*> SkMeshes;
	Character->GetComponents(SkMeshes);
	for (USkeletalMeshComponent* SkMesh : SkMeshes)
	{
		if (!SkMesh || SkMesh == Character->GetMesh()) { continue; }
		SkMesh->SetOwnerNoSee(true);
	}
	TArray<UStaticMeshComponent*> StaticMeshes;
	Character->GetComponents(StaticMeshes);
	for (UStaticMeshComponent* Mesh : StaticMeshes)
	{
		if (!Mesh || Mesh == TetherBeam || Mesh == HookBolt || Mesh == HookTravelRope) { continue; }
		const FName Name = Mesh->GetFName();
		if (Name.ToString().Contains(TEXT("Arm"), ESearchCase::IgnoreCase)
			|| Name.ToString().Contains(TEXT("Body"), ESearchCase::IgnoreCase)
			|| Name.ToString().Contains(TEXT("Mesh"), ESearchCase::IgnoreCase))
		{
			Mesh->SetOwnerNoSee(true);
			Mesh->SetHiddenInGame(true);
		}
	}
}

void UGrappleComponent::HideHookCosmetics()
{
	if (HookTravelRope) { HookTravelRope->SetVisibility(false, true); }
	if (HookBolt) { HookBolt->SetVisibility(false, true); }
}

void UGrappleComponent::SuppressObsoleteOwnerSplineVisuals()
{
	ACharacter* Character = GetOwningCharacter();
	if (!Character) { return; }
	TArray<USplineMeshComponent*> Splines;
	Character->GetComponents(Splines);
	for (USplineMeshComponent* Spline : Splines)
	{
		if (!Spline || Spline == TetherBeam || Spline == HookTravelRope) { continue; }
		Spline->SetVisibility(false, true);
		Spline->SetHiddenInGame(true);
	}
}

void UGrappleComponent::EnsurePresentation()
{
	ACharacter* Character = GetOwningCharacter();
	if (!Character || !GetWorld()) { return; }

	if (!TetherBeam && TetherBeamMesh)
	{
		TetherBeam = NewObject<USplineMeshComponent>(Character);
		Character->AddInstanceComponent(TetherBeam);
		TetherBeam->SetMobility(EComponentMobility::Movable);
		TetherBeam->SetupAttachment(Character->GetRootComponent());
		TetherBeam->SetAbsolute(true, true, true);
		TetherBeam->SetStaticMesh(TetherBeamMesh);
		TetherBeam->SetForwardAxis(ESplineMeshAxis::Z);
		TetherBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TetherBeam->SetCastShadow(false);
		TetherBeam->RegisterComponent();
		TetherMaterial = TetherBeam->CreateDynamicMaterialInstance(0);
		TetherBeam->SetVisibility(false, true);
	}
	if (!HookBolt && (HookHeadMesh || TetherBeamMesh))
	{
		HookBolt = NewObject<UStaticMeshComponent>(Character);
		Character->AddInstanceComponent(HookBolt);
		HookBolt->SetupAttachment(Character->GetRootComponent());
		HookBolt->SetStaticMesh(HookHeadMesh ? HookHeadMesh : TetherBeamMesh);
		HookBolt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HookBolt->SetCastShadow(false);
		HookBolt->SetMobility(EComponentMobility::Movable);
		HookBolt->SetWorldScale3D(FVector(0.09f, 0.09f, 0.16f));
		HookBolt->RegisterComponent();
		HookMaterial = HookBolt->CreateDynamicMaterialInstance(0);
		if (HookMaterial) { HookMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.22f, 0.23f, 0.26f)); }
		HookBolt->SetVisibility(false, true);
	}
	if (bShowHookShotTravel && !HookTravelRope && TetherBeamMesh)
	{
		HookTravelRope = NewObject<USplineMeshComponent>(Character);
		Character->AddInstanceComponent(HookTravelRope);
		HookTravelRope->SetupAttachment(Character->GetRootComponent());
		HookTravelRope->SetMobility(EComponentMobility::Movable);
		HookTravelRope->SetAbsolute(true, true, true);
		HookTravelRope->SetStaticMesh(TetherBeamMesh);
		HookTravelRope->SetForwardAxis(ESplineMeshAxis::Z);
		HookTravelRope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HookTravelRope->SetCastShadow(false);
		HookTravelRope->RegisterComponent();
		HookTravelRope->SetVisibility(false, true);
		if (!TetherMaterial)
		{
			TetherMaterial = TetherBeam ? TetherBeam->CreateDynamicMaterialInstance(0) : HookTravelRope->CreateDynamicMaterialInstance(0);
		}
		if (TetherMaterial) { TetherMaterial->SetVectorParameterValue(TEXT("Color"), ColorTether); }
	}
}

void UGrappleComponent::BeginHookShotVisual(const FVector& EndWorld)
{
	if (!bShowHookShotTravel) { return; }
	if (!GetOwningCharacter()) { return; }
	EnsurePresentation(); // Also handles a Blueprint enabling travel after BeginPlay.
	HookShotStartWorld = GetHookLaunchOriginWorld();
	HookShotEndWorld = EndWorld;
	HookShotTimeRemaining = FMath::Max(0.02f, HookShotTravelSeconds);
	bHookImpactSoundPlayed = false;
	HookImpactFlashRemaining = 0.f;
	if (HookBolt) { HookBolt->SetVisibility(true, true); }
	if (HookTravelRope) { HookTravelRope->SetVisibility(true, true); }
	if (TetherBeam) { TetherBeam->SetVisibility(false, true); }
}

void UGrappleComponent::FinishHookShotVisual()
{
	if (HookTravelRope) { HookTravelRope->SetVisibility(false, true); }
	if (HookBolt) { HookBolt->SetVisibility(false, true); }
	if (!bHookImpactSoundPlayed)
	{
		PlayFeedbackSound(AttachSound);
		bHookImpactSoundPlayed = true;
	}
	HookImpactFlashRemaining = 0.08f;
}

void UGrappleComponent::UpdateHookShotVisual(float DeltaSeconds)
{
	if (!bIsGrapplingActive && HookShotTimeRemaining <= 0.f)
	{
		HideHookCosmetics();
		return;
	}
	if (HookImpactFlashRemaining > 0.f)
	{
		HookImpactFlashRemaining -= DeltaSeconds;
		if (HookMaterial)
		{
			const float Flash = FMath::Clamp(HookImpactFlashRemaining / 0.08f, 0.f, 1.f);
			HookMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::LerpUsingHSV(
				FLinearColor(0.85f, 0.55f, 0.15f), FLinearColor(0.22f, 0.23f, 0.26f, 1.f), 1.f - Flash));
		}
	}
	if (HookShotTimeRemaining <= 0.f) { return; }
	HookShotTimeRemaining -= DeltaSeconds;
	const float Duration = FMath::Max(0.02f, HookShotTravelSeconds);
	const float Alpha = 1.f - FMath::Clamp(HookShotTimeRemaining / Duration, 0.f, 1.f);
	const FVector BoltPos = FMath::Lerp(HookShotStartWorld, HookShotEndWorld, Alpha);
	if (HookBolt)
	{
		const FVector Dir = (HookShotEndWorld - HookShotStartWorld).GetSafeNormal();
		HookBolt->SetWorldLocation(BoltPos);
		if (!Dir.IsNearlyZero()) { HookBolt->SetWorldRotation(Dir.Rotation()); }
		HookBolt->SetVisibility(true, true);
		if (HookMaterial && HookImpactFlashRemaining <= 0.f)
		{
			HookMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.22f, 0.23f, 0.26f));
		}
	}
	if (HookTravelRope && HookTravelRope->GetStaticMesh())
	{
		ApplyCableSpline(HookTravelRope, HookShotStartWorld, BoltPos, HookTravelCableRadiusCm);
		if (TetherMaterial) { TetherMaterial->SetVectorParameterValue(TEXT("Color"), ColorTether * 0.85f); }
		HookTravelRope->SetVisibility(true, true);
	}
	if (HookShotTimeRemaining <= 0.f) { FinishHookShotVisual(); }
}

void UGrappleComponent::EvaluateTarget()
{
	TargetState = ESonHatGrappleTargetState::NoHit;
	TargetAimPoint = FVector::ZeroVector;

	FHitResult Hit;
	if (!PerformGrappleTrace(Hit))
	{
		return;
	}

	TargetAimPoint = Hit.ImpactPoint;
	if (IsValidGrappleSurface(Hit))
	{
		TargetState = MovementStyle == ESonHatGrappleMovementStyle::SwingAndReel && IsSwingTargetTooClose(Hit.ImpactPoint)
			? ESonHatGrappleTargetState::TooClose : ESonHatGrappleTargetState::Ready;
	}
	else
	{
		TargetState = ESonHatGrappleTargetState::Blocked;
	}
}

void UGrappleComponent::UpdatePresentation()
{
	EnsurePresentation();
	const ACharacter* Character = GetOwningCharacter();
	if (!Character) { return; }
	UpdateReticle();
	if (bIsGrapplingActive && bShowActiveTether && TetherBeam)
	{
		if (bShowHookShotTravel && HookShotTimeRemaining > 0.f)
		{
			TetherBeam->SetVisibility(false, true);
			return;
		}
		if (HookTravelRope) { HookTravelRope->SetVisibility(false, true); }
		if (HookBolt) { HookBolt->SetVisibility(false, true); }
		TetherBeam->SetVisibility(true, true);
		const FVector Start = GetCableOriginWorld();
		const FVector End = GrappleAnchorWorldPoint;
		const float ReelPulse = IsReeling() ? (0.92f + 0.08f * FMath::Sin(GetWorld()->GetTimeSeconds() * 14.f)) : 1.f;
		ApplyCableSpline(TetherBeam, Start, End, TetherRadiusCm * ReelPulse);
		if (TetherMaterial)
		{
			const FLinearColor CableColor = IsReeling()
				? FLinearColor::LerpUsingHSV(ColorTether, FLinearColor(0.72f, 0.68f, 0.62f), 0.35f) : ColorTether;
			TetherMaterial->SetVectorParameterValue(TEXT("Color"), CableColor);
		}
		return;
	}

	HideHookCosmetics();
	if (TetherBeam) { TetherBeam->SetVisibility(false, true); }
}

void UGrappleComponent::PlayFeedbackSound(USoundBase* Sound) const
{
	if (!Sound) { return; }
	if (const ACharacter* Character = GetOwningCharacter())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Character->GetActorLocation(), FRotator::ZeroRotator,
			GrappleSoundVolume);
	}
}

void UGrappleComponent::UpdateReelLoopAudio(bool bShouldPlay)
{
	if (!ReelSound) { return; }
	ACharacter* Character = GetOwningCharacter();
	if (!Character) { return; }
	if (!ReelLoopAudio)
	{
		ReelLoopAudio = NewObject<UAudioComponent>(Character);
		Character->AddInstanceComponent(ReelLoopAudio);
		ReelLoopAudio->SetupAttachment(Character->GetRootComponent());
		ReelLoopAudio->bAutoActivate = false;
		ReelLoopAudio->bAutoDestroy = false;
		ReelLoopAudio->SetSound(ReelSound);
		ReelLoopAudio->RegisterComponent();
	}
	ReelLoopAudio->SetVolumeMultiplier(ReelLoopVolume);
	if (bShouldPlay)
	{
		if (!ReelLoopAudio->IsPlaying()) { ReelLoopAudio->Play(); }
	}
	else if (ReelLoopAudio->IsPlaying())
	{
		ReelLoopAudio->Stop();
	}
}

void UGrappleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bPreservingReleaseMomentum && (!OverriddenMovement.IsValid() || !OverriddenMovement->IsFalling()
		|| MovementStyle != ESonHatGrappleMovementStyle::SwingAndReel)) { RestoreAirControl(); }
	RefreshInputController();
	UpdateAirBoostInput(); // PC input is processed; CMC has not performed this frame's jump yet.
	AirBoostFeedbackRemaining = FMath::Max(0.f, AirBoostFeedbackRemaining - DeltaTime);
	if (bRoundInputEnabled && !bAwaitingFallRetry && bReadAttachInputFromController && InputController.IsValid())
	{
		if (InputController->WasInputKeyJustReleased(AttachInputKey)) { StopGrapple(); }
		else if (InputController->WasInputKeyJustPressed(AttachInputKey) && !IsGrappling()) { TryStartGrapple(); }
	}
	if (bIsGrapplingActive && MovementStyle != AttachedStyle) { ClearGrappleState(TEXT("StyleChanged")); }
	if (bIsGrapplingActive && !RefreshAnchorPoint(DeltaTime)) { return; }

	UpdateLastSafeGround();
	UpdateFallRecovery(DeltaTime);

	if (!bIsGrapplingActive)
	{
		EvaluateTarget();
	}
	if (bShowFirstUseControlHint && ControlHintRemaining < 0.f && InputController.IsValid())
	{
		ControlHintRemaining = FMath::Max(1.f, ControlHintDurationSeconds);
	}
	else if (ControlHintRemaining > 0.f)
	{
		ControlHintRemaining -= DeltaTime;
	}
	UpdateHookShotVisual(DeltaTime);
	UpdatePresentation();

	if (!bIsGrapplingActive)
	{
		return;
	}

	if (!IsValid(GrappleAnchorActor))
	{
		ClearGrappleState(TEXT("AnchorLost"));
		return;
	}

	ApplyGrappleMovement(DeltaTime);
	UpdateGrappleTelemetry();

	if (bDrawDebug)
	{
		if (const ACharacter* Character = GetOwningCharacter())
		{
			DrawDebugLine(GetWorld(), Character->GetActorLocation(), GrappleAnchorWorldPoint, FColor::Cyan, false, 0.f, 0, 2.f);
		}
	}
}

bool UGrappleComponent::TryStartGrapple()
{
	if (!bRoundInputEnabled || bAwaitingFallRetry) { return false; }
	if (IsGrappling()) { return true; } // Blueprint + native input must not reset the winch twice.
	if (bPreservingReleaseMomentum && MovementStyle != ESonHatGrappleMovementStyle::SwingAndReel) { RestoreAirControl(); }
	FHitResult Hit;
	const bool bTraceHit = PerformGrappleTrace(Hit);
	const AActor* HitActor = bTraceHit ? Hit.GetActor() : nullptr;
	const bool bTagMatch = IsValid(HitActor) && HitActor->ActorHasTag(GrappleAnchorTag);
	const float TraceDistance = bTraceHit ? Hit.Distance : GrappleRange;
	if (bLogGrappleTelemetry) { UE_LOG(LogSonHatGrapple, Display, TEXT("Acquire style=%s cameraHitCm=%.1f hit=%s class=%s tag=%s"),
		*UEnum::GetValueAsString(MovementStyle), TraceDistance,
		IsValid(HitActor) ? *HitActor->GetName() : TEXT("None"),
		IsValid(HitActor) ? *HitActor->GetClass()->GetName() : TEXT("None"), bTagMatch ? TEXT("true") : TEXT("false")); }

	if (!bTraceHit || !IsValidGrappleSurface(Hit))
	{
		return false;
	}
	if (MovementStyle == ESonHatGrappleMovementStyle::SwingAndReel && IsSwingTargetTooClose(Hit.ImpactPoint))
	{
		TargetState = ESonHatGrappleTargetState::TooClose;
		TargetAimPoint = Hit.ImpactPoint;
		if (bLogGrappleTelemetry) { UE_LOG(LogSonHatGrapple, Display, TEXT("Attach rejected: TooClose pawnHitCm=%.1f cameraHitCm=%.1f minRopeCm=%.1f"),
			FVector::Distance(GetOwner()->GetActorLocation(), Hit.ImpactPoint), TraceDistance, MinimumSwingRopeLength); }
		UpdatePresentation();
		return false;
	}

	GrappleAnchorActor = const_cast<AActor*>(HitActor);
	GrappleAnchorWorldPoint = Hit.ImpactPoint;
	AnchorComponent = Hit.GetComponent();
	bAnchorUsesComponent = AnchorComponent.IsValid();
	AnchorLocalPoint = bAnchorUsesComponent ? AnchorComponent->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint)
		: GrappleAnchorActor->GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
	AttachedStyle = MovementStyle;
	CurrentWinchSpeed = 0;
	NextMotionTelemetryTime = 0;
	bManualReelRequested = false;
	bLastReelInput = false;
	PeakSwingSpeed = 0;
	if (ACharacter* Character = GetOwningCharacter())
	{
		ActiveRopeLength = FMath::Max(FMath::IsFinite(MinimumSwingRopeLength) ? FMath::Max(100.0f, MinimumSwingRopeLength) : 350.0f,
			static_cast<float>(FVector::Distance(Character->GetActorLocation(), Hit.ImpactPoint))
			+ (FMath::IsFinite(InitialRopeSlack) ? FMath::Max(0.0f, InitialRopeSlack) : 10.0f));
		bHasBeenAirborne = Character->GetCharacterMovement()->IsFalling();
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const bool bGrounded = Movement->IsMovingOnGround();
		const FVector BeforeVelocity = Movement->Velocity;
		FVector LaunchAssist = FVector::ZeroVector;
		if (AttachedStyle == ESonHatGrappleMovementStyle::SwingAndReel)
		{
			SaveAndOverrideAirControl(Movement);
			LaunchAssist = ApplyAttachLaunchAssist(Movement);
		}
		PeakSwingSpeed = Movement->Velocity.Size();
		if (bLogGrappleTelemetry) { UE_LOG(LogSonHatGrapple, Display,
			TEXT("Attach style=%s pawnHitCm=%.1f cameraHitCm=%.1f ropeCm=%.1f minRopeCm=%.1f grounded=%d initialV=%s assist=%s finalV=%s mode=%d"),
			*UEnum::GetValueAsString(AttachedStyle), FVector::Distance(Character->GetActorLocation(), Hit.ImpactPoint), TraceDistance,
			ActiveRopeLength, MinimumSwingRopeLength, bGrounded, *BeforeVelocity.ToCompactString(), *LaunchAssist.ToCompactString(),
			*Movement->Velocity.ToCompactString(), static_cast<int32>(Movement->MovementMode)); }
	}
	bIsGrapplingActive = true;
	TargetState = ESonHatGrappleTargetState::Ready;
	TargetAimPoint = GrappleAnchorWorldPoint;
	PlayFeedbackSound(HookFireSound ? HookFireSound : AttachSound);
	BeginHookShotVisual(GrappleAnchorWorldPoint);
	if (!bShowHookShotTravel && AttachSound) { PlayFeedbackSound(AttachSound); }
	return true;
}

void UGrappleComponent::UpdateAirBoostInput()
{
	APlayerController* PC = InputController.Get();
	const bool bDown = PC && PC->IsInputKeyDown(EKeys::SpaceBar);
	const bool bPressed = PC && PC->WasInputKeyJustPressed(EKeys::SpaceBar);
	const bool bFreshPress = bPressed && !bAirBoostSpaceWasDown;
	// A short down+up can have a Pressed event but no Down state. Consume that pulse once too.
	bAirBoostSpaceWasDown = bDown || bPressed;
	ACharacter* Character = GetOwningCharacter();
	if (Character && bSpaceJumpRequested && !bDown)
	{
		Character->StopJumping(); bSpaceJumpRequested = false;
	}
	if (bFreshPress && Character && bRoundInputEnabled && !bAwaitingFallRetry)
	{
		if (Character->GetCharacterMovement()->IsMovingOnGround())
		{
			Character->Jump(); // Idempotent with IA_Jump. Also handles a short pulse missed by Enhanced Input.
			bSpaceJumpRequested = true;
		}
		else { TryAirBoost(); }
	}
}

bool UGrappleComponent::TryAirBoost()
{
	ACharacter* Character = GetOwningCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!IsAirBoostAvailable() || !Movement || !Movement->IsFalling() || !Movement->UpdatedComponent
		|| Movement->Velocity.ContainsNaN()) { return false; }
	const FVector Before = Movement->Velocity;
	const float Up = FMath::Clamp(GrappleSetting(AirBoostUpSpeed, 900.f), 0.f, 1200.f);
	const float Forward = FMath::Clamp(GrappleSetting(AirBoostForwardSpeed, 250.f), 0.f, 600.f);
	if (Up <= 0.f && Forward <= 0.f) { return false; }
	// Camera yaw supplies a small additive steer; looking down cannot turn the lift downward.
	FVector Velocity = Before + FRotator(0.f, Character->GetControlRotation().Yaw, 0.f).Vector() * Forward;
	Velocity.Z = Up > 0.f ? FMath::Max(Before.Z, FMath::Min(1600.0, FMath::Max(Before.Z + Up, static_cast<double>(Up)))) : Before.Z;
	// Same speed envelope as the existing swing. CMC collision and the existing rope solver still apply.
	Movement->Velocity = Velocity.GetClampedToMaxSize(GrappleSetting(SwingSpeedLimit, 2600.f, 100.f));
	Movement->UpdateComponentVelocity();
	bAirBoostUsed = true;
	AirBoostFeedbackRemaining = 0.8f;
	if (bLogGrappleTelemetry)
	{
		UE_LOG(LogSonHatGrapple, Display, TEXT("AirBoost tethered=%d before=%s after=%s"), IsGrappling(), *Before.ToCompactString(), *Movement->Velocity.ToCompactString());
	}
	return true;
}

void UGrappleComponent::StopGrapple()
{
	if (!bIsGrapplingActive) { return; } // Native + Blueprint release may arrive in the same frame.
	if (bIsGrapplingActive)
	{
		if (ACharacter* Character = GetOwningCharacter())
		{
			if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				FVector Velocity = MoveComp->Velocity;
				if (AttachedStyle == ESonHatGrappleMovementStyle::SwingAndReel)
				{
					if (FMath::IsFinite(SwingReleaseAssist) && SwingReleaseAssist > 0 && !Velocity.IsNearlyZero())
					{
						Velocity = (Velocity + Velocity.GetSafeNormal() * SwingReleaseAssist).GetClampedToMaxSize(SwingSpeedLimit);
					}
				}
				else if (ReleaseLaunchBoost > 0.f)
				{
					const FVector LaunchDir = Character->GetControlRotation().Vector().GetSafeNormal();
					Velocity += LaunchDir * ReleaseLaunchBoost;
					const float Speed = Velocity.Size();
					if (Speed > ReleaseLaunchMaxSpeed)
					{
						Velocity *= ReleaseLaunchMaxSpeed / Speed;
					}
				}
				MoveComp->Velocity = Velocity;
			}
		}
		PlayFeedbackSound(ReleaseSound);
	}
	ClearGrappleState(TEXT("Released"));
}

bool UGrappleComponent::IsSwingTargetTooClose(const FVector& HitPoint) const
{
	const ACharacter* Character = GetOwningCharacter();
	const float MinimumDistance = FMath::Max(GrappleSetting(MinimumSwingAttachDistance, 500, 100),
		GrappleSetting(MinimumSwingRopeLength, 350, 100) + 50);
	return !Character || HitPoint.ContainsNaN() || FVector::DistSquared(Character->GetActorLocation(), HitPoint) < FMath::Square(MinimumDistance);
}

FString UGrappleComponent::GetTargetFeedback() const
{
	if (IsGrappling()) { return FString(); }
	switch (TargetState)
	{
	case ESonHatGrappleTargetState::Ready: return TEXT("Grapple here (RMB)");
	case ESonHatGrappleTargetState::Blocked: return TEXT("Cannot grapple here (RMB)");
	case ESonHatGrappleTargetState::TooClose: return TEXT("Too close — aim farther");
	case ESonHatGrappleTargetState::NoHit: return FString();
	default: return FString();
	}
}

FVector UGrappleComponent::ApplyAttachLaunchAssist(UCharacterMovementComponent* Movement)
{
	if (!bEnableAttachLaunchAssist || !Movement) { return FVector::ZeroVector; }
	const FVector Before = Movement->Velocity;
	if (!Movement->IsMovingOnGround() && (!Movement->IsFalling() || Before.Size() >= GrappleSetting(AirborneAssistSpeedThreshold, 350)))
	{
		return FVector::ZeroVector;
	}
	const ACharacter* Character = GetOwningCharacter();
	const FVector D = (GrappleAnchorWorldPoint - Character->GetActorLocation()).GetSafeNormal();
	// Use movement intent when supplied, otherwise camera YAW (looking up must not eliminate forward takeoff).
	FVector Forward = Character->GetPendingMovementInputVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero()) { Forward = FRotator(0, Character->GetControlRotation().Yaw, 0).Vector(); }
	FVector Tangent = FVector::VectorPlaneProject(Forward, D).GetSafeNormal();
	if (Tangent.IsNearlyZero()) { Tangent = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal(); }
	FVector Velocity = Before + Tangent * FMath::Max(0.0, static_cast<double>(GrappleSetting(AttachTangentSpeed, 1400)) - FVector::DotProduct(Before, Tangent));
	// Modest motor/lift floors handle shallow anchors and clear the roof. No teleport or release impulse.
	Velocity += Forward * FMath::Max(0.0, static_cast<double>(GrappleSetting(AttachForwardSpeed, 900)) - FVector::DotProduct(Velocity, Forward));
	Velocity.Z = FMath::Max(Velocity.Z, static_cast<double>(GrappleSetting(AttachUpSpeed, 850)));
	Movement->SetMovementMode(MOVE_Falling); // Walking would discard lift / apply ground braking.
	Movement->Velocity = Velocity.GetClampedToMaxSize(GrappleSetting(SwingSpeedLimit, 2600, 100));
	Movement->UpdateComponentVelocity();
	return Movement->Velocity - Before;
}

void UGrappleComponent::UpdateGrappleTelemetry()
{
	if (!bIsGrapplingActive) { return; }
	if (const ACharacter* Character = GetOwningCharacter()) { PeakSwingSpeed = FMath::Max(PeakSwingSpeed, static_cast<float>(Character->GetCharacterMovement()->Velocity.Size())); }
	const bool bReel = IsReeling();
	if (bReel != bLastReelInput)
	{
		UpdateReelLoopAudio(bReel);
		if (bLogGrappleTelemetry)
		{
			UE_LOG(LogSonHatGrapple, Display, TEXT("Reel input=%d ropeCm=%.1f peakCmPerSec=%.1f"), bReel, ActiveRopeLength, PeakSwingSpeed);
		}
	}
	bLastReelInput = bReel;
}

bool UGrappleComponent::IsGrappling() const
{
	return bIsGrapplingActive && IsValid(GrappleAnchorActor);
}

ACharacter* UGrappleComponent::GetOwningCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

void UGrappleComponent::RefreshInputController()
{
	ACharacter* Character = GetOwningCharacter();
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (PC && !PC->IsLocalController()) { PC = nullptr; }
	if (InputController.Get() == PC) { return; }
	if (InputController.IsValid()) { RemoveTickPrerequisiteActor(InputController.Get()); }
	if (Reticle) { Reticle->RemoveFromParent(); Reticle = nullptr; }
	InputController = PC;
	if (PC)
	{
		AddTickPrerequisiteActor(PC);
		ConfigureFirstPersonOwnerVisibility(); // Possession can happen after component BeginPlay.
	}
}

bool UGrappleComponent::IsReeling() const
{
	return IsGrappling() && AttachedStyle == ESonHatGrappleMovementStyle::SwingAndReel
		&& (bManualReelRequested || (bReadReelInputFromController && InputController.IsValid()
			&& InputController->IsInputKeyDown(ReelInputKey)));
}

bool UGrappleComponent::RefreshAnchorPoint(float DeltaSeconds)
{
	if (!IsValid(GrappleAnchorActor) || (bAnchorUsesComponent && !AnchorComponent.IsValid())) { ClearGrappleState(TEXT("AnchorLost")); return false; }
	const FVector NewPoint = bAnchorUsesComponent ? AnchorComponent->GetComponentTransform().TransformPosition(AnchorLocalPoint)
		: GrappleAnchorActor->GetActorTransform().TransformPosition(AnchorLocalPoint);
	// A teleporting/destroyed anchor should release, never drag the capsule through a building.
	if (NewPoint.ContainsNaN() || FVector::DistSquared(NewPoint, GrappleAnchorWorldPoint) > FMath::Square(600.0)) { ClearGrappleState(TEXT("AnchorTeleported")); return false; }
	GrappleAnchorWorldPoint = NewPoint;
	if (AttachedStyle == ESonHatGrappleMovementStyle::SwingAndReel && bBreakWhenRopeObstructed)
	{
		ACharacter* Character = GetOwningCharacter();
		if (!Character) { ClearGrappleState(TEXT("OwnerLost")); return false; }
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappleObstruction), false, Character);
		// Never ignore a whole building: another facade (or another component on it)
		// can lie between the player and the attached surface after rounding a corner.
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Character->GetActorLocation(), NewPoint, ECC_Visibility, Params);
		const bool bEndpointContact = bHit && !Hit.bStartPenetrating && Hit.GetComponent() == AnchorComponent.Get()
			&& FVector::DistSquared(Hit.ImpactPoint, NewPoint) <= FMath::Square(FMath::Clamp(GrappleSetting(RopeEndpointTolerance, 12.f), 0.f, 30.f));
		const bool bLineBlocked = bHit && !bEndpointContact;
		if (bLineBlocked)
		{
			RopeObstructionElapsed += FMath::Max(0.f, DeltaSeconds);
			const float Grace = GrappleSetting(RopeObstructionGraceSeconds, 0.3f, 0.f);
			if (RopeObstructionElapsed >= Grace)
			{
				ClearGrappleState(TEXT("RopeObstructed"));
				return false;
			}
		}
		else
		{
			RopeObstructionElapsed = 0.f;
		}
	}
	return true;
}

void UGrappleComponent::UpdateLastSafeGround()
{
	ACharacter* Character = GetOwningCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Movement || !Movement->IsMovingOnGround() || !Movement->CurrentFloor.IsWalkableFloor()) { return; }
	LastSafeGroundLocation = Character->GetActorLocation();
	LastSafeGroundRotation = FRotator(0.f, Character->GetControlRotation().Yaw, 0.f);
	bHasLastSafeGround = true;
}

void UGrappleComponent::ResetFallRecoveryCheckpoint()
{
	if (bAwaitingFallRetry)
	{
		if (ACharacter* Character = GetOwningCharacter()) { Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
	}
	bHasLastSafeGround = false;
	bAwaitingFallRetry = false;
}

void UGrappleComponent::SetRoundInputEnabled(bool bEnabled)
{
	bRoundInputEnabled = bEnabled;
	if (!bEnabled) { ClearGrappleState(TEXT("RoundEnded")); }
	UpdatePresentation();
}

void UGrappleComponent::UpdateFallRecovery(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (!bEnableFallRecovery || bAwaitingFallRetry) { return; }
	ACharacter* Character = GetOwningCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Movement || !bHasLastSafeGround) { return; }
	const float RecoveryZ = bUseAbsoluteFallRecoveryZ ? FallRecoveryWorldZ
		: LastSafeGroundLocation.Z - GrappleSetting(FallRecoveryDropDistance, 1500.f, 300.f);
	if (!FMath::IsFinite(RecoveryZ) || Character->GetActorLocation().Z >= RecoveryZ || !Movement->IsFalling()) { return; }
	if (IsGrappling()) { StopGrapple(); }
	if (bRequireFallRetryAcknowledgment)
	{
		bAwaitingFallRetry = true;
		Movement->StopMovementImmediately();
		Movement->DisableMovement(); // Acknowledgment must not continue falling into KillZ.
		HideHookCosmetics();
		if (TetherBeam) { TetherBeam->SetVisibility(false, true); }
		UpdateReelLoopAudio(false);
		return;
	}
	AcknowledgeFallRetry();
}

bool UGrappleComponent::AcknowledgeFallRetry()
{
	if (!bEnableFallRecovery || (bRequireFallRetryAcknowledgment && !bAwaitingFallRetry)) { return false; }
	ACharacter* Character = GetOwningCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !Movement || !bHasLastSafeGround) { return false; }
	Movement->StopMovementImmediately();
	if (!Character->TeleportTo(LastSafeGroundLocation, LastSafeGroundRotation))
	{
		UE_LOG(LogSonHatGrapple, Warning, TEXT("FallRecovery: checkpoint is obstructed; place a supported spawn and clear roof landing space."));
		ResetFallRecoveryCheckpoint();
		bAwaitingFallRetry = false;
		return false;
	}
	if (Character->GetController()) { Character->GetController()->SetControlRotation(LastSafeGroundRotation); }
	Movement->Velocity = FVector::ZeroVector;
	Movement->SetMovementMode(MOVE_Walking);
	bAwaitingFallRetry = false;
	ClearGrappleState(TEXT("FallRecovery"));
	UE_LOG(LogSonHatGrapple, Display, TEXT("FallRecovery: returned to last safe ground at Z=%.1f"), LastSafeGroundLocation.Z);
	return true;
}

void UGrappleComponent::SaveAndOverrideAirControl(UCharacterMovementComponent* Movement)
{
	if (!Movement) { return; }
	if (OverriddenMovement.Get() != Movement)
	{
		RestoreAirControl();
		OverriddenMovement = Movement;
		SavedAirControl = Movement->AirControl;
		SavedAirControlBoost = Movement->AirControlBoostMultiplier;
		SavedFallingFriction = Movement->FallingLateralFriction;
		SavedFallingBraking = Movement->BrakingDecelerationFalling;
	}
	bPreservingReleaseMomentum = false;
	// One steering acceleration while attached. Gravity and CMC's swept movement remain active.
	Movement->AirControl = 0;
	Movement->AirControlBoostMultiplier = 0;
	Movement->FallingLateralFriction = 0;
	Movement->BrakingDecelerationFalling = 0;
}

void UGrappleComponent::RestoreAirControl(bool bPreserveReleaseMomentum)
{
	if (UCharacterMovementComponent* Movement = OverriddenMovement.Get())
	{
		Movement->AirControl = SavedAirControl;
		Movement->AirControlBoostMultiplier = SavedAirControlBoost;
		// The real BP pawn brakes falling motion at 1500 cm/s². Do not erase the
		// earned release trajectory. Restore its drag on landing; steering returns now.
		if (bPreserveReleaseMomentum && Movement->IsFalling())
		{
			bPreservingReleaseMomentum = true;
			return;
		}
		Movement->FallingLateralFriction = SavedFallingFriction;
		Movement->BrakingDecelerationFalling = SavedFallingBraking;
	}
	OverriddenMovement.Reset();
	bPreservingReleaseMomentum = false;
}

void UGrappleComponent::UpdateReticle()
{
	const ACharacter* Character = GetOwningCharacter();
	APlayerController* PC = InputController.Get();
	if (!bShowTargetingFeedback || !PC || !PC->GetLocalPlayer())
	{
		if (Reticle) { Reticle->RemoveFromParent(); Reticle = nullptr; }
		return;
	}
	if (!Reticle)
	{
		Reticle = CreateWidget<UGrappleReticleWidget>(PC, UGrappleReticleWidget::StaticClass());
		if (Reticle) { Reticle->AddToPlayerScreen(10); }
	}
	if (Reticle)
	{
		const bool bReel = IsReeling();
		const FLinearColor Color = !bRoundInputEnabled || bAwaitingFallRetry ? ColorNoHit : IsGrappling() ? (bReel ? ColorReady : ColorAttachedReticle)
			: (TargetState == ESonHatGrappleTargetState::Ready ? ColorReady
				: (TargetState == ESonHatGrappleTargetState::Blocked || TargetState == ESonHatGrappleTargetState::TooClose ? ColorBlocked : ColorNoHit));
		const FString TargetHint = !bRoundInputEnabled || bAwaitingFallRetry ? FString()
			: IsGrappling() ? (bReel ? TEXT("REELING — release LMB to swing") : TEXT("SWINGING — hold LMB to reel")) : GetTargetFeedback();
		Reticle->SetIndicator(Color, IsGrappling(), bReel, TargetHint);
		const FString Hint = (bRoundInputEnabled && bShowFirstUseControlHint && ControlHintRemaining > 0.f)
			? TEXT("RMB hold: swing  |  LMB: reel  |  Release RMB: momentum  |  E: terminal") : FString();
		Reticle->SetControlHint(Hint);
		if (bRoundInputEnabled && !bAwaitingFallRetry && bEnableAirBoost && Character && Character->GetCharacterMovement()->IsFalling())
		{
			Reticle->SetControlHint(AirBoostFeedbackRemaining > 0 ? TEXT("AIR BOOST | Land to recharge")
				: bAirBoostUsed ? TEXT("SPACE: boost spent | Land to recharge") : TEXT("SPACE: air boost ready | RMB: swing | LMB: reel"));
		}
		Reticle->SetFallOverlay(bAwaitingFallRetry && bRoundInputEnabled
			? TEXT("YOU FELL — R: last rooftop (round time keeps running)") : FString());
	}
}

void UGrappleComponent::ApplyGrappleMovement(float DeltaTime)
{
	if (AttachedStyle == ESonHatGrappleMovementStyle::LegacyPull) { ApplyLegacyGrappleMovement(DeltaTime); return; }
	ACharacter* Character = GetOwningCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || (!Movement->IsFalling() && !Movement->IsMovingOnGround())) { ClearGrappleState(TEXT("UnsupportedMovement")); return; }
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0) { return; }
	const FVector ToAnchor = GrappleAnchorWorldPoint - Character->GetActorLocation();
	if (ToAnchor.ContainsNaN() || ToAnchor.SizeSquared() < 1) { ClearGrappleState(TEXT("ZeroDistance")); return; }
	if (IsReeling() && Movement->IsMovingOnGround() && ToAnchor.GetSafeNormal().Z > 0.05)
	{
		Movement->SetMovementMode(MOVE_Falling);
	}
	SonHatGrapplePhysics::FSettings Settings;
	Settings.MinimumLength = GrappleSetting(MinimumSwingRopeLength, 350, 100);
	Settings.SpeedLimit = GrappleSetting(SwingSpeedLimit, 2600, 100);
	Settings.SteeringAcceleration = GrappleSetting(SwingSteeringAcceleration, 1400);
	Settings.ReelSpeed = GrappleSetting(ReelSpeed, 650);
	Settings.ReelAcceleration = GrappleSetting(ReelAcceleration, 2400);
	Settings.ReelPullSpeed = GrappleSetting(ReelPullSpeed, 1600);
	Settings.ConstraintAcceleration = GrappleSetting(RopeConstraintAcceleration, 12000);
	Settings.RecoverySpeed = GrappleSetting(RopeRecoverySpeed, 1400);
	const FVector BeforeStepVelocity = Movement->Velocity;
	const auto Step = SonHatGrapplePhysics::Step(Movement->Velocity, ToAnchor, FVector(0, 0, Movement->GetGravityZ()),
		Character->GetPendingMovementInputVector(), DeltaTime, ActiveRopeLength, CurrentWinchSpeed, IsReeling(), Settings);
	ActiveRopeLength = Step.RopeLength;
	CurrentWinchSpeed = Step.WinchSpeed;
	Movement->Velocity = Step.Velocity;
	if (bLandingAssistDuringSwing)
	{
		FVector Assisted = Movement->Velocity;
		ApplyLandingAssist(Movement, Assisted);
		Movement->Velocity = Assisted;
	}
	if (bLogGrappleTelemetry && GetWorld()->GetTimeSeconds() >= NextMotionTelemetryTime)
	{
		const FVector D = ToAnchor.GetSafeNormal();
		UE_LOG(LogSonHatGrapple, Display, TEXT("Motion reel=%d distanceCm=%.1f ropeCm=%.1f radialBefore=%.1f radialAfter=%.1f winchCmPerSec=%.1f reelAccel=%.1f tensionAccel=%.1f landingDeltaV=%s"),
			IsReeling(), ToAnchor.Size(), ActiveRopeLength, FVector::DotProduct(BeforeStepVelocity, D), FVector::DotProduct(Movement->Velocity, D),
			CurrentWinchSpeed, Step.ReelAccelerationApplied, Step.ConstraintAccelerationApplied, *(Movement->Velocity - Step.Velocity).ToCompactString());
		NextMotionTelemetryTime = GetWorld()->GetTimeSeconds() + 0.25;
	}
}

void UGrappleComponent::HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	// Recharge only after CMC confirms a supporting walkable floor, not on detach or a mode toggle.
	if (const ACharacter* OwnerCharacter = GetOwningCharacter())
	{
		const UCharacterMovementComponent* OwnerMovement = OwnerCharacter->GetCharacterMovement();
		if (OwnerMovement && OwnerMovement->IsMovingOnGround() && OwnerMovement->CurrentFloor.IsWalkableFloor())
		{
			bAirBoostUsed = false;
			AirBoostFeedbackRemaining = 0.f;
		}
	}
	if (bPreservingReleaseMomentum && OverriddenMovement.IsValid() && !OverriddenMovement->IsFalling()) { RestoreAirControl(); }
	if (!IsGrappling() || AttachedStyle != ESonHatGrappleMovementStyle::SwingAndReel || bHandlingConstraint) { return; }
	TGuardValue<bool> Guard(bHandlingConstraint, true);
	ACharacter* Character = GetOwningCharacter();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !Movement->UpdatedComponent) { ClearGrappleState(TEXT("MovementLost")); return; }
	if (!RefreshAnchorPoint(DeltaSeconds)) { return; }
	if (bDetachOnLanding && bHasBeenAirborne && Movement->IsMovingOnGround()) { ClearGrappleState(TEXT("Landed")); UpdatePresentation(); return; }
	if (Movement->IsFalling()) { bHasBeenAirborne = true; }
	FVector ToAnchor = GrappleAnchorWorldPoint - Character->GetActorLocation();
	double Distance = ToAnchor.Size();
	if (ToAnchor.ContainsNaN() || Distance < 1 || Distance - ActiveRopeLength > GrappleSetting(MaximumRopeStretch, 500, 10))
	{
		ClearGrappleState(TEXT("StretchLimitOrInvalidDistance"));
		return;
	}
	if (Distance > ActiveRopeLength + 5 && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0)
	{
		const double Correction = FMath::Min3(Distance - ActiveRopeLength - 5,
			static_cast<double>(GrappleSetting(RopePositionCorrectionSpeed, 1000) * FMath::Min(DeltaSeconds, 0.05f)),
			static_cast<double>(GrappleSetting(MaxRopeCorrectionPerFrame, 50)));
		FHitResult Hit;
		Movement->SafeMoveUpdatedComponent(ToAnchor / Distance * Correction, Movement->UpdatedComponent->GetComponentQuat(), true, Hit);
		if (Hit.bBlockingHit || Hit.bStartPenetrating) { ClearGrappleState(TEXT("BlockedCorrection")); UpdatePresentation(); return; }
		ToAnchor = GrappleAnchorWorldPoint - Character->GetActorLocation();
		Distance = ToAnchor.Size();
	}
	if (Distance >= ActiveRopeLength - 5)
	{
		Movement->Velocity = SonHatGrapplePhysics::RemoveOutwardVelocity(Movement->Velocity, ToAnchor.GetSafeNormal());
	}
	if (bLandingAssistDuringSwing || bPreservingReleaseMomentum)
	{
		FVector Assisted = Movement->Velocity;
		ApplyLandingAssist(Movement, Assisted);
		Movement->Velocity = Assisted;
	}
	Movement->Velocity = Movement->Velocity.GetClampedToMaxSize(GrappleSetting(SwingSpeedLimit, 2600, 100));
	Movement->UpdateComponentVelocity();
	UpdateGrappleTelemetry();
	UpdatePresentation();
}

bool UGrappleComponent::PerformGrappleTrace(FHitResult& OutHit) const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return false;
	}

	const AController* Controller = Character->GetController();
	if (!Controller)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * GrappleRange;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GrappleTrace), false, Character);
	QueryParams.AddIgnoredActor(Character);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);
}

bool UGrappleComponent::IsValidGrappleSurface(const FHitResult& Hit) const
{
	const AActor* Actor = Hit.GetActor();
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Hit.bBlockingHit || Hit.bStartPenetrating || !IsValid(Actor) || !IsValid(Component)
		|| (!GrappleExcludedTag.IsNone() && (Actor->ActorHasTag(GrappleExcludedTag) || Component->ComponentHasTag(GrappleExcludedTag)))) { return false; }
	if (Actor->ActorHasTag(GrappleAnchorTag)) { return true; }
	if (!bAllowSolidWorldSurfaces || Component->GetCollisionObjectType() != ECC_WorldStatic
		|| Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block || Component->IsSimulatingPhysics()) { return false; }
	const FVector Size = Component->Bounds.BoxExtent;
	const float Minimum = GrappleSetting(MinimumSurfaceHalfExtent, 100.f, 10.f);
	return (Size.X >= Minimum) + (Size.Y >= Minimum) + (Size.Z >= Minimum) >= 2;
}

void UGrappleComponent::ApplyLandingAssist(UCharacterMovementComponent* MoveComp, FVector& InOutVelocity) const
{
	if (!MoveComp || LandingAssistMaxHeight <= 0.f || LandingAssistTraceDepth <= 0.f)
	{
		return;
	}

	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return;
	}

	const FVector Start = Character->GetActorLocation();
	FHitResult FloorHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrappleLandingAssist), false, Character);
	if (!GetWorld()->LineTraceSingleByChannel(FloorHit, Start, Start - FVector(0.f, 0.f, LandingAssistTraceDepth), ECC_Visibility, Params))
	{
		return;
	}

	if (FloorHit.Distance > LandingAssistMaxHeight || FloorHit.Normal.Z < 0.55f)
	{
		return;
	}

	if (InOutVelocity.Z < 0.f)
	{
		InOutVelocity.Z *= FMath::Clamp(LandingAssistFallDamping, 0.f, 1.f);
		if (LandingAssistUpBoost > 0.f)
		{
			InOutVelocity.Z += LandingAssistUpBoost;
		}
	}
}

void UGrappleComponent::ApplyLegacyGrappleMovement(float DeltaTime)
{
	ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		ClearGrappleState();
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		ClearGrappleState();
		return;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	FVector ToAnchor = GrappleAnchorWorldPoint - CharacterLocation;
	const float DistanceToAnchor = ToAnchor.Size();
	if (DistanceToAnchor < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector RadialIn = ToAnchor / DistanceToAnchor;

	FVector Velocity = MoveComp->Velocity;
	const float RadialSpeed = FVector::DotProduct(Velocity, RadialIn);
	FVector TangentialVelocity = Velocity - RadialIn * RadialSpeed;

	float NewRadialSpeed = RadialSpeed + PullAcceleration * DeltaTime;

	if (DistanceToAnchor > RopeLength)
	{
		const float Stretch = DistanceToAnchor - RopeLength;
		NewRadialSpeed += Stretch * (RopeStretchPullAcceleration / RopeLength) * DeltaTime;
	}

	if (DistanceToAnchor >= RopeLength && NewRadialSpeed < 0.f)
	{
		NewRadialSpeed = 0.f;
	}

	FVector NewVelocity = TangentialVelocity + RadialIn * NewRadialSpeed;

	if (AirSteeringAcceleration > 0.f)
	{
		const FVector LocalInput = Character->GetLastMovementInputVector();
		if (!LocalInput.IsNearlyZero())
		{
			FRotator YawOnly(0.f, Character->GetControlRotation().Yaw, 0.f);
			const FVector WorldSteer = YawOnly.RotateVector(LocalInput.GetSafeNormal());
			NewVelocity += WorldSteer * AirSteeringAcceleration * DeltaTime;
		}
	}

	ApplyLandingAssist(MoveComp, NewVelocity);

	const float Speed = NewVelocity.Size();
	if (Speed > MaxGrappleSpeed)
	{
		NewVelocity *= MaxGrappleSpeed / Speed;
	}

	MoveComp->Velocity = NewVelocity;
}

void UGrappleComponent::ClearGrappleState(FName Reason)
{
	if (bIsGrapplingActive)
	{
		LastDetachReason = Reason;
		const ACharacter* Character = GetOwningCharacter();
		const FVector Velocity = Character ? Character->GetCharacterMovement()->Velocity : FVector::ZeroVector;
		PeakSwingSpeed = FMath::Max(PeakSwingSpeed, static_cast<float>(Velocity.Size()));
		if (bLogGrappleTelemetry || bLogDetachReasons) { UE_LOG(LogSonHatGrapple, Display, TEXT("Detach reason=%s velocity=%s direction=%s peakCmPerSec=%.1f ropeCm=%.1f obstructionGraceUsed=%.3f"),
			*Reason.ToString(), *Velocity.ToCompactString(), *Velocity.GetSafeNormal().ToCompactString(), PeakSwingSpeed, ActiveRopeLength, RopeObstructionElapsed); }
	}
	RopeObstructionElapsed = 0.f;
	RestoreAirControl(bIsGrapplingActive && Reason == TEXT("Released")
		&& AttachedStyle == ESonHatGrappleMovementStyle::SwingAndReel);
	bIsGrapplingActive = false;
	GrappleAnchorActor = nullptr;
	GrappleAnchorWorldPoint = FVector::ZeroVector;
	AnchorComponent.Reset();
	ActiveRopeLength = 0;
	CurrentWinchSpeed = 0;
	bManualReelRequested = false;
	bHasBeenAirborne = false;
	HookShotTimeRemaining = 0.f;
	HookImpactFlashRemaining = 0.f;
	bHookImpactSoundPlayed = false;
	if (TetherBeam) { TetherBeam->SetVisibility(false, true); }
	if (HookTravelRope) { HookTravelRope->SetVisibility(false, true); }
	if (HookBolt) { HookBolt->SetVisibility(false, true); }
	UpdateReelLoopAudio(false);
}

