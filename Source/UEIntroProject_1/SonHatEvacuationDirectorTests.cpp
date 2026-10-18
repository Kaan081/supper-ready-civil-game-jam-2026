#if WITH_DEV_AUTOMATION_TESTS

#include "SonHatEvacuationDirector.h"

#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "GrappleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/TextBlock.h"
#include "SonHatEvacuationHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SonHatPopulationPresenter.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "SonHatEvacuationLink.h"
#include "SonHatEvacuationNode.h"

namespace SonHatEvacuationDirectorTests
{
	struct FLevel
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		APlayerController* PC = nullptr;
		ACharacter* Pawn = nullptr;
		ASonHatEvacuationDirector* Director = nullptr;
		TArray<TObjectPtr<ASonHatEvacuationNode>> Nodes;
		TArray<TObjectPtr<ASonHatEvacuationLink>> Links;

		FLevel(bool bRescue = false)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
			PC = World->SpawnActor<APlayerController>();
			PC->SetAsLocalPlayerController();
			PC->PlayerInput = NewObject<UPlayerInput>(PC);
			Pawn = World->SpawnActor<ACharacter>();
			Pawn->SetActorLocation(FVector(100, 200, 600));
			PC->Possess(Pawn);
			const int32 Population[] = {0, 35, 45, 60, 20};
			const FVector Positions[] = {FVector(0, 0, 2000), FVector(3000, 0, 1200), FVector(6000, 0, 1400), FVector(0, 4000, 1000), FVector(4000, 4000, 800)};
			const TCHAR* Names[] = {TEXT("Hub"), TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D")};
			for (int32 Index = 0; Index < 5; ++Index)
			{
				ASonHatEvacuationNode* Node = World->SpawnActor<ASonHatEvacuationNode>();
				Node->BuildingName = FText::FromString(Names[Index]);
				Node->CivilianCount = Population[Index];
				Node->bIsSafeHub = Index == 0;
				Node->Tags.Add(TEXT("RooftopRound"));
				Node->SetActorLocation(Positions[Index]);
				Nodes.Add(Node);
			}
			const int32 From[] = {0, 1, 0, 3, 1, 0};
			const int32 To[] = {1, 2, 3, 4, 3, 4};
			for (int32 Index = 0; Index < (bRescue ? 4 : 6); ++Index)
			{
				ASonHatEvacuationLink* Link = World->SpawnActor<ASonHatEvacuationLink>();
				Link->NodeA = Nodes[bRescue ? 0 : From[Index]];
				Link->NodeB = Nodes[bRescue ? Index + 1 : To[Index]];
				Link->bFloodSafe = bRescue || Index != 4;
				Link->Tags.Add(TEXT("RooftopRound"));
				Link->SetActorLocation(FVector(Index * 1000, -800, 1296));
				Links.Add(Link);
			}
		}

		~FLevel()
		{
			if (IsValid(Director)) { Director->Destroy(); }
			World->EndPlay(EEndPlayReason::EndPlayInEditor);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		void PlaceDirector(bool bDiscover = false, bool bRescue = false)
		{
			const FTransform Transform(FRotator::ZeroRotator, FVector(0, 0, 500));
			Director = World->SpawnActorDeferred<ASonHatEvacuationDirector>(ASonHatEvacuationDirector::StaticClass(), Transform);
			Director->bAutoDiscoverNetwork = bDiscover;
			Director->NetworkTag = TEXT("RooftopRound");
			Director->Nodes = Nodes;
			Director->CandidateLinks = Links;
			Director->bRescueSignalMode = bRescue;
			if (bRescue)
			{
				for (int32 Index = 0; Index < 4; ++Index)
				{
					FSonHatRescueGroup Group;
					Group.GroupId = FName(*Nodes[Index + 1]->BuildingName.ToString());
					Group.PopulationNode = Nodes[Index + 1]; Group.Beacon = Links[Index];
					Group.FloodClosureElevation = 700.f; // Synthetic fixture, unrelated to the city.
					AActor* Anchor = World->SpawnActor<AActor>();
					USceneComponent* Root = NewObject<USceneComponent>(Anchor); Anchor->AddInstanceComponent(Root); Anchor->SetRootComponent(Root); Root->RegisterComponent();
					Anchor->SetActorLocation(FVector(Index * 1000,1000,700)); Group.PopulationAnchor = Anchor;
					Director->RescueGroups.Add(Group);
				}
			}
			Director->FloodRevealDuration = 2;
			Director->bRequireLineOfSight = false;
			Director->FinishSpawning(Transform);
			if (bRescue && Director->GetHudWidget()) { Director->GetHudWidget()->TakeWidget(); }
			Director->Tick(1.0f / 60.0f);
		}

		void AimAt(const FVector& Target)
		{
			FVector Camera;
			FRotator Rotation;
			PC->GetPlayerViewPoint(Camera, Rotation);
			const FRotator Aim = (Target - Camera).Rotation();
			PC->SetControlRotation(Aim);
			Pawn->SetActorRotation(Aim);
		}

		ESonHatEvacuationActivationResult Select(int32 LinkIndex)
		{
			const FVector Terminal = Links[LinkIndex]->GetActorLocation();
			Pawn->SetActorLocation(Terminal + FVector(-100, 0, 0));
			AimAt(Terminal);
			return Director->Interact();
		}

		void Key(FKey Key, EInputEvent Event)
		{
			PC->PlayerInput->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, Key, Event, FPlatformTime::Cycles64()));
			PC->PlayerInput->ProcessInputStack(TArray<UInputComponent*>(), 1.0f / 60.0f, false);
		}
	};
}

