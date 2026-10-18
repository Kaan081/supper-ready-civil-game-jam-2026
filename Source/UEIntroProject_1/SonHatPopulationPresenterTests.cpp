#if WITH_DEV_AUTOMATION_TESTS

#include "SonHatPopulationPresenter.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "SonHatEvacuationDirector.h"
#include "SonHatEvacuationLink.h"
#include "SonHatEvacuationManager.h"
#include "SonHatEvacuationNode.h"

namespace SonHatPopulationTests
{
	struct FNetwork
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ASonHatEvacuationManager* Manager = nullptr;
		ASonHatPopulationPresenter* Presenter = nullptr;
		TArray<TObjectPtr<ASonHatEvacuationNode>> Nodes;
		TArray<TObjectPtr<ASonHatEvacuationLink>> Links;
		FNetwork()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
			const int32 Counts[] = {0, 35, 45, 60, 20};
			for (int32 Index = 0; Index < 5; ++Index)
			{
				auto* Node = World->SpawnActor<ASonHatEvacuationNode>();
				Node->CivilianCount = Counts[Index];
				Node->bIsSafeHub = Index == 0;
				Node->SetActorLocation(FVector(Index * 1000, 0, 1000));
				Node->BuildingName = FText::FromString(FString::Printf(TEXT("Building %d"), Index));
				Nodes.Add(Node);
			}
			const int32 From[] = {0, 1, 0, 3, 1, 0};
			const int32 To[] = {1, 2, 3, 4, 3, 4};
			for (int32 Index = 0; Index < 6; ++Index)
			{
				auto* Link = World->SpawnActor<ASonHatEvacuationLink>();
				Link->NodeA = Nodes[From[Index]];
				Link->NodeB = Nodes[To[Index]];
				Link->bFloodSafe = Index != 4;
				Links.Add(Link);
			}
		}
		~FNetwork()
		{
			World->EndPlay(EEndPlayReason::EndPlayInEditor);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		void PlaceManager()
		{
			Manager = World->SpawnActorDeferred<ASonHatEvacuationManager>(ASonHatEvacuationManager::StaticClass(), FTransform::Identity);
			Manager->bAutoStartPreparation = false;
			Manager->Nodes = Nodes;
			Manager->Nodes.Add(Nodes[1]); // Duplicate references must not duplicate figures/counts.
			Manager->CandidateLinks = Links;
			Manager->PreparationDuration = 90;
			Manager->InteractionDistance = 100000;
			Manager->FinishSpawning(FTransform::Identity);
		}
		void PlacePresenter(ASonHatEvacuationDirector* Director = nullptr, bool bProject = false)
		{
			Presenter = World->SpawnActorDeferred<ASonHatPopulationPresenter>(ASonHatPopulationPresenter::StaticClass(), FTransform::Identity);
			Presenter->Director = Director;
			Presenter->StandaloneManager = Director ? nullptr : Manager;
			Presenter->bAutoFindSource = false;
			Presenter->bProjectFeetToFloor = bProject;
			Presenter->FinishSpawning(FTransform::Identity);
		}
		FSonHatPopulationBuildingStatus Status(int32 Index) const
		{
			for (const auto& S : Presenter->GetBuildingStatuses()) { if (S.Node == Nodes[Index]) { return S; } }
			return FSonHatPopulationBuildingStatus();
		}
	};
}

