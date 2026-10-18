#include "SonHatEvacuationManager.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "SonHatEvacuationLink.h"
#include "SonHatEvacuationNode.h"

DEFINE_LOG_CATEGORY_STATIC(LogSonHatEvacuation, Log, All);

ASonHatEvacuationManager::ASonHatEvacuationManager()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("NetworkLocation")));
}

void ASonHatEvacuationManager::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoStartPreparation)
	{
		StartPreparation();
	}
}

bool ASonHatEvacuationManager::StartPreparation()
{
	UWorld* World = GetWorld();
	if (bHasStarted || !World)
	{
		return false;
	}

	for (ASonHatEvacuationNode* Node : Nodes)
	{
		if (IsValid(Node) && Node->GetWorld() == World)
		{
			RegisteredNodes.AddUnique(Node);
		}
		else
		{
			UE_LOG(LogSonHatEvacuation, Warning, TEXT("%s: ignoring a missing or foreign-world node."), *GetName());
		}
	}
	for (ASonHatEvacuationLink* Link : CandidateLinks)
	{
		if (IsValid(Link) && Link->GetWorld() == World)
		{
			RegisteredLinks.AddUnique(Link);
			if (!Link->NetworkOwner.IsValid())
			{
				Link->NetworkOwner = this;
				Link->bActive = false;
			}
			if (Link->NetworkOwner.Get() != this || !IsUsableLink(Link))
			{
				UE_LOG(LogSonHatEvacuation, Warning, TEXT("%s: link %s has invalid endpoints, unregistered nodes, or another owner."), *GetName(), *Link->GetName());
			}
		}
		else
		{
			UE_LOG(LogSonHatEvacuation, Warning, TEXT("%s: ignoring a missing or foreign-world link."), *GetName());
		}
	}

	bHasStarted = true;
	RoundActivationBudget = FMath::Max(0, MaxActivations);
	const float Duration = FMath::IsFinite(PreparationDuration) ? FMath::Max(0.0f, PreparationDuration) : 0.0f;
	PreparationDeadline = World->GetTimeSeconds() + static_cast<double>(Duration);
	EvaluateNetwork();
	if (ConfigurationStatus != ESonHatEvacuationConfiguration::Ready)
	{
		UE_LOG(LogSonHatEvacuation, Warning, TEXT("%s: configure at least one valid node and exactly one safe hub. Activations are disabled until the network is valid."), *GetName());
	}
	if (Duration <= 0.0f)
	{
		FinalizePreparation();
	}
	else
	{
		World->GetTimerManager().SetTimer(PreparationUpdateTimer, this,
			&ASonHatEvacuationManager::RefreshPreparation, 0.1f, true);
		OnStateChanged.Broadcast();
	}
	return true;
}

float ASonHatEvacuationManager::GetRemainingTime() const
{
	return bHasStarted && !bGameEnded && GetWorld()
		? static_cast<float>(FMath::Max(0.0, PreparationDeadline - GetWorld()->GetTimeSeconds())) : 0.0f;
}

void ASonHatEvacuationManager::ExtendPreparationDeadline(float ExtraSeconds)
{
	if (!bHasStarted || bGameEnded || !GetWorld() || !FMath::IsFinite(ExtraSeconds) || ExtraSeconds <= 0.f) { return; }
	PreparationDeadline += static_cast<double>(ExtraSeconds);
}

bool ASonHatEvacuationManager::CheckDeadline()
{
	if (bHasStarted && !bGameEnded && GetWorld() && GetWorld()->GetTimeSeconds() >= PreparationDeadline)
	{
		FinalizePreparation();
	}
	return bGameEnded;
}

bool ASonHatEvacuationManager::IsValidInteractor(const AActor* Interactor) const
{
	return IsValid(Interactor) && Interactor->GetWorld() == GetWorld()
		&& !Interactor->GetActorLocation().ContainsNaN();
}

bool ASonHatEvacuationManager::IsUsableLink(const ASonHatEvacuationLink* Link) const
{
	return IsValid(Link) && Link->GetWorld() == GetWorld() && Link->HasValidEndpoints()
		&& RegisteredNodes.Contains(Link->NodeA) && RegisteredNodes.Contains(Link->NodeB);
}

