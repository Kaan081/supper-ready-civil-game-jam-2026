#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SonHatEvacuationLink.h"
#include "SonHatEvacuationManager.h"
#include "SonHatEvacuationNode.h"

namespace SonHatEvacuationTests
{
	// Transient worlds: no level assets, player input, or existing gameplay are changed.
	struct FNetwork
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ASonHatEvacuationManager* Manager = nullptr;
		ASonHatEvacuationNode* Hub = nullptr;
		ASonHatEvacuationNode* A = nullptr;
		ASonHatEvacuationNode* B = nullptr;
		ASonHatEvacuationNode* C = nullptr;
		ASonHatEvacuationNode* D = nullptr;

		FNetwork()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Manager = World->SpawnActor<ASonHatEvacuationManager>();
			Manager->bAutoStartPreparation = false;
			Manager->DispatchBeginPlay();
			Hub = AddNode(0, true);
			A = AddNode(10);
			B = AddNode(20);
			C = AddNode(30);
			D = AddNode(40);
		}

		~FNetwork()
		{
			if (IsValid(Manager)) { Manager->Destroy(); }
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		ASonHatEvacuationNode* AddNode(int32 Population, bool bHub = false)
		{
			ASonHatEvacuationNode* Node = World->SpawnActor<ASonHatEvacuationNode>();
			Node->CivilianCount = Population;
			Node->bIsSafeHub = bHub;
			Manager->Nodes.Add(Node);
			return Node;
		}

		ASonHatEvacuationLink* AddLink(ASonHatEvacuationNode* From, ASonHatEvacuationNode* To, bool bSafe = true)
		{
			ASonHatEvacuationLink* Link = World->SpawnActor<ASonHatEvacuationLink>();
			Link->NodeA = From;
			Link->NodeB = To;
			Link->bFloodSafe = bSafe;
			Manager->CandidateLinks.Add(Link);
			return Link;
		}
	};

	class FAutomaticExpiryCommand : public IAutomationLatentCommand
	{
	public:
		explicit FAutomaticExpiryCommand(FAutomationTestBase* InTest) : Test(InTest)
		{
			Network = MakeUnique<FNetwork>();
			Network->Manager->PreparationDuration = 1.0f;
			Network->Manager->StartPreparation();
			// Timers scheduled before a world's first tick are pending until its tick ends.
			Network->World->GetTimerManager().Tick(0.0f);
			InitialFrame = GFrameCounter;
		}

		virtual bool Update() override
		{
			if (GFrameCounter == InitialFrame) { return false; }
			Network->World->TimeSeconds += 1.0;
			Network->World->GetTimerManager().Tick(1.0f);
			Test->TestTrue(TEXT("Timer ends round without interaction"), Network->Manager->bGameEnded);
			Network.Reset();
			return true;
		}

	private:
		FAutomationTestBase* Test = nullptr;
		TUniquePtr<FNetwork> Network;
		uint64 InitialFrame = 0;
	};
}