using namespace SonHatPopulationTests;
using EPopulation = ESonHatPopulationVisualState;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPopulationGraphTest, "SonHat.Population.GraphChoicesRepresentativesAndFinalCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPopulationGraphTest::RunTest(const FString& Parameters)
{
	FNetwork N;
	N.PlaceManager();
	N.PlacePresenter(); // Deliberately before StartPreparation, exercising BeginPlay order.
	N.Manager->StartPreparation();
	TestEqual(TEXT("Four population buildings, no empty hub or duplicate group"), N.Presenter->GetBuildingStatuses().Num(), 4);
	TestEqual(TEXT("Manager owns total of 160"), N.Manager->TotalPopulation, 160);
	TestEqual(TEXT("Six figures are not 35 residents"), N.Status(1).VisualRepresentatives, 6);
	TestEqual(TEXT("Actual A population retained"), N.Status(1).Residents, 35);
	TestEqual(TEXT("Initially waiting"), N.Status(1).State, EPopulation::Waiting);
	N.Manager->TryActivateLink(N.Links[1], N.Nodes[1]);
	TestEqual(TEXT("Disconnected selected safe link is not safe access"), N.Status(2).State, EPopulation::Waiting);
	N.Manager->TryActivateLink(N.Links[0], N.Nodes[1]);
	TestEqual(TEXT("Activation event immediately updates A without polling"), N.Status(1).State, EPopulation::SafeAccess);
	TestEqual(TEXT("Activation event immediately updates downstream B"), N.Status(2).State, EPopulation::SafeAccess);
	N.Manager->TryActivateLink(N.Links[4], N.Nodes[1]);
	TestEqual(TEXT("Unsafe link does not make C safe"), N.Status(3).State, EPopulation::Waiting);
	TestTrue(TEXT("Summary uses manager safe/total"), N.Presenter->GetNetworkSummary().Contains(TEXT("80 / 160")));
	N.Manager->FinalizePreparation();
	TestEqual(TEXT("Final A safe access"), N.Status(1).State, EPopulation::SafeAccess);
	TestEqual(TEXT("Final C stranded"), N.Status(3).State, EPopulation::Stranded);
	TestEqual(TEXT("Final D stranded"), N.Status(4).State, EPopulation::Stranded);
	TestTrue(TEXT("Final summary explicitly states lack of access"), N.Presenter->GetNetworkSummary().Contains(TEXT("LACKED SAFE-ROUTE ACCESS: 80")));
	N.Nodes[3]->CivilianCount = 999; // Presentation must not rewrite a frozen final count after authoring edits.
	N.Presenter->RefreshPresentation();
	TestEqual(TEXT("Final local population snapshot frozen"), N.Status(3).Residents, 60);
	TestEqual(TEXT("Final authoritative score unchanged"), N.Manager->FinalSafePopulation, 80);
	TestFalse(TEXT("Presenter has no Actor Tick"), N.Presenter->PrimaryActorTick.bCanEverTick);
	TArray<UInstancedStaticMeshComponent*> Components;
	N.Presenter->GetComponents(Components);
	TestEqual(TEXT("One instanced human mesh component per building"), Components.Num(), 4);
	for (const auto* C : Components)
	{
		TestEqual(TEXT("Proxies cannot block grapple or movement"), C->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("No proxy nav influence"), C->CanEverAffectNavigation());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPopulationFloodReplayTest, "SonHat.Population.DirectorFloodThreatFinalAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPopulationFloodReplayTest::RunTest(const FString& Parameters)
{
	FNetwork N;
	auto* PC = N.World->SpawnActor<APlayerController>();
	PC->SetAsLocalPlayerController();
	auto* Pawn = N.World->SpawnActor<ACharacter>();
	PC->Possess(Pawn);
	auto* Director = N.World->SpawnActorDeferred<ASonHatEvacuationDirector>(ASonHatEvacuationDirector::StaticClass(), FTransform::Identity);
	Director->bAutoDiscoverNetwork = false;
	Director->Nodes = N.Nodes;
	Director->CandidateLinks = N.Links;
	Director->bAutoStart = false;
	Director->InteractionDistance = 100000;
	Director->FloodStartElevation = 200;
	Director->FloodFinalElevation = 2000;
	Director->FloodRiseExponent = 1;
	Director->FinishSpawning(FTransform::Identity);
	TestTrue(TEXT("Actual Director round starts"), Director->RestartRound());
	N.PlacePresenter(Director);
	const auto FirstHuman = [&N]()
	{
		TArray<UInstancedStaticMeshComponent*> Components;
		N.Presenter->GetComponents(Components);
		FTransform Result; double MinX = MAX_dbl;
		for (auto* Component : Components)
		{
			if (!Component->GetInstanceCount()) { continue; }
			FTransform T; Component->GetInstanceTransform(0, T, true);
			if (T.GetLocation().X < MinX) { MinX = T.GetLocation().X; Result = T; }
		}
		return Result;
	};
	const FTransform InitialAnimationPose = FirstHuman();
	TestEqual(TEXT("Water initially distant"), N.Status(3).State, EPopulation::Waiting);
	Director->GetRoundManager()->TryActivateLink(N.Links[0], N.Nodes[1]);
	N.World->TimeSeconds += 40; // Public Director water query now yields 1000; groups are at Z1000.
	N.Presenter->RefreshPresentation();
	TestEqual(TEXT("Rising water changes unconnected C"), N.Status(3).State, EPopulation::FloodThreatened);
	TestTrue(TEXT("Safe building still reports nearby flood"), N.Status(1).bFloodThreatened);
	TestEqual(TEXT("Flood does not revoke graph-verified access"), N.Status(1).State, EPopulation::SafeAccess);
	Director->GetRoundManager()->FinalizePreparation();
	TestEqual(TEXT("Final C clearly stranded"), N.Status(3).State, EPopulation::Stranded);
	TestTrue(TEXT("Final graph result, not number of figures"), N.Presenter->GetNetworkSummary().Contains(TEXT("35 / 160")));
	TestTrue(TEXT("Director can replace its manager"), Director->RestartRound());
	N.Presenter->RefreshPresentation();
	TestEqual(TEXT("Replay clears prior green state"), N.Status(1).State, EPopulation::Waiting);
	TestEqual(TEXT("Replay restores population, not prior authoring edits"), N.Status(3).Residents, 60);
	TestTrue(TEXT("Replay starts at zero access"), N.Presenter->GetNetworkSummary().Contains(TEXT("0 / 160")));
	TestTrue(TEXT("Replay resets animation phase and transforms"), FirstHuman().Equals(InitialAnimationPose, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPopulationPlacementTest, "SonHat.Population.FloorProjectionCapsAndSourceLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPopulationPlacementTest::RunTest(const FString& Parameters)
{
	FNetwork N;
	auto* Floor = N.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(10000, 10000, 10));
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	N.PlaceManager();
	N.Manager->StartPreparation();
	N.PlacePresenter(nullptr, true);
	N.Presenter->RepresentativesPerBuilding = 1000;
	N.Presenter->MaxTotalRepresentatives = 17;
	auto* Orphan = NewObject<UInstancedStaticMeshComponent>(N.Presenter);
	Orphan->ComponentTags.Add(TEXT("SonHatPopulationVisual"));
	N.Presenter->AddInstanceComponent(Orphan);
	Orphan->RegisterComponent(); // Simulate an editor-preview component with lost transient bookkeeping.
	N.Presenter->RebuildPresentation();
	int32 Figures = 0;
	for (const auto& Status : N.Presenter->GetBuildingStatuses()) { Figures += Status.VisualRepresentatives; TestTrue(TEXT("Per-building hard cap"), Status.VisualRepresentatives <= 16); }
	TestEqual(TEXT("Global visual budget enforced"), Figures, 17);
	TArray<UInstancedStaticMeshComponent*> Components;
	N.Presenter->GetComponents(Components);
	TestEqual(TEXT("Rebuild removes orphaned editor previews"), Components.Num(), 4);
	for (const auto* C : Components)
	{
		for (int32 Index = 0; Index < C->GetInstanceCount(); ++Index)
		{
			FTransform T;
			C->GetInstanceTransform(Index, T, true);
			TestTrue(TEXT("Figures projected to actual floor, not node marker height"), T.GetLocation().Z < 250);
		}
	}
	N.Manager->Destroy();
	N.Presenter->RefreshPresentation();
	TestTrue(TEXT("Destroyed source cannot retain a green/final claim"), N.Presenter->GetBuildingStatuses().IsEmpty());
	TestTrue(TEXT("Missing source explicitly described"), N.Presenter->GetNetworkSummary().Contains(TEXT("WAITING FOR A VALID ROUND")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPopulationDiscoveryTest, "SonHat.Population.AutoDiscoveryRejectsAmbiguousNetworks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPopulationDiscoveryTest::RunTest(const FString& Parameters)
{
	FNetwork N;
	N.PlaceManager();
	N.Manager->StartPreparation();
	N.Presenter = N.World->SpawnActorDeferred<ASonHatPopulationPresenter>(ASonHatPopulationPresenter::StaticClass(), FTransform::Identity);
	N.Presenter->bProjectFeetToFloor = false;
	N.Presenter->FinishSpawning(FTransform::Identity); // Default auto discovery; no configured references.
	TestEqual(TEXT("One standalone network automatically discovered"), N.Presenter->GetBuildingStatuses().Num(), 4);
	auto* Other = N.World->SpawnActorDeferred<ASonHatEvacuationManager>(ASonHatEvacuationManager::StaticClass(), FTransform::Identity);
	Other->bAutoStartPreparation = false;
	Other->FinishSpawning(FTransform::Identity);
	N.Presenter->RefreshPresentation();
	TestTrue(TEXT("Two sources are not guessed"), N.Presenter->GetNetworkSummary().Contains(TEXT("AMBIGUOUS NETWORK")));
	TestTrue(TEXT("Ambiguity cannot show incorrect safe groups"), N.Presenter->GetBuildingStatuses().IsEmpty());
	N.Presenter->StandaloneManager = N.Manager;
	N.Presenter->RefreshPresentation();
	TestEqual(TEXT("Explicit reference resolves ambiguity without graph edits"), N.Presenter->GetBuildingStatuses().Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPopulationLivingTest, "SonHat.Population.LivingHumansFootPlantReactionsAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPopulationLivingTest::RunTest(const FString& Parameters)
{
	FNetwork N;
	N.PlaceManager(); N.Manager->StartPreparation(); N.PlacePresenter();
	for (UStaticMesh* Mesh : {N.Presenter->HumanWaitingMesh.Get(), N.Presenter->HumanSafeMesh.Get(), N.Presenter->HumanThreatMesh.Get(), N.Presenter->HumanStrandedMesh.Get()})
	{
		if (!TestNotNull(TEXT("Cook-referenced original human pose asset loaded"), Mesh)) { return false; }
		TestEqual(TEXT("One material slot per human"), Mesh->GetStaticMaterials().Num(), 1);
		TestTrue(TEXT("Mesh feet at origin, not centered/floating"), FMath::Abs(Mesh->GetBoundingBox().Min.Z) < 0.5);
		TestTrue(TEXT("Human proportions in centimeters"), Mesh->GetBoundingBox().Max.Z > 165 && Mesh->GetBoundingBox().Max.Z < 205);
	}
	TArray<UInstancedStaticMeshComponent*> Components;
	N.Presenter->GetComponents(Components);
	TestEqual(TEXT("Four batches, not 24 or 160 NPC actors"), Components.Num(), 4);
	TArray<FTransform> Initial;
	int32 Total = 0;
	for (auto* Component : Components)
	{
		Total += Component->GetInstanceCount();
		FTransform T; Component->GetInstanceTransform(0, T, true); Initial.Add(T);
		TestTrue(TEXT("Initial waiting pose"), Component->GetStaticMesh() == N.Presenter->HumanWaitingMesh);
	}
	TestEqual(TEXT("Exactly 24 representative humans"), Total, 24);
	N.World->TimeSeconds += 0.4;
	N.Presenter->RefreshPresentation();
	bool bMoved = false;
	for (int32 Index = 0; Index < Components.Num(); ++Index)
	{
		FTransform T; Components[Index]->GetInstanceTransform(0, T, true);
		TestEqual(TEXT("Idle preserves feet positions"), T.GetLocation(), Initial[Index].GetLocation());
		bMoved |= !T.Equals(Initial[Index]);
		TestEqual(TEXT("Animation never adds duplicates"), Components[Index]->GetInstanceCount(), 6);
	}
	TestTrue(TEXT("Idle actually changes instance transforms"), bMoved);
	N.Manager->TryActivateLink(N.Links[4], N.Nodes[1]);
	TestEqual(TEXT("Unsafe still leaves 0/160"), N.Manager->EstimatedSafePopulation, 0);
	N.Manager->TryActivateLink(N.Links[0], N.Nodes[1]);
	N.Manager->TryActivateLink(N.Links[1], N.Nodes[1]);
	TestEqual(TEXT("Safe chain still yields 80/160"), N.Manager->EstimatedSafePopulation, 80);
	int32 PositiveGroups = 0;
	for (auto* Component : Components) { PositiveGroups += Component->GetStaticMesh() == N.Presenter->HumanSafeMesh; }
	TestEqual(TEXT("Both affected buildings react immediately"), PositiveGroups, 2);
	N.Manager->FinalizePreparation();
	int32 StrandedGroups = 0;
	for (auto* Component : Components) { StrandedGroups += Component->GetStaticMesh() == N.Presenter->HumanStrandedMesh; }
	TestEqual(TEXT("Disconnected buildings have distinct final pose"), StrandedGroups, 2);
	// Rebuild resets the animation epoch as replay manager replacement does.
	N.Presenter->RebuildPresentation();
	N.Presenter->GetComponents(Components);
	TestEqual(TEXT("Rebuild remains four components"), Components.Num(), 4);
	TestFalse(TEXT("No Actor Tick added"), N.Presenter->PrimaryActorTick.bCanEverTick);
	return true;
}

#endif