bool ASonHatEvacuationManager::HasSafeRoute(ASonHatEvacuationNode* Node) const
{
	return IsValid(Node) && NodesWithSafeAccess.Contains(Node);
}

bool ASonHatEvacuationManager::OwnsLink(ASonHatEvacuationLink* Link) const
{
	return IsValid(Link) && Link->NetworkOwner.Get() == this && RegisteredLinks.Contains(Link);
}

ESonHatEvacuationActivationResult ASonHatEvacuationManager::TryActivateNearestLink(
	AActor* Interactor, ASonHatEvacuationLink*& OutLink)
{
	OutLink = nullptr;
	if (!bHasStarted) { return ESonHatEvacuationActivationResult::NotStarted; }
	if (CheckDeadline()) { return ESonHatEvacuationActivationResult::PreparationEnded; }
	if (!IsValidInteractor(Interactor)) { return ESonHatEvacuationActivationResult::InvalidInteractor; }

	const double Range = FMath::IsFinite(InteractionDistance) ? FMath::Max(0.0f, InteractionDistance) : 0.0;
	double BestDistanceSquared = Range * Range;
	ASonHatEvacuationLink* NearestActive = nullptr;
	double ActiveDistanceSquared = BestDistanceSquared;
	for (ASonHatEvacuationLink* Link : RegisteredLinks)
	{
		if (!IsValid(Link) || Link->GetActorLocation().ContainsNaN()) { continue; }
		const double DistanceSquared = FVector::DistSquared(Interactor->GetActorLocation(), Link->GetActorLocation());
		if (Link->bActive)
		{
			if (DistanceSquared <= ActiveDistanceSquared && (!NearestActive || DistanceSquared < ActiveDistanceSquared))
			{
				NearestActive = Link;
				ActiveDistanceSquared = DistanceSquared;
			}
		}
		else if (DistanceSquared <= BestDistanceSquared && (!OutLink || DistanceSquared < BestDistanceSquared))
		{
			OutLink = Link;
			BestDistanceSquared = DistanceSquared;
		}
	}
	if (!OutLink)
	{
		OutLink = NearestActive;
	}
	return OutLink ? TryActivateLink(OutLink, Interactor) : ESonHatEvacuationActivationResult::NoCandidateInRange;
}

ESonHatEvacuationActivationResult ASonHatEvacuationManager::TryActivateLink(
	ASonHatEvacuationLink* Link, AActor* Interactor)
{
	if (!bHasStarted) { return ESonHatEvacuationActivationResult::NotStarted; }
	if (CheckDeadline()) { return ESonHatEvacuationActivationResult::PreparationEnded; }
	if (!IsValidInteractor(Interactor)) { return ESonHatEvacuationActivationResult::InvalidInteractor; }
	if (!IsValid(Link)) { return ESonHatEvacuationActivationResult::InvalidLink; }
	if (!RegisteredLinks.Contains(Link)) { return ESonHatEvacuationActivationResult::LinkNotRegistered; }
	if (Link->NetworkOwner.Get() != this) { return ESonHatEvacuationActivationResult::LinkOwnedByAnotherManager; }
	if (!IsUsableLink(Link)) { return ESonHatEvacuationActivationResult::InvalidLink; }
	if (Link->bActive) { return ESonHatEvacuationActivationResult::AlreadyActive; }
	const double Range = FMath::IsFinite(InteractionDistance) ? FMath::Max(0.0f, InteractionDistance) : 0.0;
	if (Link->GetActorLocation().ContainsNaN()
		|| FVector::DistSquared(Interactor->GetActorLocation(), Link->GetActorLocation()) > Range * Range)
	{
		return ESonHatEvacuationActivationResult::TooFarAway;
	}
	EvaluateNetwork();
	if (ConfigurationStatus != ESonHatEvacuationConfiguration::Ready) { return ESonHatEvacuationActivationResult::InvalidNetwork; }
	if (GetRemainingConnections() <= 0) { return ESonHatEvacuationActivationResult::BudgetExhausted; }

	// Commit all state before notifying Blueprint; unsafe links spend the same resource.
	Link->bActive = true;
	++UsedActivations;
	EvaluateNetwork();
	Link->OnActivationChanged.Broadcast(true);
	OnStateChanged.Broadcast();
	return ESonHatEvacuationActivationResult::Success;
}

