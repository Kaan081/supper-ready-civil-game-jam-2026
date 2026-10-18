#if WITH_DEV_AUTOMATION_TESTS

#include "GrappleComponent.h"
#include "GrappleSwingPhysics.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"

namespace SonHatGrappleTests
{
	struct FGrappleTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APlayerController* PC = nullptr;
		ACharacter* Character = nullptr;
		UGrappleComponent* Grapple = nullptr;
		AActor* Anchor = nullptr;

		FGrappleTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
			PC = World->SpawnActor<APlayerController>();
			PC->SetAsLocalPlayerController();
			PC->PlayerInput = NewObject<UPlayerInput>(PC);
			Character = World->SpawnActor<ACharacter>();
			PC->Possess(Character);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			Grapple = NewObject<UGrappleComponent>(Character);
			Character->AddInstanceComponent(Grapple);
			Grapple->bShowTargetingFeedback = false;
			Grapple->bShowActiveTether = false;
			Grapple->ReleaseLaunchBoost = 0;
			Grapple->RegisterComponent();
			FVector Eye;
			FRotator Rotation;
			PC->GetPlayerViewPoint(Eye, Rotation);
			Anchor = AddBox(Eye + Rotation.Vector() * 1000, FVector(60));
			Anchor->Tags.Add(TEXT("GrappleAnchor"));
		}

		~FGrappleTestWorld()
		{
			World->EndPlay(EEndPlayReason::EndPlayInEditor);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		AActor* AddBox(FVector Position, FVector Extent)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Box);
			Actor->SetRootComponent(Box);
			Box->SetMobility(EComponentMobility::Static);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetWorldLocation(Position);
			Box->RegisterComponent();
			return Actor;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleBaselineTest, "SonHat.Grapple.Baseline.AcquisitionReleaseAndTagContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleBaselineTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	TestTrue(TEXT("Camera trace acquires actor-tagged anchor"), Scene.Grapple->TryStartGrapple());
	TestTrue(TEXT("Attachment active"), Scene.Grapple->IsGrappling());
	const FVector EarnedVelocity(750, 400, 220);
	Scene.Character->GetCharacterMovement()->Velocity = EarnedVelocity;
	Scene.Grapple->StopGrapple();
	TestFalse(TEXT("Release detaches"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Zero-assist release preserves actual velocity"), Scene.Character->GetCharacterMovement()->Velocity, EarnedVelocity);
	Scene.Anchor->Tags.Reset();
	Scene.Anchor->GetRootComponent()->ComponentTags.Add(TEXT("GrappleAnchor"));
	TestFalse(TEXT("Actor tag contract is not replaced by component tags"), Scene.Grapple->TryStartGrapple());
	Scene.Anchor->Tags.Add(TEXT("GrappleAnchor"));
	FVector Eye;
	FRotator Rotation;
	Scene.PC->GetPlayerViewPoint(Eye, Rotation);
	Scene.AddBox(Eye + Rotation.Vector() * 400, FVector(50));
	TestFalse(TEXT("Opaque untagged obstacle blocks acquisition"), Scene.Grapple->TryStartGrapple());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleSwingMathTest, "SonHat.Grapple.Swing.ConstraintReelAndSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleSwingMathTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrapplePhysics;
	FSettings Settings;
	const FVector D(1, 0, 0);
	TestEqual(TEXT("Only outward radial motion is removed"), RemoveOutwardVelocity(FVector(-200, 700, 100), D), FVector(0, 700, 100));
	TestEqual(TEXT("Inward momentum is free"), RemoveOutwardVelocity(FVector(200, 700, 100), D), FVector(200, 700, 100));
	const auto Slack = Step(FVector(0, 700, 0), D * 700, FVector(0, 0, -980), FVector::ZeroVector, 1.f / 60, 1000, 0, false, Settings);
	TestTrue(TEXT("RMB supplies no tractor pull when slack"), Slack.Velocity.Equals(FVector(0, 700, 0), 0.001));
	const auto Taut = Step(FVector(-100, 700, 0), D * 1000, FVector(0, 0, -980), FVector::ZeroVector, 1.f / 60, 1000, 0, false, Settings);
	TestTrue(TEXT("Tension opposes outward motion"), Taut.Velocity.X > -100);
	TestTrue(TEXT("Tension keeps lateral velocity"), FMath::IsNearlyEqual(Taut.Velocity.Y, 700.0, 0.001));
	const auto Reel = Step(FVector(0, 700, 0), D * 1000, FVector(0, 0, -980), FVector::ZeroVector, 1.f / 60, 1000, 0, true, Settings);
	TestTrue(TEXT("Reel shortens rope"), Reel.RopeLength < 1000);
	TestTrue(TEXT("Reel adds inward motion"), Reel.Velocity.X > 0);
	TestTrue(TEXT("Reel preserves lateral momentum"), FMath::IsNearlyEqual(Reel.Velocity.Y, 700.0, 0.001));
	const auto FastInward = Step(FVector(1800, 100, 0), D * 1000, FVector::ZeroVector, FVector::ZeroVector, 1.f / 60, 1000, 0, true, Settings);
	TestTrue(TEXT("Winch never brakes faster earned inward motion"), FastInward.Velocity.X >= 1799);
	const auto Limited = Step(FVector(0, 2600, 0), D * 1000, FVector(0, 0, -980), FVector(0, 1, 0), 0.05f, 1000, 650, true, Settings);
	TestTrue(TEXT("Combined speed respects limit"), Limited.Velocity.Size() <= 2600.01);
	TestTrue(TEXT("Speed budget favors tangent over winch"), FMath::IsNearlyEqual(Limited.Velocity.Y, 2600.0, 0.01));
	const auto Steer = Step(FVector::ZeroVector, D * 700, FVector::ZeroVector, FVector(0, 1, 0), 1.f / 60, 1000, 0, false, Settings);
	TestTrue(TEXT("World-space input can start a swing from rest"), Steer.Velocity.Y > 20 && Steer.Velocity.Y < 24 && Steer.Velocity.X == 0);
	const auto Zero = Step(FVector(1, 2, 3), FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, 0, 0, 0, true, Settings);
	TestFalse(TEXT("Zero distance/time stays finite"), Zero.Velocity.ContainsNaN());
	TestTrue(TEXT("Minimum rope length is enforced"), Zero.RopeLength >= Settings.MinimumLength);
	const auto Hitch = Step(FVector::ZeroVector, D * 1000, FVector::ZeroVector, FVector::ZeroVector, 10, 1000, 0, true, Settings);
	TestTrue(TEXT("Hitch cannot retract hundreds of meters"), 1000 - Hitch.RopeLength <= Settings.ReelSpeed * 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleLifecycleTest, "SonHat.Grapple.Swing.LifecycleAndCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	UCharacterMovementComponent* Movement = Scene.Character->GetCharacterMovement();
	Movement->AirControl = 0.37f;
	Movement->AirControlBoostMultiplier = 1.7f;
	Movement->FallingLateralFriction = 0.12f;
	Movement->BrakingDecelerationFalling = 19;
	const FVector Earned(120, 800, 140);
	Movement->Velocity = Earned;
	TestTrue(TEXT("Swing acquisition works"), Scene.Grapple->TryStartGrapple());
	TestEqual(TEXT("Acquisition preserves velocity"), Movement->Velocity, Earned);
	TestTrue(TEXT("Rope captures actual distance, not fixed legacy radius"), Scene.Grapple->GetActiveRopeLength() > 900 && Scene.Grapple->GetActiveRopeLength() < 1100);
	TestEqual(TEXT("CMC steering disabled only while attached"), Movement->AirControl, 0.f);
	Scene.Grapple->SetReeling(true);
	const float BeforeLength = Scene.Grapple->GetActiveRopeLength();
	Scene.Character->AddMovementInput(FVector(0, 1, 0));
	Scene.Grapple->TickComponent(1.f / 60, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Independent reel can run during steering"), Scene.Grapple->IsReeling());
	TestTrue(TEXT("Component retracts rope"), Scene.Grapple->GetActiveRopeLength() < BeforeLength);
	TestTrue(TEXT("Component keeps lateral motion"), Movement->Velocity.Y >= Earned.Y);
	const FVector BeforeRelease = Movement->Velocity;
	Scene.Grapple->StopGrapple();
	TestEqual(TEXT("Actual hybrid release trajectory is continuous"), Movement->Velocity, BeforeRelease);
	TestEqual(TEXT("Air control restored"), Movement->AirControl, 0.37f);
	TestEqual(TEXT("Air boost restored"), Movement->AirControlBoostMultiplier, 1.7f);
	TestEqual(TEXT("Release holds drag off until landing"), Movement->FallingLateralFriction, 0.f);
	TestEqual(TEXT("Release holds falling brake off until landing"), Movement->BrakingDecelerationFalling, 0.f);

	TestTrue(TEXT("Reacquire for collision test"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->bBreakWhenRopeObstructed = false; // Exercise capsule sweep independently of rope LOS.
	Scene.Character->SetActorLocation(FVector(-150, 0, 0));
	Scene.AddBox(FVector(-95, 0, 0), FVector(5, 200, 200));
	const FVector BeforeCorrection = Scene.Character->GetActorLocation();
	Scene.Character->OnCharacterMovementUpdated.Broadcast(0.05f, BeforeCorrection, Movement->Velocity);
	TestTrue(TEXT("Swept correction cannot cross wall"), Scene.Character->GetActorLocation().X < -130);
	TestTrue(TEXT("Correction remains bounded"), FVector::Distance(BeforeCorrection, Scene.Character->GetActorLocation()) <= 50.01);
	TestFalse(TEXT("Blocked correction safely detaches"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Collision detach restores air control"), Movement->AirControl, 0.37f);
	TestEqual(TEXT("Collision detach restores original friction across reattachment"), Movement->FallingLateralFriction, 0.12f);
	TestEqual(TEXT("Collision detach restores original braking across reattachment"), Movement->BrakingDecelerationFalling, 19.f);
	TestFalse(TEXT("Collision leaves finite velocity"), Movement->Velocity.ContainsNaN());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleFallbackTest, "SonHat.Grapple.Swing.AnchorLossLandingAndLegacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleFallbackTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	auto* Movement = Scene.Character->GetCharacterMovement();
	const float OriginalAirControl = Movement->AirControl;
	TestTrue(TEXT("Attach for landing"), Scene.Grapple->TryStartGrapple());
	Movement->SetMovementMode(MOVE_Walking);
	Scene.Character->OnCharacterMovementUpdated.Broadcast(1.f / 60, Scene.Character->GetActorLocation(), Movement->Velocity);
	TestFalse(TEXT("Landing auto-detaches without boost"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Landing restores air control"), Movement->AirControl, OriginalAirControl);
	Movement->SetMovementMode(MOVE_Falling);
	TestTrue(TEXT("Reattach after landing"), Scene.Grapple->TryStartGrapple());
	Scene.Anchor->Destroy();
	const FVector BeforeLoss(200, 300, -100);
	Movement->Velocity = BeforeLoss;
	Scene.Grapple->TickComponent(1.f / 60, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Destroyed anchor detaches"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Anchor loss preserves momentum"), Movement->Velocity, BeforeLoss);
	TestEqual(TEXT("Anchor loss restores CMC settings"), Movement->AirControl, OriginalAirControl);

	FGrappleTestWorld Legacy;
	Legacy.Grapple->MovementStyle = ESonHatGrappleMovementStyle::LegacyPull;
	TestTrue(TEXT("Legacy acquisition remains available"), Legacy.Grapple->TryStartGrapple());
	Legacy.Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	Legacy.Grapple->TickComponent(1.f / 60, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Legacy retains its original pull"), Legacy.Character->GetCharacterMovement()->Velocity.X > 40);
	Legacy.Grapple->StopGrapple();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleFrameRateTest, "SonHat.Grapple.Swing.CharacterMovementAt30_60_120Hz",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleFrameRateTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	for (int32 Hz : {30, 60, 120})
	{
		FGrappleTestWorld Scene;
		auto* Movement = Scene.Character->GetCharacterMovement();
		Scene.Grapple->bBreakWhenRopeObstructed = false;
		TestTrue(FString::Printf(TEXT("%dHz acquires"), Hz), Scene.Grapple->TryStartGrapple());
		Movement->Velocity = FVector(0, 700, 0);
		const float Dt = 1.f / Hz;
		double MaxStretch = 0;
		double MaxSpeed = 0;
		double LowestZ = Scene.Character->GetActorLocation().Z;
		double Travelled = 0;
		TestTrue(TEXT("Test world has downward gravity"), Movement->GetGravityZ() < -900);
		for (int32 Frame = 0; Frame < Hz * 3; ++Frame)
		{
			const FVector PreviousPosition = Scene.Character->GetActorLocation();
			Scene.Grapple->SetReeling(Frame >= Hz && Frame < Hz * 2);
			Scene.Character->AddMovementInput(FVector(0, 1, 0), 0.2f);
			Scene.Grapple->TickComponent(Dt, LEVELTICK_All, nullptr);
			Movement->TickComponent(Dt, LEVELTICK_All, nullptr);
			// Anchor trace hit is its near face, 60cm before the center along view X.
			const FVector AnchorPoint = Scene.Anchor->GetActorLocation() - FVector(60, 0, 0);
			MaxStretch = FMath::Max(MaxStretch, FVector::Distance(Scene.Character->GetActorLocation(), AnchorPoint) - Scene.Grapple->GetActiveRopeLength());
			MaxSpeed = FMath::Max(MaxSpeed, Movement->Velocity.Size());
			LowestZ = FMath::Min(LowestZ, Scene.Character->GetActorLocation().Z);
			Travelled += FVector::Distance(PreviousPosition, Scene.Character->GetActorLocation());
		}
		TestTrue(FString::Printf(TEXT("%dHz remains attached during swing and reel"), Hz), Scene.Grapple->IsGrappling());
		TestTrue(FString::Printf(TEXT("%dHz rope stretch bounded (%.2fcm)"), Hz, MaxStretch), MaxStretch < 20);
		TestTrue(FString::Printf(TEXT("%dHz speed bounded (%.2fcm/s)"), Hz, MaxSpeed), MaxSpeed <= 2600.1);
		TestFalse(FString::Printf(TEXT("%dHz location finite"), Hz), Scene.Character->GetActorLocation().ContainsNaN());
		AddInfo(FString::Printf(TEXT("%dHz: travelled=%.1fcm, lowestZ=%.1f, final=%s, maxStretch=%.2f, maxSpeed=%.1f"), Hz, Travelled, LowestZ,
			*Scene.Character->GetActorLocation().ToString(), MaxStretch, MaxSpeed));
		TestTrue(FString::Printf(TEXT("%dHz CharacterMovement actually moves"), Hz), Travelled > 500);
		TestTrue(FString::Printf(TEXT("%dHz gravity moves character below anchor"), Hz), LowestZ < Scene.Anchor->GetActorLocation().Z - 100);
		TestTrue(FString::Printf(TEXT("%dHz reel reduces radius"), Hz), Scene.Grapple->GetActiveRopeLength() < 800);
		const FVector BeforeRelease = Movement->Velocity;
		Scene.Grapple->StopGrapple();
		TestEqual(FString::Printf(TEXT("%dHz release continuous"), Hz), Movement->Velocity, BeforeRelease);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleNativeInputTest, "SonHat.Grapple.Input.NativeMouseWithoutMappings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleNativeInputTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	const auto SendKey = [&](FKey Key, EInputEvent Event)
	{
		FInputKeyEventArgs Args{};
		Args.ControllerId = 0;
		Args.Key = Key;
		Args.Event = Event;
		Args.AmountDepressed = Event == IE_Released ? 0.f : 1.f;
		Scene.PC->PlayerInput->InputKey(Args);
		Scene.PC->PlayerInput->ProcessInputStack(TArray<UInputComponent*>(), 1.f / 60, false);
		Scene.Grapple->TickComponent(1.f / 60, LEVELTICK_All, nullptr);
	};
	SendKey(EKeys::RightMouseButton, IE_Pressed);
	TestTrue(TEXT("Native RMB acquires without an Input Mapping Context"), Scene.Grapple->IsGrappling());
	const float InitialLength = Scene.Grapple->GetActiveRopeLength();
	SendKey(EKeys::LeftMouseButton, IE_Pressed);
	TestTrue(TEXT("LMB reels while RMB stays held"), Scene.Grapple->IsReeling());
	TestTrue(TEXT("Raw LMB retracts rope"), Scene.Grapple->GetActiveRopeLength() < InitialLength);
	SendKey(EKeys::LeftMouseButton, IE_Released);
	TestFalse(TEXT("LMB release stops winch"), Scene.Grapple->IsReeling());
	TestTrue(TEXT("LMB release keeps tether"), Scene.Grapple->IsGrappling());
	const FVector Earned(150, 650, 210);
	Scene.Character->GetCharacterMovement()->Velocity = Earned;
	SendKey(EKeys::RightMouseButton, IE_Released);
	TestFalse(TEXT("RMB release detaches"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Native release keeps trajectory"), Scene.Character->GetCharacterMovement()->Velocity, Earned);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleStandingLaunchTest, "SonHat.Grapple.Assist.StandingTakeoffReelAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleStandingLaunchTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	for (int32 Hz : {30, 60, 120})
	{
		FGrappleTestWorld Scene;
		Scene.Anchor->Destroy();
		Scene.Anchor = Scene.AddBox(FVector(300, 0, 1500), FVector(60));
		Scene.Anchor->Tags.Add(TEXT("GrappleAnchor"));
		Scene.AddBox(FVector(0, 0, -106), FVector(600, 600, 10));
		Scene.Character->SetActorLocation(FVector(0, 0, 2));
		auto* Movement = Scene.Character->GetCharacterMovement();
		Movement->SetMovementMode(MOVE_Walking);
		Movement->Velocity = FVector::ZeroVector;
		FVector Eye;
		FRotator View;
		Scene.PC->GetPlayerViewPoint(Eye, View);
		Scene.PC->SetControlRotation((Scene.Anchor->GetActorLocation() - Eye).Rotation());
		Scene.PC->SetViewTarget(Scene.Character);
		Scene.World->TimeSeconds = 1;
		// This isolated controller has no ULocalPlayer to drive a client camera update.
		FMinimalViewInfo POV;
		POV.Location = Eye;
		POV.Rotation = (Scene.Anchor->GetActorLocation() - Eye).Rotation();
		Scene.PC->PlayerCameraManager->FillCameraCache(POV);
		const FVector Start = Scene.Character->GetActorLocation();
		Scene.Grapple->bLogGrappleTelemetry = true;
		if (!TestTrue(TEXT("Standing elevated target acquired"), Scene.Grapple->TryStartGrapple())) { return false; }
		TestTrue(TEXT("Attachment immediately exits Walking"), Movement->IsFalling());
		TestTrue(TEXT("Attach supplies useful lift without jump input"), Movement->Velocity.Z >= 849);
		TestTrue(TEXT("Attach supplies forward travel"), Movement->Velocity.X >= 900);
		const FVector V = Movement->Velocity;
		const FVector D = (Scene.Anchor->GetActorLocation() - Start).GetSafeNormal();
		const double Radial = FVector::DotProduct(V, D);
		TestTrue(TEXT("Elevated course launch is primarily tangent"), (V - D * Radial).Size() > FMath::Abs(Radial));
		for (int32 Frame = 0; Frame < Hz * 0.3; ++Frame)
		{
			Scene.Grapple->TickComponent(1.f / Hz, LEVELTICK_All, nullptr);
			Movement->TickComponent(1.f / Hz, LEVELTICK_All, nullptr);
		}
		TestTrue(FString::Printf(TEXT("%dHz moves >2m within 0.3s"), Hz), FVector::Distance(Start, Scene.Character->GetActorLocation()) > 200);
		TestTrue(TEXT("Roof clearance gained"), Scene.Character->GetActorLocation().Z > Start.Z + 150);
		TestTrue(TEXT("Swing remains attached"), Scene.Grapple->IsGrappling());
		Scene.Grapple->SetReeling(true);
		const float BeforeRope = Scene.Grapple->GetActiveRopeLength();
		Scene.Character->AddMovementInput(FVector(0, 1, 0));
		Scene.Grapple->TickComponent(1.f / Hz, LEVELTICK_All, nullptr);
		TestTrue(TEXT("Independent winch still retracts"), Scene.Grapple->GetActiveRopeLength() < BeforeRope);
		TestTrue(TEXT("WASD still adds lateral control"), Movement->Velocity.Y > 0);
		const FVector BeforeRelease = Movement->Velocity;
		Scene.Grapple->StopGrapple();
		TestEqual(TEXT("Release uses actual assisted swing velocity"), Movement->Velocity, BeforeRelease);
		TestEqual(TEXT("Release reason available without logging"), Scene.Grapple->LastDetachReason, FName(TEXT("Released")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleCloseTargetTest, "SonHat.Grapple.Assist.CloseTargetsAndLegacyFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleCloseTargetTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	Scene.Anchor->Destroy();
	FVector Eye;
	FRotator View;
	Scene.PC->GetPlayerViewPoint(Eye, View);
	Scene.Anchor = Scene.AddBox(Eye + View.Vector() * 180, FVector(30));
	Scene.Anchor->Tags.Add(TEXT("GrappleAnchor"));
	const FVector BeforeVelocity = Scene.Character->GetCharacterMovement()->Velocity;
	Scene.Grapple->TickComponent(1.f / 60, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Close actor tag does not show ready"), Scene.Grapple->GetTargetState(), ESonHatGrappleTargetState::TooClose);
	TestFalse(TEXT("Close target rejected rather than creating unusable slack"), Scene.Grapple->TryStartGrapple());
	TestTrue(TEXT("Close rejection has explicit player feedback"), Scene.Grapple->GetTargetFeedback().Contains(TEXT("TOO CLOSE")));
	TestEqual(TEXT("Rejected target does not alter velocity"), Scene.Character->GetCharacterMovement()->Velocity, BeforeVelocity);
	Scene.Grapple->MovementStyle = ESonHatGrappleMovementStyle::LegacyPull;
	TestTrue(TEXT("Legacy preserves prior close-range acquisition"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->StopGrapple();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleReleaseBrakingTest, "SonHat.Grapple.Swing.ReleaseWithBlueprintFallingBrake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleReleaseBrakingTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	auto* Movement = Scene.Character->GetCharacterMovement();
	Movement->BrakingDecelerationFalling = 1500; // Actual BP_GrappleCharacter PIE setting.
	Movement->FallingLateralFriction = 0.12f;
	Movement->AirControl = 0.37f;
	Movement->Velocity = FVector(1000, 0, 200);
	if (!TestTrue(TEXT("Attach succeeds"), Scene.Grapple->TryStartGrapple())) { return false; }
	Scene.Grapple->StopGrapple();
	Scene.Grapple->StopGrapple(); // Native input and Blueprint Completed can both release.
	TestEqual(TEXT("Air steering immediately restored"), Movement->AirControl, 0.37f);
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		Scene.Grapple->TickComponent(1.f / 60, LEVELTICK_All, nullptr);
		Movement->TickComponent(1.f / 60, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("Earned horizontal velocity survives subsequent movement ticks"), Movement->Velocity.Size2D() > 999);
	TestEqual(TEXT("Duplicate release cannot restore the airborne brake"), Movement->BrakingDecelerationFalling, 0.f);
	TestTrue(TEXT("Reattach during release flight"), Scene.Grapple->TryStartGrapple());
	TestEqual(TEXT("Reattachment disables CMC steering for tangent solver"), Movement->AirControl, 0.f);
	Scene.Grapple->StopGrapple();
	Movement->SetMovementMode(MOVE_Walking);
	Scene.Character->OnCharacterMovementUpdated.Broadcast(1.f / 60, Scene.Character->GetActorLocation(), Movement->Velocity);
	TestEqual(TEXT("Landing restores original Blueprint braking"), Movement->BrakingDecelerationFalling, 1500.f);
	TestEqual(TEXT("Landing restores original friction"), Movement->FallingLateralFriction, 0.12f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatGrappleObstructionGraceTest, "SonHat.Grapple.Swing.RopeObstructionGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatGrappleObstructionGraceTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	Scene.Grapple->RopeObstructionGraceSeconds = 0.3f;
	Scene.Grapple->bBreakWhenRopeObstructed = true;
	TestTrue(TEXT("Attach for obstruction grace"), Scene.Grapple->TryStartGrapple());
	const FVector Start = Scene.Character->GetActorLocation();
	const FVector End = Scene.Anchor->GetActorLocation();
	Scene.AddBox((Start + End) * 0.5, FVector(12, 220, 220));
	const float Step = 0.05f;
	for (int32 i = 0; i < 5; ++i)
	{
		Scene.Grapple->TickComponent(Step, LEVELTICK_All, nullptr);
		TestTrue(TEXT("Brief occlusion keeps tether alive"), Scene.Grapple->IsGrappling());
	}
	Scene.Grapple->TickComponent(Step, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Grace expiry detaches on sustained obstruction"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Detach reason reported"), Scene.Grapple->LastDetachReason, FName(TEXT("RopeObstructed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatCitySurfaceTest, "SonHat.MapIndependence.Grapple.SurfaceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatCitySurfaceTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	Scene.Anchor->Tags.Reset();
	auto* Surface = Cast<UBoxComponent>(Scene.Anchor->GetRootComponent());
	Surface->SetBoxExtent(FVector(30, 350, 600), true); // Untagged building wall.
	TestTrue(TEXT("City surface grappling enabled by native default"), Scene.Grapple->bAllowSolidWorldSurfaces);
	Scene.Grapple->bAllowSolidWorldSurfaces = false;
	TestFalse(TEXT("Anchor-only rollback still rejects unmarked wall"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->bAllowSolidWorldSurfaces = true;
	TestTrue(TEXT("Ordinary solid building wall accepted by opt-in"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->StopGrapple();
	Surface->SetBoxExtent(FVector(350, 350, 30), true);
	TestTrue(TEXT("Broad solid roof component accepted"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->StopGrapple();
	Surface->SetBoxExtent(FVector(20), true);
	TestFalse(TEXT("Small decorative component excluded"), Scene.Grapple->TryStartGrapple());
	Scene.Anchor->Tags.Add(TEXT("GrappleAnchor"));
	TestTrue(TEXT("Small explicitly tagged anchor still accepted"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->StopGrapple();
	Surface->ComponentTags.Add(TEXT("NoGrapple"));
	TestFalse(TEXT("Water/decor exclusion overrides explicit anchor"), Scene.Grapple->TryStartGrapple());
	Surface->ComponentTags.Reset();
	Scene.Anchor->Tags.Reset();
	Surface->SetBoxExtent(FVector(30, 350, 600), true);
	Surface->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	TestFalse(TEXT("Visibility-only decorative surface excluded"), Scene.Grapple->TryStartGrapple());
	Surface->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Noncolliding flood presentation excluded"), Scene.Grapple->TryStartGrapple());
	Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// A small foreground decoration occludes a valid wall; never see/attach through it.
	const FVector Foreground = (Scene.Character->GetActorLocation() + Scene.Anchor->GetActorLocation()) * 0.5;
	AActor* Decoration = Scene.AddBox(Foreground, FVector(40, 80, 80));
	TestFalse(TEXT("Occluded valid surface cannot be acquired through decoration"), Scene.Grapple->TryStartGrapple());
	Decoration->Destroy();
	Surface->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	TestFalse(TEXT("Nonblocking visibility geometry is not targetable"), Scene.Grapple->TryStartGrapple());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatSameActorObstructionTest, "SonHat.MapIndependence.Grapple.SameActorObstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatSameActorObstructionTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	Scene.Grapple->RopeObstructionGraceSeconds = 0;
	TestTrue(TEXT("Attach before moving around building"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Normal endpoint contact keeps attachment"), Scene.Grapple->IsGrappling());
	// Another facade on the SAME actor must obstruct, regardless of anchor actor tag.
	auto* Facade = NewObject<UBoxComponent>(Scene.Anchor);
	Scene.Anchor->AddInstanceComponent(Facade);
	Facade->SetBoxExtent(FVector(20, 250, 250));
	Facade->SetCollisionProfileName(TEXT("BlockAll"));
	Facade->SetWorldLocation((Scene.Character->GetActorLocation() + Scene.Anchor->GetActorLocation()) * 0.5);
	Facade->RegisterComponent();
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Second component of attachment building blocks rope"), Scene.Grapple->IsGrappling());
	Facade->DestroyComponent();
	TestTrue(TEXT("Reattach after removing facade"), Scene.Grapple->TryStartGrapple());
	Scene.Character->SetActorLocation(Scene.Anchor->GetActorLocation() + FVector(200, 0, 0));
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Far side of SAME component blocks rope to near-side attach point"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Clear reason"), Scene.Grapple->LastDetachReason, FName(TEXT("RopeObstructed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatRelativeRecoveryTest, "SonHat.MapIndependence.Grapple.RelativeRecoveryBelowWorldZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatRelativeRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	Scene.AddBox(FVector(0, 0, -5000), FVector(500, 500, 10));
	const FVector Standing(0, 0, -4990 + Scene.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2);
	auto* Movement = Scene.Character->GetCharacterMovement();
	Scene.Character->SetActorLocation(Standing);
	Movement->SetMovementMode(MOVE_Walking);
	Movement->FindFloor(Standing, Movement->CurrentFloor, false);
	TestTrue(TEXT("Real supporting roof below zero"), Movement->CurrentFloor.IsWalkableFloor());
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Standing below old absolute threshold does not recover"), Scene.Character->GetActorLocation(), Standing);
	Movement->SetMovementMode(MOVE_Falling);
	Scene.Character->SetActorLocation(Standing - FVector(0, 0, 1600));
	Movement->Velocity = FVector(200, 100, -1000);
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Relative drop waits for retry acknowledgment"), Scene.Grapple->IsAwaitingFallRetry());
	TestEqual(TEXT("Pending retry holds CMC safely, not falling toward KillZ"), static_cast<EMovementMode>(Movement->MovementMode), MOVE_None);
	const FVector HeldPosition = Scene.Character->GetActorLocation();
	Movement->TickComponent(0.25f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Pending acknowledgment cannot drift or fall"), Scene.Character->GetActorLocation(), HeldPosition);
	TestFalse(TEXT("RMB cannot bypass fall acknowledgment"), Scene.Grapple->TryStartGrapple());
	TestTrue(TEXT("Acknowledged retry returns to supported roof"), Scene.Grapple->AcknowledgeFallRetry());
	TestTrue(TEXT("Relative drop recovers to supported roof"), Scene.Character->GetActorLocation().Equals(Standing, 5));
	TestTrue(TEXT("Recovery clears velocity"), Movement->Velocity.IsNearlyZero());
	Scene.Grapple->ResetFallRecoveryCheckpoint();
	Movement->SetMovementMode(MOVE_Falling);
	const FVector Teleported(10000, 0, -10000);
	Scene.Character->SetActorLocation(Teleported);
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Replay reset cannot return to a stale roof"), Scene.Character->GetActorLocation(), Teleported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatRMBLMBSeparationTest, "SonHat.Grapple.Feel.FixedRMBAndIntentionalLMB",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatRMBLMBSeparationTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrapplePhysics;
	FSettings Assisted;
	FSettings Original = Assisted;
	Original.ReelPullSpeed = 0;
	for (int32 Hz : {30, 60, 120})
	{
		const float Dt = 1.f / Hz;
		for (double Distance : {700.0, 1000.0, 1100.0})
		{
			const FVector ToAnchor(Distance, 0, 0);
			const FVector Velocity(900, 500, 120);
			const auto A = Step(Velocity, ToAnchor, FVector(0, 0, -980), FVector(0, 1, 0), Dt, 1000, 650, false, Original);
			const auto B = Step(Velocity, ToAnchor, FVector(0, 0, -980), FVector(0, 1, 0), Dt, 1000, 650, false, Assisted);
			TestEqual(TEXT("RMB trajectory unchanged by new LMB setting, slack or taut"), B.Velocity, A.Velocity);
			TestEqual(TEXT("RMB retains authoritative rope length even after a prior reel"), B.RopeLength, 1000.f);
			TestEqual(TEXT("RMB supplies zero winch acceleration"), B.ReelAccelerationApplied, 0.f);
		}
		const FVector Velocity(1000, 400, 100);
		const auto A = Step(Velocity, FVector(1000, 0, 0), FVector::ZeroVector, FVector::ZeroVector, Dt, 1800, 0, true, Original);
		const auto B = Step(Velocity, FVector(1000, 0, 0), FVector::ZeroVector, FVector::ZeroVector, Dt, 1800, 0, true, Assisted);
		TestTrue(TEXT("Old slack winch shortens rope without pulling"), A.RopeLength < 1800 && A.ReelAccelerationApplied == 0);
		TestTrue(TEXT("LMB now adds prompt bounded pull during slack travel"), B.Velocity.X > A.Velocity.X);
		TestTrue(TEXT("Configured acceleration budget respected"), B.ReelAccelerationApplied <= Assisted.ReelAcceleration + 0.1f);
		TestEqual(TEXT("Pull preserves tangential velocity"), B.Velocity.Y, Velocity.Y);
		TestEqual(TEXT("Rope shortening is independent of radial motor"), B.RopeLength, A.RopeLength);
		const auto ReleasedLMB = Step(B.Velocity, FVector(900, 0, 0), FVector::ZeroVector, FVector::ZeroVector, Dt, B.RopeLength, B.WinchSpeed, false, Assisted);
		TestEqual(TEXT("Releasing LMB stops shortening immediately"), ReleasedLMB.RopeLength, B.RopeLength);
		TestEqual(TEXT("Releasing LMB removes motor without braking earned velocity"), ReleasedLMB.Velocity, B.Velocity);
	}
	const auto Close = Step(FVector::ZeroVector, FVector(300, 0, 0), FVector::ZeroVector, FVector::ZeroVector, 1.f / 60, 350, 0, true, Assisted);
	TestEqual(TEXT("No reel motor inside minimum radius"), Close.ReelAccelerationApplied, 0.f);
	const auto Fast = Step(FVector(2000, 100, 0), FVector(1000, 0, 0), FVector::ZeroVector, FVector::ZeroVector, 1.f / 60, 1800, 0, true, Assisted);
	TestEqual(TEXT("Motor does not brake faster earned inward momentum"), Fast.Velocity.X, 2000.0);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatPresentationLifecycleTest, "SonHat.ReleaseGate.Grapple.PresentationAndRoundCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatPresentationLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	Scene.Grapple->bShowActiveTether = true;
	const auto VisibleSplines = [&Scene]()
	{
		TArray<USplineMeshComponent*> Components; Scene.Character->GetComponents(Components);
		int32 Count = 0; for (auto* C : Components) { Count += C->IsVisible() && !C->bHiddenInGame; } return Count;
	};
	const auto VisibleHeads = [&Scene]()
	{
		TArray<UStaticMeshComponent*> Components; Scene.Character->GetComponents(Components);
		int32 Count = 0; for (auto* C : Components) { Count += !C->IsA<USplineMeshComponent>() && C->IsVisible() && !C->bHiddenInGame; } return Count;
	};
	for (int32 Index = 0; Index < 12; ++Index)
	{
		TestTrue(TEXT("Repeated valid attach"), Scene.Grapple->TryStartGrapple());
		Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("Exactly one attached tether"), VisibleSplines(), 1);
		TestEqual(TEXT("Default travel-off has no hook head"), VisibleHeads(), 0);
		Scene.Grapple->StopGrapple();
		TestEqual(TEXT("Release clears all ropes immediately"), VisibleSplines(), 0);
		TestEqual(TEXT("Release clears hook head"), VisibleHeads(), 0);
	}
	Scene.Grapple->bShowHookShotTravel = true; // Serialized opt-in must still cleanly finish.
	TestTrue(TEXT("Legacy cosmetic opt-in attaches"), Scene.Grapple->TryStartGrapple());
	TestEqual(TEXT("Travel has one rope, not a duplicate tether"), VisibleSplines(), 1);
	Scene.Grapple->TickComponent(0.12f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Travel hands off to one tether"), VisibleSplines(), 1);
	TestEqual(TEXT("Travel head disappears at handoff"), VisibleHeads(), 0);
	const FVector Earned(650, 400, 250);
	Scene.Character->GetCharacterMovement()->Velocity = Earned;
	Scene.Grapple->SetRoundInputEnabled(false);
	TestFalse(TEXT("Result detaches"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Boundary cleanup does not invent release velocity"), Scene.Character->GetCharacterMovement()->Velocity, Earned);
	TestEqual(TEXT("Result leaves no rope"), VisibleSplines(), 0);
	TestEqual(TEXT("Result leaves no hook head"), VisibleHeads(), 0);
	TestFalse(TEXT("Blueprint/native RMB disabled during result"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->SetRoundInputEnabled(true);
	TestTrue(TEXT("Replay restores valid attachment"), Scene.Grapple->TryStartGrapple());
	Scene.Grapple->StopGrapple(); Scene.Anchor->Destroy();
	TestFalse(TEXT("Failed acquisition remains clean"), Scene.Grapple->TryStartGrapple());
	TestEqual(TEXT("Miss cannot leave a rope"), VisibleSplines(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatPossessionVisibilityTest, "SonHat.ReleaseGate.Grapple.LatePossessionHidesFirstPersonMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatPossessionVisibilityTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	Scene.PC->UnPossess();
	auto* Character = Scene.World->SpawnActor<ACharacter>();
	auto* Arms = NewObject<USkeletalMeshComponent>(Character, TEXT("FirstPersonArms"));
	Character->AddInstanceComponent(Arms); Arms->RegisterComponent();
	auto* Grapple = NewObject<UGrappleComponent>(Character); Character->AddInstanceComponent(Grapple);
	Grapple->bShowTargetingFeedback = false; Grapple->RegisterComponent();
	TestFalse(TEXT("Before possession visibility has not been forced"), Arms->bOwnerNoSee);
	Scene.PC->Possess(Character);
	Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Local possession hides arms from owning camera"), Arms->bOwnerNoSee);
	TestTrue(TEXT("Local possession hides main body"), Character->GetMesh()->bOwnerNoSee);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatAirBoostInputTest, "SonHat.Grapple.AirBoost.JumpInputAndOneCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatAirBoostInputTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	auto* Move = Scene.Character->GetCharacterMovement();
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr); // Resolve local input owner.
	const auto Key = [&Scene](EInputEvent Event)
	{
		FInputKeyEventArgs Args{}; Args.Key = EKeys::SpaceBar; Args.Event = Event; Args.AmountDepressed = Event == IE_Released ? 0.f : 1.f;
		Scene.PC->PlayerInput->InputKey(Args);
		Scene.PC->PlayerInput->ProcessInputStack(TArray<UInputComponent*>(), 1.f / 60, false);
	};
	Move->SetMovementMode(MOVE_Walking);
	Key(IE_Pressed); // Native fallback requests the same Jump as IA_Jump without binding/consuming it.
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Ground press leaves native jump requested"), Scene.Character->bPressedJump);
	TestTrue(TEXT("Ground press does not spend aerial charge"), Scene.Grapple->IsAirBoostAvailable());
	TestFalse(TEXT("Direct boost cannot replace grounded jump"), Scene.Grapple->TryAirBoost());
	Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(700, 200, 300);
	Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Holding initial jump does not auto boost after takeoff"), Move->Velocity, FVector(700, 200, 300));
	Key(IE_Released); Scene.Character->StopJumping(); Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	Key(IE_Pressed); Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	const FVector Boosted = Move->Velocity;
	TestTrue(TEXT("Second physical press supplies controlled lift"), Boosted.Z >= 1199 && Boosted.Z <= 1600);
	TestTrue(TEXT("Horizontal momentum is additive, not replaced"), Boosted.X >= 949 && Boosted.Y == 200);
	for (int32 I = 0; I < 10; ++I) { Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr); }
	TestEqual(TEXT("Holding boost does not repeat"), Move->Velocity, Boosted);
	Key(IE_Released); Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	Key(IE_Pressed); Scene.Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Repeated presses cannot create unlimited flight"), Move->Velocity, Boosted);
	Scene.Grapple->StopGrapple(); Scene.Grapple->ResetFallRecoveryCheckpoint();
	TestFalse(TEXT("Detach/checkpoint reset does not recharge midair"), Scene.Grapple->IsAirBoostAvailable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatAirBoostLandingTest, "SonHat.Grapple.AirBoost.LandingSafetyAndDisable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatAirBoostLandingTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	auto* Move = Scene.Character->GetCharacterMovement();
	Move->Velocity = FVector(500, 200, -1200);
	TestTrue(TEXT("Falling freely permits one boost"), Scene.Grapple->TryAirBoost());
	TestEqual(TEXT("Strong lift reverses a bounded fall"), Move->Velocity.Z, 900.0);
	TestEqual(TEXT("Lateral momentum remains"), Move->Velocity.Y, 200.0);
	Move->SetMovementMode(MOVE_Walking); Move->CurrentFloor.Clear();
	Move->TickComponent(.01f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Unsupported mode change does not recharge"), Scene.Grapple->IsAirBoostAvailable());
	Scene.AddBox(FVector(0, 0, -10), FVector(1000, 1000, 10));
	Scene.Character->SetActorLocation(FVector(0, 0, Scene.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 8));
	Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(0, 0, -500);
	Move->TickComponent(.05f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Swept CMC descent confirms a real landing"), Move->IsMovingOnGround() && Move->CurrentFloor.IsWalkableFloor());
	TestTrue(TEXT("Confirmed landing recharges aerial boost"), Scene.Grapple->IsAirBoostAvailable());
	Move->SetMovementMode(MOVE_Falling); Scene.Grapple->bEnableAirBoost = false;
	const FVector Before = Move->Velocity;
	TestFalse(TEXT("Designer opt-out disables boost immediately"), Scene.Grapple->TryAirBoost());
	TestEqual(TEXT("Disabled boost leaves movement untouched"), Move->Velocity, Before);
	Scene.Grapple->bEnableAirBoost = true; Scene.Grapple->SetRoundInputEnabled(false);
	TestFalse(TEXT("Result input lock also rejects boost"), Scene.Grapple->TryAirBoost());
	Scene.Grapple->SetRoundInputEnabled(true);
	Scene.Grapple->AirBoostUpSpeed = 999999; Scene.Grapple->AirBoostForwardSpeed = 999999;
	Move->Velocity = FVector(2400, 100, 1400);
	TestTrue(TEXT("Tuning extremes remain a finite boost"), Scene.Grapple->TryAirBoost());
	TestTrue(TEXT("Safety cap bounds total speed"), !Move->Velocity.ContainsNaN() && Move->Velocity.Size() <= Scene.Grapple->SwingSpeedLimit + .01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatAirBoostTetherTest, "SonHat.Grapple.AirBoost.TetherReelReleaseCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatAirBoostTetherTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	auto* Move = Scene.Character->GetCharacterMovement(); Move->Velocity = FVector(500, 200, 100);
	TestTrue(TEXT("Existing anchor attaches"), Scene.Grapple->TryStartGrapple());
	const float Rope = Scene.Grapple->GetActiveRopeLength();
	TestTrue(TEXT("One air boost works while tethered"), Scene.Grapple->TryAirBoost());
	TestTrue(TEXT("Boost retains tether"), Scene.Grapple->IsGrappling());
	TestEqual(TEXT("Boost does not shorten rope"), Scene.Grapple->GetActiveRopeLength(), Rope);
	TestTrue(TEXT("Boost retains lateral speed"), Move->Velocity.Y >= 199);
	Scene.Grapple->SetReeling(true); Scene.Grapple->TickComponent(.02f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Independent LMB winch still shortens rope"), Scene.Grapple->GetActiveRopeLength() < Rope);
	const FVector Earned = Move->Velocity; Scene.Grapple->StopGrapple();
	TestEqual(TEXT("Release preserves boosted earned velocity"), Move->Velocity, Earned);
	TestFalse(TEXT("Reattach does not restore boost charge"), Scene.Grapple->TryAirBoost());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatAirBoostShortPulseTest, "SonHat.Grapple.AirBoost.ShortSpacePulseCannotDoubleSpend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatAirBoostShortPulseTest::RunTest(const FString& Parameters)
{
	using namespace SonHatGrappleTests;
	FGrappleTestWorld Scene;
	auto* Move = Scene.Character->GetCharacterMovement();
	Scene.Grapple->TickComponent(.01f, LEVELTICK_All, nullptr);
	Move->SetMovementMode(MOVE_Walking);
	FInputKeyEventArgs Key{}; Key.ControllerId = 0; Key.Key = EKeys::SpaceBar;
	Key.Event = IE_Pressed; Key.AmountDepressed = 1; Scene.PC->PlayerInput->InputKey(Key);
	Key.Event = IE_Released; Key.AmountDepressed = 0; Scene.PC->PlayerInput->InputKey(Key);
	Scene.PC->PlayerInput->ProcessInputStack(TArray<UInputComponent*>(), .016f, false);
	TestTrue(TEXT("Short pulse preserves a pressed edge"), Scene.PC->WasInputKeyJustPressed(EKeys::SpaceBar));
	TestFalse(TEXT("Short pulse already released"), Scene.PC->IsInputKeyDown(EKeys::SpaceBar));
	Scene.Grapple->TickComponent(.01f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Short ground press requests normal jump"), Scene.Character->bPressedJump);
	Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(100, 200, 400);
	for (int32 I = 0; I < 5; ++I) { Scene.Grapple->TickComponent(.01f, LEVELTICK_All, nullptr); }
	TestTrue(TEXT("Same tap cannot become an aerial boost after takeoff"), Scene.Grapple->IsAirBoostAvailable());
	TestEqual(TEXT("Short pulse does not change earned aerial velocity"), Move->Velocity, FVector(100, 200, 400));
	return true;
}

#endif

