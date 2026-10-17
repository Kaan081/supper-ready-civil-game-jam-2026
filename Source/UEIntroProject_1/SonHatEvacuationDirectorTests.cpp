#if WITH_DEV_AUTOMATION_TESTS

#include "SonHatEvacuationDirector.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
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

		FLevel()
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
			for (int32 Index = 0; Index < 6; ++Index)
			{
				ASonHatEvacuationLink* Link = World->SpawnActor<ASonHatEvacuationLink>();
				Link->NodeA = Nodes[From[Index]];
				Link->NodeB = Nodes[To[Index]];
				Link->bFloodSafe = Index != 4;
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

		void PlaceDirector(bool bDiscover = false)
		{
			const FTransform Transform(FRotator::ZeroRotator, FVector(0, 0, 500));
			Director = World->SpawnActorDeferred<ASonHatEvacuationDirector>(ASonHatEvacuationDirector::StaticClass(), Transform);
			Director->bAutoDiscoverNetwork = bDiscover;
			Director->NetworkTag = TEXT("RooftopRound");
			Director->Nodes = Nodes;
			Director->CandidateLinks = Links;
			Director->FloodRevealDuration = 2;
			Director->bRequireLineOfSight = false;
			Director->FinishSpawning(Transform);
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

#endif // WITH_DEV_AUTOMATION_TESTS