void ASonHatEvacuationManager::EvaluateNetwork()
{
	NodesWithSafeAccess.Reset();
	TSet<ASonHatEvacuationNode*> ValidNodes;
	ASonHatEvacuationNode* Hub = nullptr;
	int32 HubCount = 0;
	int64 Population = 0;
	for (ASonHatEvacuationNode* Node : RegisteredNodes)
	{
		if (!IsValid(Node) || Node->GetWorld() != GetWorld()) { continue; }
		ValidNodes.Add(Node);
		Population += FMath::Max(0, Node->CivilianCount);
		if (Node->bIsSafeHub)
		{
			Hub = Node;
			++HubCount;
		}
	}
	TotalPopulation = static_cast<int32>(FMath::Min<int64>(Population, MAX_int32));
	ConfigurationStatus = ValidNodes.IsEmpty() ? ESonHatEvacuationConfiguration::NoValidNodes
		: HubCount == 0 ? ESonHatEvacuationConfiguration::MissingHub
		: HubCount > 1 ? ESonHatEvacuationConfiguration::MultipleHubs
		: ESonHatEvacuationConfiguration::Ready;
	EstimatedSafePopulation = 0;
	if (ConfigurationStatus != ESonHatEvacuationConfiguration::Ready) { return; }

	// Undirected graph; unsafe, destroyed, foreign, or unregistered edges never carry routes.
	TMap<ASonHatEvacuationNode*, TArray<ASonHatEvacuationNode*>> Neighbors;
	for (ASonHatEvacuationLink* Link : RegisteredLinks)
	{
		if (IsUsableLink(Link) && Link->NetworkOwner.Get() == this && Link->bActive && Link->bFloodSafe)
		{
			Neighbors.FindOrAdd(Link->NodeA.Get()).Add(Link->NodeB.Get());
			Neighbors.FindOrAdd(Link->NodeB.Get()).Add(Link->NodeA.Get());
		}
	}
	TSet<ASonHatEvacuationNode*> Visited;
	TArray<ASonHatEvacuationNode*> Queue;
	Visited.Add(Hub);
	Queue.Add(Hub);
	NodesWithSafeAccess.Add(Hub);
	int64 SafePopulation = 0;
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		ASonHatEvacuationNode* Node = Queue[Index];
		SafePopulation += FMath::Max(0, Node->CivilianCount);
		if (const TArray<ASonHatEvacuationNode*>* Adjacent = Neighbors.Find(Node))
		{
			for (ASonHatEvacuationNode* Neighbor : *Adjacent)
			{
				if (!Visited.Contains(Neighbor))
				{
					Visited.Add(Neighbor);
					Queue.Add(Neighbor);
					NodesWithSafeAccess.Add(Neighbor);
				}
			}
		}
	}
	EstimatedSafePopulation = static_cast<int32>(FMath::Min<int64>(SafePopulation, MAX_int32));
}

void ASonHatEvacuationManager::RefreshPreparation()
{
	if (CheckDeadline() || !bHasStarted) { return; }
	EvaluateNetwork();
	OnStateChanged.Broadcast();
}

void ASonHatEvacuationManager::FinalizePreparation()
{
	if (!bHasStarted || bGameEnded) { return; }
	EvaluateNetwork();
	FinalSafePopulation = EstimatedSafePopulation;
	bGameEnded = true;
	GetWorld()->GetTimerManager().ClearTimer(PreparationUpdateTimer);
	OnStateChanged.Broadcast();
	OnRoundEnded.Broadcast(FinalSafePopulation, TotalPopulation);
}

void ASonHatEvacuationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PreparationUpdateTimer);
	}
	for (ASonHatEvacuationLink* Link : RegisteredLinks)
	{
		if (IsValid(Link) && Link->NetworkOwner.Get() == this)
		{
			Link->bActive = false;
			Link->NetworkOwner.Reset();
		}
	}
	Super::EndPlay(EndPlayReason);
}