using namespace SonHatEvacuationTests;
using EResult = ESonHatEvacuationActivationResult;
using EConfiguration = ESonHatEvacuationConfiguration;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatSafeRoutesTest, "SonHat.Evacuation.SafeRoutesAndCycles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatSafeRoutesTest::RunTest(const FString& Parameters)
{
	FNetwork N;
	ASonHatEvacuationLink* HA = N.AddLink(N.Hub, N.A);
	ASonHatEvacuationLink* AB = N.AddLink(N.A, N.B);
	ASonHatEvacuationLink* BH = N.AddLink(N.B, N.Hub);
	N.Manager->Nodes.Add(N.A); // Duplicate configuration must not duplicate population.
	N.Manager->CandidateLinks.Add(HA);
	TestTrue(TEXT("Starts"), N.Manager->StartPreparation());
	TestEqual(TEXT("Total counts each node once"), N.Manager->TotalPopulation, 100);
	TestEqual(TEXT("Direct safe link"), N.Manager->TryActivateLink(HA, N.A), EResult::Success);
	TestEqual(TEXT("A reaches hub"), N.Manager->EstimatedSafePopulation, 10);
	TestEqual(TEXT("Safe chain"), N.Manager->TryActivateLink(AB, N.A), EResult::Success);
	TestEqual(TEXT("Both A and B reach hub"), N.Manager->EstimatedSafePopulation, 30);
	TestEqual(TEXT("Cycle activates"), N.Manager->TryActivateLink(BH, N.A), EResult::Success);
	TestEqual(TEXT("Cycle does not double count"), N.Manager->EstimatedSafePopulation, 30);
	TestEqual(TEXT("Three activations spent"), N.Manager->GetRemainingConnections(), 0);
	N.Manager->FinalizePreparation();
	TestEqual(TEXT("Disconnected C and D stay unsafe"), N.Manager->FinalSafePopulation, 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatOutcomesTest, "SonHat.Evacuation.TwoPlanningOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatOutcomesTest::RunTest(const FString& Parameters)
{
	for (bool bGoodPlan : {true, false})
	{
		FNetwork N;
		ASonHatEvacuationLink* HB = N.AddLink(N.Hub, N.B);
		ASonHatEvacuationLink* BC = N.AddLink(N.B, N.C);
		ASonHatEvacuationLink* CD = N.AddLink(N.C, N.D);
		ASonHatEvacuationLink* HA = N.AddLink(N.Hub, N.A);
		ASonHatEvacuationLink* AC = N.AddLink(N.A, N.C, false);
		N.AddLink(N.A, N.B);
		N.Manager->StartPreparation();
		const TArray<ASonHatEvacuationLink*> Plan = bGoodPlan
			? TArray<ASonHatEvacuationLink*>{HB, BC, CD} : TArray<ASonHatEvacuationLink*>{HA, AC, CD};
		for (ASonHatEvacuationLink* Link : Plan)
		{
			TestEqual(TEXT("Every plan spends three resources"), N.Manager->TryActivateLink(Link, N.A), EResult::Success);
		}
		N.Manager->FinalizePreparation();
		TestEqual(TEXT("Same budget, different safe populations"), N.Manager->FinalSafePopulation, bGoodPlan ? 90 : 10);
		TestEqual(TEXT("Unsafe links also spend budget"), N.Manager->GetRemainingConnections(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatInteractionTest, "SonHat.Evacuation.InteractionRejections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatInteractionTest::RunTest(const FString& Parameters)
{
	FNetwork N;
	ASonHatEvacuationLink* HA = N.AddLink(N.Hub, N.A);
	ASonHatEvacuationLink* HB = N.AddLink(N.Hub, N.B);
	ASonHatEvacuationLink* ForeignLink = N.World->SpawnActor<ASonHatEvacuationLink>();
	N.Manager->MaxActivations = 1;
	TestEqual(TEXT("Not started"), N.Manager->TryActivateLink(HA, N.A), EResult::NotStarted);
	N.Manager->StartPreparation();
	TestFalse(TEXT("Cannot reset round to regain resources"), N.Manager->StartPreparation());
	TestEqual(TEXT("Null interactor"), N.Manager->TryActivateLink(HA, nullptr), EResult::InvalidInteractor);
	TestEqual(TEXT("Null link"), N.Manager->TryActivateLink(nullptr, N.A), EResult::InvalidLink);
	TestEqual(TEXT("Unregistered link"), N.Manager->TryActivateLink(ForeignLink, N.A), EResult::LinkNotRegistered);
	HA->SetActorLocation(FVector(1000, 0, 0));
	HB->SetActorLocation(FVector(2000, 0, 0));
	TestEqual(TEXT("Too far"), N.Manager->TryActivateLink(HA, N.A), EResult::TooFarAway);
	ASonHatEvacuationLink* Selected = nullptr;
	TestEqual(TEXT("No terminal nearby"), N.Manager->TryActivateNearestLink(N.A, Selected), EResult::NoCandidateInRange);
	TestNull(TEXT("No selection"), Selected);
	HA->SetActorLocation(FVector(N.Manager->InteractionDistance, 0, 0));
	TestEqual(TEXT("Range boundary inclusive"), N.Manager->TryActivateNearestLink(N.A, Selected), EResult::Success);
	TestTrue(TEXT("Returns activated terminal"), Selected == HA);
	TestEqual(TEXT("Duplicate activation rejected"), N.Manager->TryActivateLink(HA, N.A), EResult::AlreadyActive);
	TestEqual(TEXT("Duplicate did not spend again"), N.Manager->GetRemainingConnections(), 0);
	HB->SetActorLocation(FVector::ZeroVector);
	TestEqual(TEXT("Exhausted budget"), N.Manager->TryActivateLink(HB, N.A), EResult::BudgetExhausted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatDeadlineTest, "SonHat.Evacuation.DeadlineAndFinalSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatDeadlineTest::RunTest(const FString& Parameters)
{
	FNetwork N;
	ASonHatEvacuationLink* HA = N.AddLink(N.Hub, N.A);
	ASonHatEvacuationLink* HB = N.AddLink(N.Hub, N.B);
	N.Manager->StartPreparation();
	TestEqual(TEXT("Default duration"), N.Manager->GetRemainingTime(), 120.0f);
	N.Manager->TryActivateLink(HA, N.A);
	// Move the transient world's clock, deliberately before the timer callback runs.
	N.World->TimeSeconds += 120.0;
	TestEqual(TEXT("Deadline itself rejects input"), N.Manager->TryActivateLink(HB, N.A), EResult::PreparationEnded);
	TestTrue(TEXT("Round has ended"), N.Manager->bGameEnded);
	TestEqual(TEXT("Time clamps to zero"), N.Manager->GetRemainingTime(), 0.0f);
	TestEqual(TEXT("Final safe population"), N.Manager->FinalSafePopulation, 10);
	N.A->CivilianCount = 999;
	N.Manager->FinalizePreparation();
	TestEqual(TEXT("Final safe result is frozen"), N.Manager->FinalSafePopulation, 10);
	TestEqual(TEXT("Final total is frozen"), N.Manager->TotalPopulation, 100);
	TestFalse(TEXT("No late activation"), HB->IsActive());
	TestEqual(TEXT("No late resource consumption"), N.Manager->GetRemainingConnections(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatAutomaticExpiryTest, "SonHat.Evacuation.AutomaticAndZeroDurationExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatAutomaticExpiryTest::RunTest(const FString& Parameters)
{
	{
		FNetwork N;
		N.Manager->PreparationDuration = 0.0f;
		N.Manager->StartPreparation();
		TestTrue(TEXT("Zero duration immediately ends"), N.Manager->bGameEnded);
	}
	ADD_LATENT_AUTOMATION_COMMAND(FAutomaticExpiryCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatInvalidConfigurationTest, "SonHat.Evacuation.InvalidConfigurationAndReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatInvalidConfigurationTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("configure at least one valid node and exactly one safe hub"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 3);
	AddExpectedMessagePlain(TEXT("ignoring a missing or foreign-world node"), ELogVerbosity::Warning);
	AddExpectedMessagePlain(TEXT("has invalid endpoints, unregistered nodes, or another owner"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 3);
	AddExpectedMessagePlain(TEXT("ignoring a missing or foreign-world link"), ELogVerbosity::Warning);
	{
		FNetwork N;
		N.Hub->bIsSafeHub = false;
		ASonHatEvacuationLink* HA = N.AddLink(N.Hub, N.A);
		N.Manager->StartPreparation();
		TestEqual(TEXT("Missing hub is understandable"), N.Manager->ConfigurationStatus, EConfiguration::MissingHub);
		TestEqual(TEXT("Cannot spend without a hub"), N.Manager->TryActivateLink(HA, N.A), EResult::InvalidNetwork);
		N.Manager->FinalizePreparation();
		TestEqual(TEXT("Missing hub safely yields zero"), N.Manager->FinalSafePopulation, 0);
	}
	{
		FNetwork N;
		N.A->bIsSafeHub = true;
		N.Manager->StartPreparation();
		TestEqual(TEXT("Multiple hubs fail explicitly"), N.Manager->ConfigurationStatus, EConfiguration::MultipleHubs);
	}
	{
		FNetwork N;
		N.Manager->Nodes.Reset();
		N.Manager->Nodes.Add(nullptr);
		N.Manager->StartPreparation();
		TestEqual(TEXT("No valid nodes"), N.Manager->ConfigurationStatus, EConfiguration::NoValidNodes);
	}
	{
		FNetwork N;
		ASonHatEvacuationLink* Missing = N.AddLink(N.A, nullptr);
		ASonHatEvacuationLink* Self = N.AddLink(N.A, N.A);
		ASonHatEvacuationNode* Unregistered = N.World->SpawnActor<ASonHatEvacuationNode>();
		ASonHatEvacuationLink* Outside = N.AddLink(N.A, Unregistered);
		ASonHatEvacuationLink* HA = N.AddLink(N.Hub, N.A);
		N.Manager->CandidateLinks.Add(nullptr);
		N.Manager->StartPreparation();
		for (ASonHatEvacuationLink* Link : {Missing, Self, Outside})
		{
			TestEqual(TEXT("Bad endpoint rejected"), N.Manager->TryActivateLink(Link, N.A), EResult::InvalidLink);
		}
		TestEqual(TEXT("Bad references cost nothing"), N.Manager->GetRemainingConnections(), 3);
		N.Manager->TryActivateLink(HA, N.A);
		N.A->Destroy();
		N.Manager->FinalizePreparation();
		TestEqual(TEXT("Destroyed node cannot evacuate"), N.Manager->FinalSafePopulation, 0);
		TestEqual(TEXT("Destroyed population removed before finalization"), N.Manager->TotalPopulation, 90);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatOwnershipTest, "SonHat.Evacuation.ExclusiveOwnershipAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatOwnershipTest::RunTest(const FString& Parameters)
{
	AddExpectedMessagePlain(TEXT("has invalid endpoints, unregistered nodes, or another owner"), ELogVerbosity::Warning);
	FNetwork N;
	ASonHatEvacuationLink* HA = N.AddLink(N.Hub, N.A);
	N.Manager->StartPreparation();
	N.Manager->TryActivateLink(HA, N.A);
	ASonHatEvacuationManager* Other = N.World->SpawnActor<ASonHatEvacuationManager>();
	Other->bAutoStartPreparation = false;
	Other->Nodes = N.Manager->Nodes;
	Other->CandidateLinks.Add(HA);
	Other->DispatchBeginPlay();
	Other->StartPreparation();
	TestEqual(TEXT("Second manager cannot share activation state"), Other->TryActivateLink(HA, N.A), EResult::LinkOwnedByAnotherManager);
	TestEqual(TEXT("Second network does not inherit routes"), Other->EstimatedSafePopulation, 0);
	TestTrue(TEXT("Second manager did not reset first manager's link"), HA->IsActive());
	N.Manager->Destroy();
	TestFalse(TEXT("Owner cleanup resets link"), HA->IsActive());
	Other->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