using namespace SonHatEvacuationDirectorTests;
using EDirectorPhase = ESonHatEvacuationRoundPhase;
using EDirectorResult = ESonHatEvacuationActivationResult;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatDirectorRoundTest, "SonHat.Evacuation.Director.TimedRoundRevealReplayAndSafeChoices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatDirectorRoundTest::RunTest(const FString& Parameters)
{
	FLevel Level;
	const FTransform PlayerStart = Level.Pawn->GetActorTransform();
	Level.PlaceDirector();
	ASonHatEvacuationManager* Manager = Level.Director->GetRoundManager();
	if (!TestNotNull(TEXT("Director creates only the round manager"), Manager)) { return false; }
	TestTrue(TEXT("Existing pawn remains possessed and unmoved"), Level.PC->GetPawn() == Level.Pawn && Level.Pawn->GetActorTransform().Equals(PlayerStart));
	TestEqual(TEXT("Preparation phase"), Level.Director->Phase, EDirectorPhase::Preparing);
	TestEqual(TEXT("90 second default"), Manager->GetRemainingTime(), 90.0f);
	TestEqual(TEXT("Configured population"), Manager->TotalPopulation, 160);
	TestEqual(TEXT("Hub has authoritative access"), Manager->HasSafeRoute(Level.Nodes[0]), true);
	for (int32 Index : {0, 1, 2}) { TestEqual(TEXT("All-safe plan accepted"), Level.Select(Index), EDirectorResult::Success); }
	TestEqual(TEXT("Safe chain and separate hub branch"), Manager->EstimatedSafePopulation, 140);
	TestTrue(TEXT("B has a complete route via A"), Manager->HasSafeRoute(Level.Nodes[2]));
	TestFalse(TEXT("D remains disconnected"), Manager->HasSafeRoute(Level.Nodes[4]));
	Level.World->TimeSeconds += 90;
	Level.Director->Tick(1.0f / 60.0f);
	TestEqual(TEXT("Deadline starts flood reveal"), Level.Director->Phase, EDirectorPhase::FloodReveal);
	TestEqual(TEXT("Reveal starts at zero"), Level.Director->GetFloodRevealProgress(), 0.0f);
	TestEqual(TEXT("Final result measures access"), Manager->FinalSafePopulation, 140);
	TestEqual(TEXT("No late interaction"), Level.Select(3), EDirectorResult::PreparationEnded);
	Level.World->TimeSeconds += 2;
	Level.Director->Tick(1.0f / 60.0f);
	TestEqual(TEXT("Reveal completes to results"), Level.Director->Phase, EDirectorPhase::Results);
	const FVector ReplayPosition = Level.Pawn->GetActorLocation();
	Level.Key(EKeys::R, IE_Pressed);
	Level.Director->Tick(1.0f / 60.0f);
	Level.Key(EKeys::R, IE_Released);
	Manager = Level.Director->GetRoundManager();
	TestEqual(TEXT("Raw R replays"), Level.Director->Phase, EDirectorPhase::Preparing);
	TestEqual(TEXT("Replay restores budget"), Manager->GetRemainingConnections(), 3);
	TestEqual(TEXT("Replay preserves current traversal position"), Level.Pawn->GetActorLocation(), ReplayPosition);
	for (ASonHatEvacuationLink* Link : Level.Links) { TestFalse(TEXT("Old manager resets links"), Link->IsActive()); }
	for (int32 Index : {0, 2, 3}) { Level.Select(Index); }
	Manager->FinalizePreparation();
	TestEqual(TEXT("Different all-safe population choice"), Manager->FinalSafePopulation, 115);
	Level.Director->Destroy();
	TestFalse(TEXT("Owned manager cleaned up"), IsValid(Manager));
	for (ASonHatEvacuationNode* Node : Level.Nodes) { TestTrue(TEXT("External buildings survive director cleanup"), IsValid(Node)); }
	for (ASonHatEvacuationLink* Link : Level.Links) { TestTrue(TEXT("External terminals survive director cleanup"), IsValid(Link)); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatDirectorRiskTest, "SonHat.Evacuation.Director.UnsafeChoiceVisibilityAndRawE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatDirectorRiskTest::RunTest(const FString& Parameters)
{
	FLevel Level;
	Level.PlaceDirector();
	// Stand back: a wall around a terminal-centered pawn can contain the camera and the entire ray.
	Level.Pawn->SetActorLocation(Level.Links[0]->GetActorLocation() - FVector(200, 0, 0));
	Level.Director->bRequireLineOfSight = true;
	FVector Camera;
	FRotator Rotation;
	Level.PC->GetPlayerViewPoint(Camera, Rotation);
	AActor* Wall = Level.World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Wall->AddInstanceComponent(Box);
	Box->SetMobility(EComponentMobility::Static);
	Box->SetBoxExtent(FVector(30));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetWorldLocation((Camera + Level.Links[0]->GetActorLocation()) * 0.5);
	Box->RegisterComponent();
	TestFalse(TEXT("Visibility fixture starts outside its blocker"), Box->Bounds.GetBox().IsInsideOrOn(Camera));
	TestNull(TEXT("Wall blocks terminal focus"), Level.Director->GetFocusedLink());
	TestEqual(TEXT("Cannot activate through a rooftop/wall"), Level.Director->Interact(), EDirectorResult::NoCandidateInRange);
	TestEqual(TEXT("Occluded terminal costs no resource"), Level.Director->GetRoundManager()->GetRemainingConnections(), 3);
	Wall->Destroy();
	Level.AimAt(Level.Links[0]->GetActorLocation());
	TestTrue(TEXT("Visible terminal becomes focused"), Level.Director->GetFocusedLink() == Level.Links[0]);
	Level.Key(EKeys::E, IE_Pressed);
	Level.Director->Tick(1.0f / 60.0f);
	Level.Key(EKeys::E, IE_Released);
	TestTrue(TEXT("Raw E activates the highlighted terminal"), Level.Links[0]->IsActive());
	Level.Director->bRequireLineOfSight = false;
	Level.Select(4); // Unsafe A-C.
	Level.Select(3); // Safe C-D, but no safe connection to hub.
	ASonHatEvacuationManager* Manager = Level.Director->GetRoundManager();
	Manager->FinalizePreparation();
	TestEqual(TEXT("Risky choice leaves only A with access"), Manager->FinalSafePopulation, 35);
	TestFalse(TEXT("C-D subgraph has no safe hub access"), Manager->HasSafeRoute(Level.Nodes[3]));
	TestEqual(TEXT("Unsafe resource was consumed"), Manager->GetRemainingConnections(), 0);
	Level.Nodes[1]->CivilianCount = 999;
	TestEqual(TEXT("Access population snapshot stays frozen"), Manager->FinalSafePopulation, 35);
	TestEqual(TEXT("Total snapshot stays frozen"), Manager->TotalPopulation, 160);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatDirectorAimTest, "SonHat.Evacuation.Director.CameraAimTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatDirectorAimTest::RunTest(const FString& Parameters)
{
	FLevel Level;
	Level.PlaceDirector();
	Level.Director->bRequireLineOfSight = false;
	Level.Pawn->SetActorLocation(Level.Links[1]->GetActorLocation() + FVector(-120, 0, 0));
	Level.AimAt(Level.Links[1]->GetActorLocation());
	TestTrue(TEXT("Aimed in-range terminal is focused"), Level.Director->GetFocusedLink() == Level.Links[1].Get());
	Level.PC->SetControlRotation(FRotator(-89.0f, 0.0f, 0.0f));
	TestNull(TEXT("Off-aim terminal is not focused"), Level.Director->GetFocusedLink());
	TestEqual(TEXT("Off-aim E does not spend resources"), Level.Director->Interact(), EDirectorResult::NoCandidateInRange);
	TestEqual(TEXT("Budget unchanged after off-aim attempt"), Level.Director->GetRoundManager()->GetRemainingConnections(), 3);
	Level.AimAt(Level.Links[0]->GetActorLocation());
	FVector Camera;
	FRotator ViewRotation;
	Level.PC->GetPlayerViewPoint(Camera, ViewRotation);
	TestTrue(TEXT("Static aim resolver matches director focus"),
		Level.Director->GetFocusedLink() == ASonHatEvacuationDirector::ResolveFocusedLinkForTests(Level.Links, Level.Pawn->GetActorLocation(),
			Camera, Level.PC->GetControlRotation().Vector(), Level.Director->GetRoundManager()->InteractionDistance,
			Level.Director->TerminalAimHalfAngleDegrees, [](const ASonHatEvacuationLink*) { return true; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatDirectorDiscoveryTest, "SonHat.Evacuation.Director.TagDiscoveryAndForeignOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatDirectorDiscoveryTest::RunTest(const FString& Parameters)
{
	FLevel Level;
	ASonHatEvacuationNode* Unrelated = Level.World->SpawnActor<ASonHatEvacuationNode>();
	Unrelated->CivilianCount = 999;
	Level.PlaceDirector(true);
	ASonHatEvacuationManager* Manager = Level.Director->GetRoundManager();
	if (!TestNotNull(TEXT("Tagged network auto-discovered"), Manager)) { return false; }
	TestEqual(TEXT("Unrelated actors excluded"), Manager->TotalPopulation, 160);
	Level.Director->Destroy();
	ASonHatEvacuationManager* Foreign = Level.World->SpawnActorDeferred<ASonHatEvacuationManager>(ASonHatEvacuationManager::StaticClass(), FTransform::Identity);
	Foreign->bAutoStartPreparation = false;
	Foreign->Nodes = Level.Nodes;
	Foreign->CandidateLinks = Level.Links;
	Foreign->FinishSpawning(FTransform::Identity);
	Foreign->StartPreparation();
	Level.Pawn->SetActorLocation(Level.Links[0]->GetActorLocation());
	Foreign->TryActivateLink(Level.Links[0], Level.Pawn);
	AddExpectedMessagePlain(TEXT("Another manager already owns a level link"), ELogVerbosity::Warning);
	Level.PlaceDirector(true);
	TestEqual(TEXT("Ownership conflict reported"), Level.Director->Phase, EDirectorPhase::ConfigurationError);
	TestTrue(TEXT("Other developer's running network unchanged"), Level.Links[0]->IsActive() && Foreign->OwnsLink(Level.Links[0]));
	TestEqual(TEXT("Other manager budget unchanged"), Foreign->GetRemainingConnections(), 2);
	Foreign->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatDirectorConfigurationTest, "SonHat.Evacuation.Director.InvalidLevelReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatDirectorConfigurationTest::RunTest(const FString& Parameters)
{
	{
		FLevel Level;
		Level.Nodes[0]->bIsSafeHub = false;
		AddExpectedMessagePlain(TEXT("Place nodes, exactly ONE safe hub"), ELogVerbosity::Warning);
		Level.PlaceDirector();
		TestEqual(TEXT("Missing hub fails before starting a timer"), Level.Director->Phase, EDirectorPhase::ConfigurationError);
		TestNull(TEXT("No manager spawned for malformed layout"), Level.Director->GetRoundManager());
	}
	{
		FLevel Level;
		Level.Links[0]->NodeB = nullptr;
		AddExpectedMessagePlain(TEXT("A candidate has missing endpoints"), ELogVerbosity::Warning);
		Level.PlaceDirector();
		TestEqual(TEXT("Missing endpoint reported"), Level.Director->Phase, EDirectorPhase::ConfigurationError);
		TestNull(TEXT("Invalid link cannot claim resources"), Level.Director->GetRoundManager());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatFloodInterpolationTest, "SonHat.Evacuation.Flood.HeightInterpolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatFloodInterpolationTest::RunTest(const FString& Parameters)
{
	using D = ASonHatEvacuationDirector;
	TestEqual(TEXT("Start elevation"), D::CalculateFloodHeight(0, 200, 1800, 1), 200.f);
	TestEqual(TEXT("Final elevation"), D::CalculateFloodHeight(1, 200, 1800, 1), 1800.f);
	TestEqual(TEXT("Linear midpoint"), D::CalculateFloodHeight(0.5f, 200, 1800, 1), 1000.f);
	TestEqual(TEXT("Designer curve changes midpoint"), D::CalculateFloodHeight(0.5f, 200, 1800, 2), 600.f);
	TestEqual(TEXT("Progress clamps before round"), D::CalculateFloodHeight(-1, 200, 1800, 1), 200.f);
	TestEqual(TEXT("Progress clamps after round"), D::CalculateFloodHeight(2, 200, 1800, 1), 1800.f);
	TestEqual(TEXT("Reversed elevations never make a falling flood"), D::CalculateFloodHeight(1, 200, -100, 1), 200.f);
	float Previous = 200;
	for (int32 Step = 1; Step <= 90; ++Step)
	{
		const float Height = D::CalculateFloodHeight(Step / 90.f, 200, 1650, 1.2f);
		TestTrue(TEXT("Default curve rises monotonically"), Height >= Previous);
		Previous = Height;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatFloodTerminalTest, "SonHat.Evacuation.Flood.ThresholdsWarningsGraphAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatFloodTerminalTest::RunTest(const FString& Parameters)
{
	FLevel Level;
	Level.PlaceDirector();
	auto* Director = Level.Director;
	Director->FloodStartElevation = 0;
	Director->FloodFinalElevation = 2000;
	Director->FloodRiseExponent = 1;
	Director->TerminalFloodWarningSeconds = 12;
	for (int32 Index = 0; Index < Level.Links.Num(); ++Index)
	{
		Level.Links[Index]->SetActorLocation(FVector(Index * 1000, -800, Index == 2 ? 500 : Index == 5 ? 2500 : 1000));
	}
	FSonHatTerminalFloodRule Protected;
	Protected.Terminal = Level.Links[2];
	Protected.bVulnerable = false;
	Director->TerminalFloodRules.Add(Protected);
	TestTrue(TEXT("Configured flood round restarts"), Director->RestartRound());
	auto* Manager = Director->GetRoundManager();
	TArray<UStaticMeshComponent*> Meshes;
	Director->GetComponents(Meshes);
	UStaticMeshComponent* Water = nullptr;
	for (auto* Mesh : Meshes) { if (Mesh->GetFName() == TEXT("FloodPlane")) { Water = Mesh; } }
	if (!TestNotNull(TEXT("Persistent runtime water mesh exists"), Water)) { return false; }
	TestTrue(TEXT("Water visible during preparation"), Water->IsVisible());
	TestEqual(TEXT("Water begins at configured Z"), static_cast<float>(Water->GetComponentLocation().Z), 0.f);
	TestEqual(TEXT("Terminal closing ETA"), Director->GetTerminalSecondsUntilFlood(Level.Links[1]), 45.f);
	TestEqual(TEXT("Protected terminal has no deadline"), Director->GetTerminalSecondsUntilFlood(Level.Links[2]), -1.f);
	TestEqual(TEXT("High terminal stays available all preparation"), Director->GetTerminalSecondsUntilFlood(Level.Links[5]), -1.f);
	TestEqual(TEXT("Prepare a low safe route before flooding"), Level.Select(0), EDirectorResult::Success);
	const int32 BudgetBeforeFlood = Manager->GetRemainingConnections();
	const int32 AccessBeforeFlood = Manager->EstimatedSafePopulation;
	Director->GetHudWidget()->TakeWidget(); // Compact presentation intentionally uses HUD warnings, not world signs.
	Level.World->TimeSeconds += 33;
	Director->Tick(1.f / 60);
	TestEqual(TEXT("Twelve-second warning deadline"), Director->GetTerminalSecondsUntilFlood(Level.Links[1]), 12.f);
	TestFalse(TEXT("Warning precedes closure"), Director->IsTerminalSubmerged(Level.Links[1]));
	bool bHasWarningLabel = false;
	Director->GetHudWidget()->WidgetTree->ForEachWidget([&bHasWarningLabel](UWidget* Widget)
	{
		if (const auto* Label = Cast<UTextBlock>(Widget))
		{
			bHasWarningLabel |= Label->GetVisibility() != ESlateVisibility::Collapsed && Label->GetText().ToString().Contains(TEXT("CLOSES IN"));
		}
	});
	TestTrue(TEXT("Visible terminal warning before submersion"), bHasWarningLabel);
	Level.World->TimeSeconds += 12;
	// Deliberately interact before Tick: no one-frame closure loophole.
	TestTrue(TEXT("Exact waterline closes terminal"), Director->IsTerminalSubmerged(Level.Links[1]));
	TestEqual(TEXT("Submerged E rejected by director"), Level.Select(1), EDirectorResult::InvalidLink);
	TestTrue(TEXT("Explicit submerged rejection message"), Director->StatusMessage.Contains(TEXT("SUBMERGED")));
	TestEqual(TEXT("Blocked terminal does not spend manager budget"), Manager->GetRemainingConnections(), BudgetBeforeFlood);
	TestEqual(TEXT("Flood never recalculates access rules"), Manager->EstimatedSafePopulation, AccessBeforeFlood);
	TestTrue(TEXT("Prepared safe connection stays active"), Level.Links[0]->IsActive());
	TestTrue(TEXT("Prepared route still gives A access"), Manager->HasSafeRoute(Level.Nodes[1]));
	Director->Tick(1.f / 60);
	TestEqual(TEXT("Runtime mesh follows current waterline"), static_cast<float>(Water->GetComponentLocation().Z), 1000.f);
	TArray<UStaticMeshComponent*> AfterMeshes;
	Director->GetComponents(AfterMeshes);
	TestEqual(TEXT("No per-tick component spawning"), AfterMeshes.Num(), Meshes.Num());
	TestTrue(TEXT("Original water component reused"), AfterMeshes.Contains(Water));
	Level.World->TimeSeconds += 5;
	Director->Tick(1.f / 60);
	Level.Pawn->SetActorLocation(Level.Links[1]->GetActorLocation() - FVector(200, 0, 0));
	FVector VisibleBeacon = Level.Links[1]->GetActorLocation();
	VisibleBeacon.Z = Director->GetFloodHeight() + 35;
	Level.AimAt(VisibleBeacon);
	TestTrue(TEXT("Aiming at floating closed beacon focuses the submerged terminal"), Director->GetFocusedLink() == Level.Links[1]);
	TestEqual(TEXT("Visible closed beacon gives explicit E rejection"), Director->Interact(), EDirectorResult::InvalidLink);
	TestEqual(TEXT("Protected terminal usable below water"), Level.Select(2), EDirectorResult::Success);
	TestEqual(TEXT("Preserved graph yields A plus C access"), Manager->EstimatedSafePopulation, 95);
	TestFalse(TEXT("Protection changes no authored risk"), Level.Links[4]->bFloodSafe);
	Level.World->TimeSeconds += 40;
	Director->Tick(1.f / 60);
	TestEqual(TEXT("Final evaluation still owned by manager"), Manager->FinalSafePopulation, 95);
	TestEqual(TEXT("Existing reveal phase preserved"), Director->Phase, EDirectorPhase::FloodReveal);
	TestEqual(TEXT("No height jump at natural deadline"), Director->GetFloodHeight(), 2000.f);
	Level.World->TimeSeconds += 2;
	Director->Tick(1.f / 60);
	TestEqual(TEXT("Results have reveal surge"), Director->GetFloodHeight(), 2100.f);
	Level.Key(EKeys::R, IE_Pressed);
	Director->Tick(1.f / 60);
	Level.Key(EKeys::R, IE_Released);
	TestEqual(TEXT("R resets water to start height"), Director->GetFloodHeight(), 0.f);
	TestFalse(TEXT("R resets terminal closure latch"), Director->IsTerminalSubmerged(Level.Links[1]));
	TestEqual(TEXT("R resets manager budget"), Director->GetRoundManager()->GetRemainingConnections(), 3);
	TestEqual(TEXT("R resets safe-route totals"), Director->GetRoundManager()->EstimatedSafePopulation, 0);
	for (ASonHatEvacuationLink* Link : Level.Links) { TestFalse(TEXT("R clears prepared edges through manager cleanup"), Link->IsActive()); }
	TestEqual(TEXT("E works again after R"), Level.Select(1), EDirectorResult::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatFloodEarlyRevealTest, "SonHat.Evacuation.Flood.EarlyRevealContinuityAndHUD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatFloodEarlyRevealTest::RunTest(const FString& Parameters)
{
	FLevel Level;
	Level.PlaceDirector();
	Level.Director->bAllowEarlyFinishForTesting = true;
	Level.World->TimeSeconds += 30;
	const float Before = Level.Director->GetFloodHeight();
	Level.Key(EKeys::F, IE_Pressed);
	Level.Director->Tick(1.f / 60);
	Level.Key(EKeys::F, IE_Released);
	TestEqual(TEXT("F still ends preparation"), Level.Director->Phase, EDirectorPhase::FloodReveal);
	TestEqual(TEXT("Early reveal starts at actual water height"), Level.Director->GetFloodHeight(), Before);
	Level.World->TimeSeconds += 1;
	Level.Director->Tick(1.f / 60);
	TestTrue(TEXT("Reveal continues rising smoothly"), Level.Director->GetFloodHeight() > Before && Level.Director->GetFloodHeight() < 1750);
	USonHatEvacuationHUDWidget* HUD = CreateWidget<USonHatEvacuationHUDWidget>(Level.World, USonHatEvacuationHUDWidget::StaticClass());
	if (TestNotNull(TEXT("Existing native HUD can build without an asset"), HUD))
	{
		HUD->TakeWidget();
		TestNotNull(TEXT("HUD root exists before Slate construction"), HUD->WidgetTree->RootWidget.Get());
		TestEqual(TEXT("HUD does not consume mouse input"), HUD->GetVisibility(), ESlateVisibility::HitTestInvisible);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatSpatialIntegrationTest, "SonHat.MapIndependence.Round.TwoSpatialConfigurations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatSpatialIntegrationTest::RunTest(const FString& Parameters)
{
	for (const FVector Offset : {FVector::ZeroVector, FVector(18000, -13000, -6500)})
	{
		FLevel Level;
		const FTransform Layout(FRotator(0, Offset.IsZero() ? 0 : 73, 0), Offset);
		for (ASonHatEvacuationNode* Node : Level.Nodes) { Node->SetActorLocation(Layout.TransformPosition(Node->GetActorLocation())); }
		for (ASonHatEvacuationLink* Link : Level.Links) { Link->SetActorLocation(Layout.TransformPosition(Link->GetActorLocation())); }
		Level.Pawn->SetActorLocation(Layout.TransformPosition(Level.Pawn->GetActorLocation()));
		Level.PlaceDirector(true);
		Level.Director->FloodStartElevation = Offset.Z;
		Level.Director->FloodFinalElevation = Offset.Z + 2000;
		Level.Director->FloodRiseExponent = 1;
		Level.Director->bReturnPlayerToStartOnReplay = true;
		auto* Spawn = Level.World->SpawnActor<AActor>();
		Spawn->SetRootComponent(NewObject<USceneComponent>(Spawn));
		Spawn->GetRootComponent()->RegisterComponent();
		const FTransform SpawnTransform(FRotator(0, 73, 0), Layout.TransformPosition(FVector(100, 200, 600)));
		Spawn->SetActorTransform(SpawnTransform);
		Level.Director->ReplaySpawnAnchor = Spawn;
		Level.Director->ExpectedNodeCount = 6;
		TestFalse(TEXT("Partial streamed network rejected explicitly"), Level.Director->RestartRound());
		TestTrue(TEXT("Incomplete network has useful error"), Level.Director->StatusMessage.Contains(TEXT("Incomplete network")));
		Level.Director->ExpectedNodeCount = 5;
		Level.Director->ExpectedLinkCount = 6;
		TestTrue(TEXT("Configured spatial layout starts"), Level.Director->RestartRound());
		auto* Presenter = Level.World->SpawnActorDeferred<ASonHatPopulationPresenter>(ASonHatPopulationPresenter::StaticClass(), FTransform::Identity);
		Presenter->Director = Level.Director;
		Presenter->bProjectFeetToFloor = false;
		Presenter->FinishSpawning(FTransform::Identity);
		auto* Manager = Level.Director->GetRoundManager();
		TestEqual(TEXT("160 residents in either layout"), Manager->TotalPopulation, 160);
		Level.Select(4); // Unsafe A-C spends resource but grants no access.
		TestEqual(TEXT("Unsafe choice never turns population safe"), Manager->EstimatedSafePopulation, 0);
		const FVector Terminal = Level.Links[0]->GetActorLocation();
		Level.Pawn->SetActorLocation(Terminal - FVector(150, 0, 0));
		Level.AimAt(Terminal);
		Level.Key(EKeys::E, IE_Pressed);
		Level.Director->Tick(1.f / 60);
		Level.Key(EKeys::E, IE_Released);
		TestTrue(TEXT("Native E activates safe link in either layout"), Level.Links[0]->IsActive());
		TestEqual(TEXT("Safe population follows manager event"), Manager->EstimatedSafePopulation, 35);
		bool bASafe = false;
		for (const auto& S : Presenter->GetBuildingStatuses()) { if (S.Node == Level.Nodes[1]) { bASafe = S.bHasSafeAccess; } }
		TestTrue(TEXT("Presentation updates A immediately"), bASafe);
		Level.World->TimeSeconds += 70;
		Level.Director->Tick(1.f / 60);
		Presenter->RefreshPresentation();
		TestTrue(TEXT("Rising flood closes unused terminal in either layout"), Level.Director->IsTerminalSubmerged(Level.Links[1]));
		Level.Pawn->SetActorLocation(Level.Links[1]->GetActorLocation() - FVector(100, 0, 0));
		FVector ClosedBeacon = Level.Links[1]->GetActorLocation();
		ClosedBeacon.Z = Level.Director->GetFloodHeight() + 35;
		Level.AimAt(ClosedBeacon);
		TestEqual(TEXT("Closed terminal rejects E path"), Level.Director->Interact(), EDirectorResult::InvalidLink);
		Level.World->TimeSeconds += 20;
		Level.Director->Tick(1.f / 60);
		TestEqual(TEXT("Final access total independent of coordinates"), Manager->FinalSafePopulation, 35);
		Level.World->TimeSeconds += 2;
		Level.Director->Tick(1.f / 60);
		TestEqual(TEXT("Results phase"), Level.Director->Phase, EDirectorPhase::Results);
		Level.Pawn->GetCharacterMovement()->Velocity = FVector(800, 400, -300);
		Level.Key(EKeys::R, IE_Pressed);
		Level.Director->Tick(1.f / 60);
		Level.Key(EKeys::R, IE_Released);
		TestEqual(TEXT("Native R resets phase"), Level.Director->Phase, EDirectorPhase::Preparing);
		TestTrue(TEXT("Replay returns to configured spawn"), Level.Pawn->GetActorLocation().Equals(SpawnTransform.GetLocation(), 1));
		TestTrue(TEXT("Replay clears CharacterMovement momentum"), Level.Pawn->GetCharacterMovement()->Velocity.IsNearlyZero());
		TestEqual(TEXT("Replay resets score"), Level.Director->GetRoundManager()->EstimatedSafePopulation, 0);
		TestEqual(TEXT("Replay resets water elevation"), Level.Director->GetFloodHeight(), static_cast<float>(Offset.Z));
		Presenter->Destroy();
	}
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatTerminalGuidanceTest, "SonHat.ReleaseGate.Round.GuidanceUsesAuthoritativeAvailability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatTerminalGuidanceTest::RunTest(const FString& Parameters)
{
	FLevel Level;
	const auto Visual = [&Level](const TCHAR* Name)
	{
		auto* Actor = Level.World->SpawnActor<AActor>(); Actor->Tags.Add(TEXT("GrappleAnchor"));
		auto* Mesh = NewObject<UStaticMeshComponent>(Actor, FName(Name)); Actor->AddInstanceComponent(Mesh); Actor->SetRootComponent(Mesh);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))); Mesh->RegisterComponent();
		return Mesh;
	};
	auto* Roof = Visual(TEXT("CityRoof"));
	auto* Decorative = Visual(TEXT("AnchorMesh"));
	Level.PlaceDirector();
	TestTrue(TEXT("Tagging an actual building must not hide its roof"), Roof->IsVisible());
	TestFalse(TEXT("Known decorative anchor art stays suppressed"), Decorative->IsVisible());
	TArray<UStaticMeshComponent*> PresentationMeshes;
	for (ASonHatEvacuationLink* Link : Level.Links) { TArray<UStaticMeshComponent*> Parts; Link->GetComponents(Parts); PresentationMeshes.Append(Parts); }
	TestEqual(TEXT("Six existing links own exactly five console parts each"), PresentationMeshes.Num(), 30);
	for (auto* Part : PresentationMeshes) { TestEqual(TEXT("Every console part is non-blocking"), Part->GetCollisionEnabled(), ECollisionEnabled::NoCollision); }
	int32 VisibleConsoles = 0;
	for (auto* Mesh : PresentationMeshes)
	{
		if (!Mesh->GetName().StartsWith(TEXT("Terminal_"))) { continue; }
		VisibleConsoles += Mesh->IsVisible();
		TestTrue(TEXT("Terminals have a recognizable console rather than a sphere"), Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName() == TEXT("Cube"));
		TestEqual(TEXT("Presentation cannot block player or grapple traces"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestTrue(TEXT("Console is restrained and human-scale"), Mesh->GetComponentScale().GetMax() <= 1.f);
	}
	TestEqual(TEXT("Every real terminal has a visible discovery marker before focus"), VisibleConsoles, 6);
	const FVector Origin = Level.Pawn->GetActorLocation();
	Level.Links[4]->SetActorLocation(Origin + FVector(10, 0, 0)); // Unsafe is closest.
	Level.Links[0]->SetActorLocation(Origin + FVector(1000, 0, 0));
	Level.Links[2]->SetActorLocation(Origin + FVector(2000, 0, 0));
	Level.Links[5]->SetActorLocation(Origin + FVector(3000, 0, 0));
	TestEqual(TEXT("First objective prefers useful safe route over closer unsafe"), Level.Director->GetSuggestedTerminal(), Level.Links[0].Get());
	TestTrue(TEXT("Discovery works before E interaction range"), FVector::Distance(Origin, Level.Director->GetSuggestedTerminal()->GetActorLocation()) > Level.Director->InteractionDistance);
	TestEqual(TEXT("Normal focused activation still owns all scoring"), Level.Select(0), EDirectorResult::Success);
	Level.Links[1]->SetActorLocation(Level.Pawn->GetActorLocation() + FVector(300, 0, 0));
	TestEqual(TEXT("Next guide uses newly propagated manager access"), Level.Director->GetSuggestedTerminal(), Level.Links[1].Get());
	Level.Links[1]->SetActorLocation(FVector(1200, 0, Level.Director->GetFloodHeight() - 10));
	TestTrue(TEXT("Submerged terminal is not suggested"), Level.Director->GetSuggestedTerminal() != Level.Links[1]);
	TestEqual(TEXT("Second safe plan remains selectable"), Level.Select(2), EDirectorResult::Success);
	TestEqual(TEXT("Third connection spends final resource"), Level.Select(5), EDirectorResult::Success);
	TestNull(TEXT("No misleading E objective after budget spent"), Level.Director->GetSuggestedTerminal());
	Level.Director->GetRoundManager()->FinalizePreparation();
	TestNull(TEXT("No available-terminal markers after round lock"), Level.Director->GetSuggestedTerminal());
	Level.Director->RestartRound();
	int32 ReplayParts=0; for (ASonHatEvacuationLink* Link : Level.Links) { TArray<UStaticMeshComponent*> Parts; Link->GetComponents(Parts); ReplayParts += Parts.Num(); }
	TestEqual(TEXT("Replay replaces all console parts without duplicating them"), ReplayParts, 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatFallRoundGateTest, "SonHat.ReleaseGate.Round.FallClockRawRetryAndResultReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatFallRoundGateTest::RunTest(const FString& Parameters)
{
	FLevel Level;
	auto* Floor = Level.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Floor); Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(600, 600, 10)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->SetWorldLocation(FVector(100, 200, 400)); Box->RegisterComponent();
	const FVector Standing(100, 200, 410 + Level.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2);
	Level.Pawn->SetActorLocation(Standing);
	auto* Movement = Level.Pawn->GetCharacterMovement(); Movement->SetMovementMode(MOVE_Walking); Movement->FindFloor(Standing, Movement->CurrentFloor, false);
	auto* Grapple = NewObject<UGrappleComponent>(Level.Pawn); Level.Pawn->AddInstanceComponent(Grapple); Grapple->bShowTargetingFeedback = false; Grapple->RegisterComponent();
	Level.PlaceDirector(); Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	ASonHatEvacuationManager* OriginalManager = Level.Director->GetRoundManager();
	const auto Fall = [&]()
	{
		Movement->SetMovementMode(MOVE_Falling); Level.Pawn->SetActorLocation(Standing - FVector(0, 0, 1700));
		Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr);
	};
	Fall(); TestTrue(TEXT("Real fall waits for R"), Grapple->IsAwaitingFallRetry());
	const float TimeBefore = OriginalManager->GetRemainingTime(), WaterBefore = Level.Director->GetFloodHeight();
	Level.World->TimeSeconds += 2; Level.Director->Tick(2);
	TestTrue(TEXT("No indefinite scoring/timer pause while awaiting R"), OriginalManager->GetRemainingTime() <= TimeBefore - 1.99f);
	TestTrue(TEXT("Water and clock continue together"), Level.Director->GetFloodHeight() > WaterBefore);
	TestEqual(TEXT("Public interaction cannot bypass retry"), Level.Director->Interact(), EDirectorResult::InvalidInteractor);
	Level.Key(EKeys::R, IE_Pressed); Level.Director->Tick(1.f / 60); Level.Key(EKeys::R, IE_Released);
	TestFalse(TEXT("Raw R acknowledges checkpoint recovery"), Grapple->IsAwaitingFallRetry());
	TestTrue(TEXT("Retry returns to supported rooftop"), Level.Pawn->GetActorLocation().Equals(Standing, 5));
	TestEqual(TEXT("Checkpoint retry keeps the same round and budget"), Level.Director->GetRoundManager(), OriginalManager);
	Grapple->TickComponent(0.01f, LEVELTICK_All, nullptr); Fall();
	OriginalManager->FinalizePreparation(); Level.World->TimeSeconds += 3; Level.Director->Tick(1.f / 60);
	TestEqual(TEXT("Deadline/result can complete while awaiting retry"), Level.Director->Phase, EDirectorPhase::Results);
	Level.Key(EKeys::R, IE_Pressed); Level.Director->Tick(1.f / 60);
	TestTrue(TEXT("Result R starts full replay, not checkpoint-only acknowledgment"), Level.Director->GetRoundManager() != OriginalManager);
	auto* ReplayManager = Level.Director->GetRoundManager(); Level.Director->Tick(1.f / 60);
	TestEqual(TEXT("Same R edge cannot restart twice"), Level.Director->GetRoundManager(), ReplayManager);
	Level.Key(EKeys::R, IE_Released);
	TestFalse(TEXT("Replay clears waiting state"), Grapple->IsAwaitingFallRetry());
	TestEqual(TEXT("Replay restores all connections"), ReplayManager->GetRemainingConnections(), 3);
	TestTrue(TEXT("Position-preserving replay recovers a fallen player before reset"), Level.Pawn->GetActorLocation().Equals(Standing, 5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatPlayerCommunicationTest, "SonHat.PlayerExperience.ObjectiveImpactRiskAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatPlayerCommunicationTest::RunTest(const FString& Parameters)
{
	FLevel Level; Level.PlaceDirector();
	auto* Hud = Level.Director->GetHudWidget(); Hud->TakeWidget(); Level.Director->Tick(.01f);
	const auto Text = [Hud]()
	{
		FString Result;
		Hud->WidgetTree->ForEachWidget([&Result](UWidget* Widget)
		{
			if (auto* Block = Cast<UTextBlock>(Widget); Block && Block->GetVisibility() != ESlateVisibility::Collapsed)
			{ Result += Block->GetText().ToString() + TEXT("\n"); }
		});
		return Result;
	};
	TestTrue(TEXT("Opening explains objective"), Text().Contains(TEXT("CONNECT SAFE ROUTES FOR 160 ROOFTOP RESIDENTS")));
	TestTrue(TEXT("Opening tells first action and controls"), Text().Contains(TEXT("FIRST GREEN CONSOLE")) && Text().Contains(TEXT("Space Jump/Air Boost")));
	TestTrue(TEXT("Starting resources are visible"), Text().Contains(TEXT("01:30")) && Text().Contains(TEXT("3 CONNECTIONS")) && Text().Contains(TEXT("0 / 160")));
	Level.Pawn->SetActorLocation(Level.Links[4]->GetActorLocation() - FVector(150,0,0)); Level.AimAt(Level.Links[4]->GetActorLocation()); Level.Director->Tick(.01f);
	TestTrue(TEXT("Risk meaning appears before activation"), Text().Contains(TEXT("RISK ROUTE | USES 1 CONNECTION, NO NEW SAFE ACCESS")));
	Level.Select(0); Level.Director->Tick(.01f);
	TestTrue(TEXT("Safe activation communicates resident impact"), Text().Contains(TEXT("SAFE ROUTE CONNECTED: ROOF A")) && Text().Contains(TEXT("35 PEOPLE NOW HAVE A ROUTE TO HIGH GROUND")));
	TestFalse(TEXT("Opening yields immediately after first interaction"), Text().Contains(TEXT("CONNECT SAFE ROUTES FOR 160 ROOFTOP RESIDENTS")));
	Level.Select(4); Level.Director->Tick(.01f);
	TestTrue(TEXT("Unsafe cost is distinct from access"), Text().Contains(TEXT("RISK CONNECTION USED")) && Text().Contains(TEXT("NO PEOPLE GAINED A SAFE ROUTE")));
	TestEqual(TEXT("Risk does not change graph access"), Level.Director->GetRoundManager()->EstimatedSafePopulation, 35);
	Level.Select(1); Level.Director->GetRoundManager()->FinalizePreparation();
	Level.Director->Tick(.01f); // Render reveal first; results must also update case-only text changes.
	Level.World->TimeSeconds += 3; Level.Director->Tick(.01f);
	TestTrue(TEXT("Result shows actual route access and lack of access"), Text().Contains(TEXT("80 / 160")) && Text().Contains(TEXT("80 residents")));
	TestTrue(TEXT("Results use consistent uppercase access heading after reveal"), Text().Contains(TEXT("SAFE-ROUTE ACCESS:"),ESearchCase::CaseSensitive));
	TestTrue(TEXT("Result retains replay instruction and gameplay truth"), Text().Contains(TEXT("R: restart")) && Text().Contains(TEXT("SAFE CHOICES CONNECT DIFFERENT ROOFTOPS")));
	Level.Director->RestartRound(); Level.Director->Tick(.01f);
	TestTrue(TEXT("Replay restores opening without stale confirmation"), Text().Contains(TEXT("CONNECT SAFE ROUTES FOR 160 ROOFTOP RESIDENTS")) && !Text().Contains(TEXT("35 PEOPLE NOW HAVE A ROUTE TO HIGH GROUND")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatWaypointAimSpaceTest, "SonHat.PlayerExperience.WaypointCardsPreserveAimAndReadout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatWaypointAimSpaceTest::RunTest(const FString& Parameters)
{
	for (const FVector2D Size : {FVector2D(1280,720), FVector2D(1920,1080)})
	{
		const FBox2D Aim(Size*.5-FVector2D(175,100), Size*.5+FVector2D(175,100));
		for (const FVector2D Point : {Size*.5, Size*FVector2D(.45,.4), Size*FVector2D(.12,.24), Size*FVector2D(.88,.74)})
		{
			const FVector2D CardSize(330,58);
			const FVector2D Card=USonHatEvacuationHUDWidget::CalculateWaypointCardPosition(Point,Size,CardSize,218);
			const FBox2D Rect(Card,Card+CardSize);
			TestFalse(TEXT("Card does not cover crosshair or grapple state text"), Rect.Intersect(Aim));
			TestFalse(TEXT("Card does not cover onboarding stats"), Rect.Intersect(FBox2D(FVector2D::ZeroVector,FVector2D(810,218))));
			TestTrue(TEXT("Card fits the viewport"), Card.X>=0 && Card.Y>=0 && Rect.Max.X<=Size.X && Rect.Max.Y<=Size.Y);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatRoundCueLifecycleTest, "SonHat.PlayerExperience.RoundAudioWiringAndBoundedReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatRoundCueLifecycleTest::RunTest(const FString& Parameters)
{
	FLevel Level; Level.PlaceDirector(); auto* D=Level.Director;
	for (USoundBase* Sound : {D->FloodWarningSound.Get(),D->FloodUrgentSound.Get(),D->LinkActivationSound.Get(),D->LinkActivationUnsafeSound.Get(),
		D->PreparationAmbienceSound.Get(),D->FloodSirenAmbienceSound.Get(),D->FloodRumbleAmbienceSound.Get(),D->TerminalSubmergedSound.Get(),D->RoundResultsSound.Get()})
	{ TestNotNull(TEXT("Owned round sound resolves without a map assignment"),Sound); }
	const auto Count = [D]() { TArray<UAudioComponent*> Audio; D->GetComponents(Audio); return Audio.Num(); };
	const auto HasSound = [D](USoundBase* Sound)
	{
		TArray<UAudioComponent*> Audio; D->GetComponents(Audio);
		return Audio.ContainsByPredicate([Sound](UAudioComponent* C){return C->Sound==Sound;});
	};
	TestEqual(TEXT("Three loops and one alert channel at start"),Count(),4);
	TestTrue(TEXT("Opening warning is routed to an owned component"),HasSound(D->FloodWarningSound));
	Level.Select(0); TestTrue(TEXT("Safe activation selects safe confirmation"),HasSound(D->LinkActivationSound));
	Level.Select(4); TestTrue(TEXT("Risk activation selects distinct unsafe confirmation"),HasSound(D->LinkActivationUnsafeSound));
	TestEqual(TEXT("Events reuse bounded channels"),Count(),5);
	for(int32 I=0;I<8;++I) { D->Interact(); }
	TestEqual(TEXT("Repeated E cannot allocate more cue components"),Count(),5);
	D->GetRoundManager()->FinalizePreparation();
	TestEqual(TEXT("Visible flood reveal retains exactly three storm loops"),Count(),3);
	TestFalse(TEXT("Result cue waits for the actual result screen"),HasSound(D->RoundResultsSound));
	Level.World->TimeSeconds += 3; D->Tick(.01f);
	TestEqual(TEXT("Result stops ambience and old cues"),Count(),1);
	TestTrue(TEXT("Result has its own confirmation"),HasSound(D->RoundResultsSound));
	for(int32 I=0;I<3;++I) { D->RestartRound(); TestEqual(TEXT("Replay recreates exactly three loops and an alert"),Count(),4); }
	return true; // Component routing/lifecycle only. Rendered playback and human listening are separate gates.
}

 // WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatComprehensionIntroTest, "SonHat.PlayerExperience.ExplicitOrientationThenFullChallenge",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatComprehensionIntroTest::RunTest(const FString& Parameters)
{
 FLevel Level; Level.PlaceDirector(); auto* D=Level.Director;
 D->OrientationDuration=12; D->RestartRound();
 const float Water=D->GetFloodHeight();
 Level.World->TimeSeconds+=5; D->Tick(.01f);
 TestEqual(TEXT("Reading the explicit intro does not consume challenge time"),D->GetRoundManager()->GetRemainingTime(),90.f);
 TestEqual(TEXT("Water waits with the explicit intro"),D->GetFloodHeight(),Water);
 TestEqual(TEXT("Bounded intro counts down"),D->GetOrientationSecondsRemaining(),7.f);
 TestEqual(TEXT("Player may connect immediately"),Level.Select(0),EDirectorResult::Success);
 TestEqual(TEXT("First connection starts the challenge early"),D->GetOrientationSecondsRemaining(),0.f);
 Level.World->TimeSeconds+=3; D->Tick(.01f);
 TestEqual(TEXT("Full challenge now advances normally"),D->GetRoundManager()->GetRemainingTime(),87.f);
 D->RestartRound(); Level.World->TimeSeconds+=14; D->Tick(.01f);
 TestEqual(TEXT("Intro expires without input and cannot freeze forever"),D->GetOrientationSecondsRemaining(),0.f);
 TestEqual(TEXT("Only twelve seconds were held"),D->GetRoundManager()->GetRemainingTime(),88.f);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatCityRecoveryBoundsTest, "SonHat.PlayerExperience.LevelBoundsPreserveValidDescentAndRound",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatCityRecoveryBoundsTest::RunTest(const FString& Parameters)
{
 FLevel Level;
 auto* Floor=Level.World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor);
 Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(600,600,10));
 Box->SetCollisionProfileName(TEXT("BlockAll")); Box->SetWorldLocation(FVector(100,200,5700)); Box->RegisterComponent();
 const FVector Standing(100,200,5710+Level.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);
 Level.Pawn->SetActorLocation(Standing); auto* Move=Level.Pawn->GetCharacterMovement();
 Move->SetMovementMode(MOVE_Walking); Move->FindFloor(Standing,Move->CurrentFloor,false);
 auto* G=NewObject<UGrappleComponent>(Level.Pawn); Level.Pawn->AddInstanceComponent(G); G->bShowTargetingFeedback=false; G->RegisterComponent();
 Level.PlaceDirector(); G->TickComponent(.01f,LEVELTICK_All,nullptr);
 Move->SetMovementMode(MOVE_Falling); Level.Pawn->SetActorLocation(Standing-FVector(0,0,1700)); G->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestTrue(TEXT("Reproduces old relative-height fall high above flood"),G->IsAwaitingFallRetry());
 TestTrue(TEXT("Old checkpoint is recoverable"),G->AcknowledgeFallRetry());
 Level.Director->bUseLevelRecoveryBounds=true; Level.Director->LevelRecoveryWorldZ=-900; Level.Director->RestartRound();
 G->TickComponent(.01f,LEVELTICK_All,nullptr);
 Move->SetMovementMode(MOVE_Falling); Level.Pawn->SetActorLocation(FVector(100,200,1200)); G->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestFalse(TEXT("Valid lower city rooftop descent does not trigger fall"),G->IsAwaitingFallRetry());
 auto* Manager=Level.Director->GetRoundManager(); const float Time=Manager->GetRemainingTime();
 Level.Pawn->SetActorLocation(FVector(100,200,-1000)); G->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestTrue(TEXT("Genuine below-city bounds still caught"),G->IsAwaitingFallRetry());
 Level.World->TimeSeconds+=2; Level.Director->Tick(.01f);
 TestFalse(TEXT("Director automatically acknowledges genuine OOB"),G->IsAwaitingFallRetry());
 TestTrue(TEXT("Returns to verified supported rooftop"),Level.Pawn->GetActorLocation().Equals(Standing,5));
 TestEqual(TEXT("Same authoritative round"),Level.Director->GetRoundManager(),Manager);
 TestEqual(TEXT("Connections retained"),Manager->GetRemainingConnections(),3);
 TestTrue(TEXT("Round clock continues through recovery"),Manager->GetRemainingTime()<=Time-1.99f);
 G->TickComponent(.01f,LEVELTICK_All,nullptr); Level.Director->Tick(.01f);
 TestFalse(TEXT("Supported checkpoint does not loop"),G->IsAwaitingFallRetry());
 return true;
}

namespace SonHatRescueTests
{
	FString VisibleHud(ASonHatEvacuationDirector* Director)
	{
		FString Text;
		if (USonHatEvacuationHUDWidget* Hud = Director->GetHudWidget())
		{
			Hud->WidgetTree->ForEachWidget([&Text](UWidget* Widget)
			{
				if (UTextBlock* Block = Cast<UTextBlock>(Widget); Block && Block->GetVisibility() != ESlateVisibility::Collapsed)
				{ Text += Block->GetText().ToString() + TEXT("\n"); }
			});
		}
		return Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatRescueRegistrationTest, "SonHat.Rescue.FourGroupsChargesHudAndMaximum140",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatRescueRegistrationTest::RunTest(const FString& Parameters)
{
	const int32 Populations[] = {35,45,60,20};
	for (int32 Group = 0; Group < 4; ++Group)
	{
		FLevel Level(true); Level.PlaceDirector(false,true); auto* D = Level.Director;
		auto* M = D->GetRoundManager(); if (!TestNotNull(TEXT("Valid rescue network starts"), M)) { return false; }
		TestEqual(TEXT("Rescue clock is 150 seconds"), M->GetRemainingTime(),150.f);
		TestEqual(TEXT("Exactly four authoritative beacons"), M->CandidateLinks.Num(),4);
		TestTrue(TEXT("Opening states the human goal"),SonHatRescueTests::VisibleHud(D).Contains(TEXT("160 PEOPLE STRANDED")));
		TestEqual(TEXT("One group contributes its actual population"), Level.Select(Group),EDirectorResult::Success);
		TestEqual(TEXT("Registered count"), M->EstimatedSafePopulation,Populations[Group]);
		TestEqual(TEXT("Exactly one charge consumed"), M->GetRemainingConnections(),2);
		TestEqual(TEXT("Repeated E cannot register twice"), Level.Select(Group),EDirectorResult::AlreadyActive);
		TestEqual(TEXT("Repeat leaves score unchanged"),M->EstimatedSafePopulation,Populations[Group]);
		TestEqual(TEXT("Repeat leaves budget unchanged"),M->GetRemainingConnections(),2);
		D->Tick(.01f);
		const FString Hud = SonHatRescueTests::VisibleHud(D);
		TestTrue(TEXT("HUD names registration rather than connections"),Hud.Contains(TEXT("REGISTERED FOR RESCUE")) && Hud.Contains(TEXT("SIGNALS LEFT: 2 / 3")));
		TestFalse(TEXT("No legacy risk instructions"),Hud.Contains(TEXT("SAFE-ROUTE")) || Hud.Contains(TEXT("RISK")));
		TestTrue(TEXT("Next objective skips the completed group"),D->GetSuggestedTerminal() && D->GetSuggestedTerminal()!=Level.Links[Group]);
		TArray<UStaticMeshComponent*> Meshes; Level.Links[Group]->GetComponents(Meshes);
		TestEqual(TEXT("One five-part beacon per link"),Meshes.Num(),5);
		for (UStaticMeshComponent* Mesh : Meshes) { TestEqual(TEXT("Beacon cannot block player, E or grapple traces"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision); }
		TArray<USplineMeshComponent*> RouteMeshes; D->GetComponents(RouteMeshes);
		TestEqual(TEXT("No legacy route beam visuals"),RouteMeshes.Num(),0);
	}
	const int32 Expected[] = {125,115,100,140}; // Omit A, B, C, D respectively.
	for (int32 Omitted = 0; Omitted < 4; ++Omitted)
	{
		FLevel Level(true); Level.PlaceDirector(false,true); auto* M = Level.Director->GetRoundManager();
		if (!M) { return false; }
		for (int32 I = 0; I < 4; ++I) { if (I != Omitted) { TestEqual(TEXT("Three distinct groups can register"),Level.Select(I),EDirectorResult::Success); } }
		TestEqual(TEXT("Each prioritization has the expected score"),M->EstimatedSafePopulation,Expected[Omitted]);
		TestEqual(TEXT("Fourth group rejected without charge"),Level.Select(Omitted),EDirectorResult::BudgetExhausted);
		TestTrue(TEXT("No false objective when out of signals"),Level.Director->GetSuggestedTerminal()==nullptr);
		TestEqual(TEXT("Three-charge cap"),M->GetRemainingConnections(),0);
		TestTrue(TEXT("Theoretical maximum is 140"),M->EstimatedSafePopulation<=140);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatRescueFloodReplayTest, "SonHat.Rescue.FloodClosureRawEFinalResultsAndRawRReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatRescueFloodReplayTest::RunTest(const FString& Parameters)
{
	FLevel Level(true); Level.PlaceDirector(false,true); auto* D=Level.Director; auto* M=D->GetRoundManager();
	if (!M) { return false; }
	// Existing native E ownership, without Level BP or Enhanced Input modifications.
	Level.Pawn->SetActorLocation(Level.Links[0]->GetActorLocation()+FVector(-100,0,0)); Level.AimAt(Level.Links[0]->GetActorLocation());
	Level.Key(EKeys::E,IE_Pressed); D->Tick(.01f); Level.Key(EKeys::E,IE_Released);
	TestEqual(TEXT("Native E registers A"),M->EstimatedSafePopulation,35);
	TestTrue(TEXT("Feedback explains who benefited"),D->StatusMessage.Contains(TEXT("ROOF A SIGNAL RECEIVED")) && D->StatusMessage.Contains(TEXT("+35 PEOPLE REGISTERED")));
	const float Closure = D->GetTerminalSecondsUntilFlood(Level.Links[1]);
	TestTrue(TEXT("Closure is based on supplied floor height"),Closure>0 && Closure<150);
	Level.World->TimeSeconds += Closure + .01f; D->Tick(.01f);
	TestTrue(TEXT("Unregistered group is submerged before beacon center"),D->IsTerminalSubmerged(Level.Links[1]) && D->GetFloodHeight()<Level.Links[1]->GetActorLocation().Z);
	TestEqual(TEXT("Flooded beacon rejects E"),Level.Select(1),EDirectorResult::InvalidLink);
	TestEqual(TEXT("No charge consumed by flood rejection"),M->GetRemainingConnections(),2);
	TestEqual(TEXT("Registration is retained"),M->EstimatedSafePopulation,35);
	TestTrue(TEXT("No objective points at unavailable groups"),D->GetSuggestedTerminal()==nullptr);
	D->Tick(.01f);
	TestFalse(TEXT("Flooded prompt offers no E activation"),SonHatRescueTests::VisibleHud(D).Contains(TEXT("E — SEND RESCUE SIGNAL")));
	Level.World->TimeSeconds += 151; D->Tick(.01f);
	TestEqual(TEXT("Deadline freezes registration"),M->FinalSafePopulation,35);
	Level.World->TimeSeconds += 3; D->Tick(.01f);
	TestEqual(TEXT("Flood reveals then results"),D->Phase,EDirectorPhase::Results);
	const FString Results=SonHatRescueTests::VisibleHud(D);
	TestTrue(TEXT("Human result and replay instructions"),Results.Contains(TEXT("RESCUE OPERATION COMPLETE")) && Results.Contains(TEXT("125 PEOPLE NOT REACHED")) && Results.Contains(TEXT("PRESS R")));
	Level.Key(EKeys::R,IE_Pressed); D->Tick(.01f); Level.Key(EKeys::R,IE_Released);
	M=D->GetRoundManager(); TestNotNull(TEXT("Replay has an authority"),M); if (!M) { return false; }
	TestEqual(TEXT("Replay score zero"),M->EstimatedSafePopulation,0);
	TestEqual(TEXT("Replay charges reset"),M->GetRemainingConnections(),3);
	TestEqual(TEXT("Replay time reset"),M->GetRemainingTime(),150.f);
	for (ASonHatEvacuationLink* Link : Level.Links) { TestFalse(TEXT("Replay resets beacon registration"),Link->IsActive()); TestFalse(TEXT("Replay resets water closure"),D->IsTerminalSubmerged(Link)); }
	TestTrue(TEXT("Replay offers a real group"),D->GetSuggestedTerminal()!=nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatRescueTopologyTest, "SonHat.Rescue.RejectLegacyExtraDuplicateAndUnsafeConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatRescueTopologyTest::RunTest(const FString& Parameters)
{
	FLevel Level(true); Level.PlaceDirector(false,true); auto* D=Level.Director;
	D->RescueGroups[1].GroupId=TEXT("A"); TestFalse(TEXT("Duplicate logical identity rejected"),D->RestartRound());
	D->RescueGroups[1].GroupId=TEXT("B"); Level.Links[1]->bFloodSafe=false; TestFalse(TEXT("Unsafe rescue topology rejected"),D->RestartRound());
	Level.Links[1]->bFloodSafe=true; Level.Links[1]->NodeA=Level.Nodes[1]; TestFalse(TEXT("Legacy chain is not a rescue beacon"),D->RestartRound());
	Level.Links[1]->NodeA=Level.Nodes[0]; auto* Extra=Level.World->SpawnActor<ASonHatEvacuationLink>(); Extra->Tags.Add(TEXT("RooftopRound"));
	TestFalse(TEXT("Hidden legacy link rejected even with explicit arrays"),D->RestartRound()); Extra->Destroy();
	AActor* Anchor=D->RescueGroups[0].PopulationAnchor; D->RescueGroups[0].PopulationAnchor=nullptr;
	TestFalse(TEXT("Missing population placement fails closed"),D->RestartRound()); D->RescueGroups[0].PopulationAnchor=Anchor;
	D->RescueGroups[0].FloodClosureElevation=2000; TestFalse(TEXT("Invalid floor elevation is rejected"),D->RestartRound()); D->RescueGroups[0].FloodClosureElevation=700;
	TestTrue(TEXT("Corrected setup can restart"),D->RestartRound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatRescuePopulationTest, "SonHat.Rescue.PopulationRegistrationLabelsAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatRescuePopulationTest::RunTest(const FString& Parameters)
{
	FLevel Level(true); Level.PlaceDirector(false,true); auto* D=Level.Director;
	auto* P=Level.World->SpawnActorDeferred<ASonHatPopulationPresenter>(ASonHatPopulationPresenter::StaticClass(),FTransform::Identity);
	P->Director=D; P->bAutoFindSource=false; P->bProjectFeetToFloor=false; P->bShowBuildingWorldLabels=true;
	FSonHatPopulationPlacement OldPlacement; OldPlacement.Node=Level.Nodes[1]; OldPlacement.Offset=FVector(100000,100000,0); OldPlacement.Representatives=1;
	P->BuildingPlacements.Add(OldPlacement); // Legacy placement must not compete with the centralized rescue rows.
	P->FinishSpawning(FTransform::Identity); P->RefreshPresentation();
	TestEqual(TEXT("Four groups, not 160 Characters"),P->GetBuildingStatuses().Num(),4);
	int32 Figures=0; for (const auto& S:P->GetBuildingStatuses()) { Figures+=S.VisualRepresentatives; TestFalse(TEXT("Initially unregistered"),S.bHasSafeAccess); }
	TestEqual(TEXT("Representative population preserved"),Figures,24);
	TArray<UInstancedStaticMeshComponent*> Instances; P->GetComponents(Instances);
	for (auto* Mesh:Instances)
	{
		for (int32 I=0; I<Mesh->GetInstanceCount(); ++I)
		{
			FTransform Instance; Mesh->GetInstanceTransform(I,Instance,true);
			TestTrue(TEXT("Central rescue placements ignore stale legacy offsets"),FMath::Abs(Instance.GetLocation().X)<10000 && FMath::Abs(Instance.GetLocation().Y)<10000);
		}
	}
	Level.Select(0); P->RefreshPresentation();
	for (const auto& S:P->GetBuildingStatuses())
	{
		TestEqual(TEXT("Only the registered rooftop changes"),S.bHasSafeAccess,S.Node==Level.Nodes[1]);
	}
	TestTrue(TEXT("Summary matches sole scoring authority"),P->GetNetworkSummary().Contains(TEXT("REGISTERED FOR RESCUE: 35 / 160")));
	TArray<UTextRenderComponent*> Labels; P->GetComponents(Labels); bool bFound=false;
	for (auto* Label:Labels) { bFound|=Label->Text.ToString().Contains(TEXT("ROOF A")) && Label->Text.ToString().Contains(TEXT("RESCUE SIGNAL RECEIVED")); }
	TestTrue(TEXT("Group feedback explains registration"),bFound);
	D->GetRoundManager()->FinalizePreparation(); P->RefreshPresentation();
	TestTrue(TEXT("Final summary does not claim physical rescue"),P->GetNetworkSummary().Contains(TEXT("PEOPLE NOT REACHED: 125")) && P->GetNetworkSummary().Contains(TEXT("no physical evacuation")));
	D->RestartRound(); P->RefreshPresentation();
	for (const auto& S:P->GetBuildingStatuses()) { TestFalse(TEXT("Replay clears green registration state"),S.bHasSafeAccess); }
	TestTrue(TEXT("Replay summary starts at zero"),P->GetNetworkSummary().Contains(TEXT("REGISTERED FOR RESCUE: 0 / 160")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatRescueRecoveryTest, "SonHat.Rescue.BoundsRecoveryPreservesRegistrationChargesAndClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatRescueRecoveryTest::RunTest(const FString& Parameters)
{
	FLevel Level(true);
	auto* Floor=Level.World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor);
	Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(600,600,10));
	Box->SetCollisionProfileName(TEXT("BlockAll")); Box->SetWorldLocation(FVector(100,200,5700)); Box->RegisterComponent();
	const FVector Standing(100,200,5710+Level.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2);
	Level.Pawn->SetActorLocation(Standing); auto* Move=Level.Pawn->GetCharacterMovement();
	Move->SetMovementMode(MOVE_Walking); Move->FindFloor(Standing,Move->CurrentFloor,false);
	auto* G=NewObject<UGrappleComponent>(Level.Pawn); Level.Pawn->AddInstanceComponent(G); G->bShowTargetingFeedback=false; G->RegisterComponent();
	Level.PlaceDirector(false,true); auto* D=Level.Director;
	D->bUseLevelRecoveryBounds=true; D->LevelRecoveryWorldZ=-900; D->RestartRound(); G->TickComponent(.01f,LEVELTICK_All,nullptr);
	Level.Select(0); auto* M=D->GetRoundManager();
	Move->SetMovementMode(MOVE_Falling); Level.Pawn->SetActorLocation(FVector(100,200,1200)); G->TickComponent(.01f,LEVELTICK_All,nullptr);
	TestFalse(TEXT("Valid descent above bounds is not a fall failure"),G->IsAwaitingFallRetry());
	Level.Pawn->SetActorLocation(FVector(100,200,-1000)); G->TickComponent(.01f,LEVELTICK_All,nullptr);
	TestTrue(TEXT("Actual OOB detected"),G->IsAwaitingFallRetry());
	Level.World->TimeSeconds+=2; D->Tick(.01f);
	TestFalse(TEXT("Automatic recovery acknowledges only actual fall"),G->IsAwaitingFallRetry());
	TestTrue(TEXT("Supported checkpoint restored"),Level.Pawn->GetActorLocation().Equals(Standing,5));
	TestEqual(TEXT("Same authority after recovery"),D->GetRoundManager(),M);
	TestEqual(TEXT("Registration kept"),M->EstimatedSafePopulation,35);
	TestEqual(TEXT("Charges kept"),M->GetRemainingConnections(),2);
	TestEqual(TEXT("Time continues"),M->GetRemainingTime(),148.f);
	G->TickComponent(.01f,LEVELTICK_All,nullptr); D->Tick(.01f);
	TestFalse(TEXT("No repeated recovery loop"),G->IsAwaitingFallRetry());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
