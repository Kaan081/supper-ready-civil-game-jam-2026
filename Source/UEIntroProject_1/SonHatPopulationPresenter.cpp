#include "SonHatPopulationPresenter.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SonHatEvacuationDirector.h"
#include "SonHatEvacuationManager.h"
#include "SonHatEvacuationNode.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	float Setting(float Value, float Fallback, float Minimum = 0)
	{
		return FMath::IsFinite(Value) ? FMath::Max(Minimum, Value) : Fallback;
	}
	FString BuildingName(const ASonHatEvacuationNode* Node)
	{
		if (!Node->BuildingName.IsEmpty()) { return Node->BuildingName.ToString(); }
		FString Label = Node->GetActorNameOrLabel();
		Label.RemoveFromStart(TEXT("Node_"));
		return Label;
	}
}

ASonHatPopulationPresenter::ASonHatPopulationPresenter()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PopulationPresentation")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Color(TEXT("/Engine/EngineDebugMaterials/LevelColorationUnlitMaterial.LevelColorationUnlitMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Text(TEXT("/Engine/EngineMaterials/UnlitText.UnlitText"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Waiting(TEXT("/Game/Jam/A/Population/SM_Citizen_Waiting.SM_Citizen_Waiting"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Safe(TEXT("/Game/Jam/A/Population/SM_Citizen_Safe.SM_Citizen_Safe"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Threat(TEXT("/Game/Jam/A/Population/SM_Citizen_Threat.SM_Citizen_Threat"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Stranded(TEXT("/Game/Jam/A/Population/SM_Citizen_Stranded.SM_Citizen_Stranded"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Human(TEXT("/Game/Jam/A/Population/M_CitizenState.M_CitizenState"));
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
	PopulationMaterial = Color.Object;
	TextMaterial = Text.Object;
	HumanWaitingMesh = Waiting.Object;
	HumanSafeMesh = Safe.Object;
	HumanThreatMesh = Threat.Object;
	HumanStrandedMesh = Stranded.Object;
	HumanMaterial = Human.Object;
}

void ASonHatPopulationPresenter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		if (bPreviewInEditor) { RebuildPresentation(); }
		else { ClearGroups(); }
	}
}

void ASonHatPopulationPresenter::BeginPlay()
{
	Super::BeginPlay();
	RebuildPresentation();
	// One batched presentation timer, with immediate manager-event state changes.
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ASonHatPopulationPresenter::RefreshPresentation, bAnimateRepresentatives ? 0.05f : 0.2f, true);
}

void ASonHatPopulationPresenter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(RefreshTimer); }
	BindManager(nullptr);
	ClearGroups();
	Super::EndPlay(Reason);
}

void ASonHatPopulationPresenter::BindManager(ASonHatEvacuationManager* NewManager)
{
	if (BoundManager.Get() == NewManager && (NewManager || !bGroupsFromStartedManager)) { return; }
	if (ASonHatEvacuationManager* Old = BoundManager.Get()) { Old->OnStateChanged.RemoveDynamic(this, &ASonHatPopulationPresenter::HandleNetworkChanged); }
	BoundManager = NewManager;
	AnimationStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0;
	bGroupsFromStartedManager = false;
	bFinalSnapshot = false;
	ClearGroups();
	if (IsValid(NewManager) && GetWorld()->IsGameWorld())
	{
		NewManager->OnStateChanged.AddUniqueDynamic(this, &ASonHatPopulationPresenter::HandleNetworkChanged);
	}
}

void ASonHatPopulationPresenter::ResolveSource()
{
	ObservedDirector.Reset();
	bAmbiguousSource = false;
	if (IsValid(Director) && Director->GetWorld() == GetWorld()) { ObservedDirector = Director; }
	else if (!IsValid(StandaloneManager) && bAutoFindSource)
	{
		for (TActorIterator<ASonHatEvacuationDirector> It(GetWorld()); It; ++It)
		{
			if (!IsValid(*It) || (!NetworkTag.IsNone() && It->NetworkTag != NetworkTag)) { continue; }
			if (ObservedDirector.IsValid()) { bAmbiguousSource = true; ObservedDirector.Reset(); break; }
			ObservedDirector = *It;
		}
	}
	ASonHatEvacuationManager* Manager = nullptr;
	if (!bAmbiguousSource)
	{
		Manager = ObservedDirector.IsValid() ? ObservedDirector->GetRoundManager() : StandaloneManager.Get();
		if (!Manager && !ObservedDirector.IsValid() && bAutoFindSource && NetworkTag.IsNone())
		{
			for (TActorIterator<ASonHatEvacuationManager> It(GetWorld()); It; ++It)
			{
				if (!IsValid(*It)) { continue; }
				if (Manager) { bAmbiguousSource = true; Manager = nullptr; break; }
				Manager = *It;
			}
		}
	}
	if (!IsValid(Manager) || Manager->GetWorld() != GetWorld()) { Manager = nullptr; }
	BindManager(Manager);
}

UInstancedStaticMeshComponent* ASonHatPopulationPresenter::CreateInstances(UStaticMesh* Mesh)
{
	auto* Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
	Component->ComponentTags.Add(TEXT("SonHatPopulationVisual"));
	AddInstanceComponent(Component);
	Component->SetupAttachment(RootComponent);
	Component->SetAbsolute(true, true, true);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);
	Component->RegisterComponent();
	Component->SetWorldTransform(FTransform::Identity);
	return Component;
}

UTextRenderComponent* ASonHatPopulationPresenter::CreateLabel()
{
	auto* Label = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
	Label->ComponentTags.Add(TEXT("SonHatPopulationVisual"));
	AddInstanceComponent(Label);
	Label->SetupAttachment(RootComponent);
	Label->SetAbsolute(true, true, true);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(Setting(LabelWorldSize, 20, 10));
	Label->SetTextMaterial(TextMaterial);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetCanEverAffectNavigation(false);
	Label->SetCastShadow(false);
	Label->RegisterComponent();
	return Label;
}

void ASonHatPopulationPresenter::ClearGroups()
{
	const auto Destroy = [this](UActorComponent* Component)
	{
		if (IsValid(Component) && Component->GetOwner() == this) { RemoveInstanceComponent(Component); Component->DestroyComponent(); }
	};
	for (auto& Group : Groups) { Destroy(Group.Bodies); Destroy(Group.Heads); Destroy(Group.Label); }
	Groups.Reset();
	Destroy(NetworkBoard);
	NetworkBoard = nullptr;
	LastBoardText.Reset();
	// PIE duplication/construction reruns can lose transient bookkeeping before BeginPlay.
	// Clean only our tagged visuals, including any orphaned editor-preview components.
	TArray<UActorComponent*> PopulationComponents;
	GetComponents(PopulationComponents);
	for (UActorComponent* Component : PopulationComponents)
	{
		if (IsValid(Component) && Component->ComponentHasTag(TEXT("SonHatPopulationVisual"))) { Destroy(Component); }
	}
}

void ASonHatPopulationPresenter::BuildGroups()
{
	ClearGroups();
	TArray<ASonHatEvacuationNode*> Nodes;
	ASonHatEvacuationManager* Manager = BoundManager.Get();
	if (Manager && Manager->bHasStarted)
	{
		for (ASonHatEvacuationNode* Node : Manager->Nodes)
		{
			if (IsValid(Node) && Node->GetWorld() == GetWorld()) { Nodes.AddUnique(Node); }
		}
		bGroupsFromStartedManager = true;
	}
	else if (!bAmbiguousSource && !GetWorld()->IsGameWorld())
	{
		// Editor preview only. Runtime groups always come from the authoritative manager's node list.
		ASonHatEvacuationDirector* Source = ObservedDirector.Get();
		if (Source && !Source->bAutoDiscoverNetwork)
		{
			for (ASonHatEvacuationNode* Node : Source->Nodes) { if (IsValid(Node)) { Nodes.AddUnique(Node); } }
		}
		else if (Source || !NetworkTag.IsNone())
		{
			const FName Tag = Source ? Source->NetworkTag : NetworkTag;
			for (TActorIterator<ASonHatEvacuationNode> It(GetWorld()); It; ++It)
			{
				if (IsValid(*It) && (Tag.IsNone() || It->ActorHasTag(Tag))) { Nodes.AddUnique(*It); }
			}
		}
	}
	Nodes.Sort([](const ASonHatEvacuationNode& A, const ASonHatEvacuationNode& B) { return A.GetPathName() < B.GetPathName(); });
	int32 Budget = FMath::Clamp(MaxTotalRepresentatives, 1, 256);
	bool bMissingFloor = false;
	for (ASonHatEvacuationNode* Node : Nodes)
	{
		if (Node->CivilianCount <= 0) { continue; }
		auto& Group = Groups.AddDefaulted_GetRef();
		Group.Status.Node = Node;
		Group.Status.Residents = FMath::Max(0, Node->CivilianCount);
		const auto* Placement = BuildingPlacements.FindByPredicate([Node](const FSonHatPopulationPlacement& P) { return P.Node == Node; });
		ASonHatEvacuationDirector* Source = ObservedDirector.Get();
		const FSonHatRescueGroup* Rescue = Source && Source->bRescueSignalMode
			? Source->RescueGroups.FindByPredicate([Node](const FSonHatRescueGroup& G) { return G.PopulationNode == Node; }) : nullptr;
		const AActor* Anchor = Rescue && IsValid(Rescue->PopulationAnchor) ? Rescue->PopulationAnchor.Get()
			: Placement && IsValid(Placement->Anchor) ? Placement->Anchor.Get() : Node;
		const FVector Offset = Rescue ? FVector::ZeroVector : Placement ? Placement->Offset : DefaultGroupOffset;
		const FQuat Rotation = Anchor->GetActorQuat() * FQuat(FVector::UpVector, FMath::DegreesToRadians(Rescue ? 0 : Placement ? Placement->FacingYaw : 0));
		const FVector Origin = Anchor->GetActorLocation() + Anchor->GetActorQuat().RotateVector(Offset);
		const int32 Desired = !Rescue && Placement && Placement->Representatives >= 0 ? Placement->Representatives : RepresentativesPerBuilding;
		const int32 Count = FMath::Min3(FMath::Clamp(Desired, 0, 16), Group.Status.Residents, Budget);
		const int32 Cols = FMath::Clamp(Columns, 1, 8);
		const int32 Rows = FMath::DivideAndRoundUp(Count, Cols);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Local((Index / Cols - (Rows - 1) * 0.5f) * Setting(Spacing, 90, 40),
				(Index % Cols - (FMath::Min(Cols, Count) - 1) * 0.5f) * Setting(Spacing, 90, 40), 0);
			if (Rescue && (FMath::Abs(Local.X) + 25.f > Rescue->PopulationHalfExtents.X || FMath::Abs(Local.Y) + 25.f > Rescue->PopulationHalfExtents.Y)) { bMissingFloor = true; continue; }
			FVector Foot = Origin + Rotation.RotateVector(Local);
			if (bProjectFeetToFloor)
			{
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(PopulationFloor), false, this);
				if (!GetWorld()->LineTraceSingleByObjectType(Hit, Foot + FVector(0, 0, Setting(ProjectionAbove, 300)),
					Foot - FVector(0, 0, Setting(ProjectionBelow, 2500, 1)), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
					|| Hit.ImpactNormal.Z < 0.65f || (Rescue && FMath::Abs(Hit.ImpactPoint.Z - Rescue->FloodClosureElevation) > 50.f))
				{
					bMissingFloor = true;
					continue; // Never silently place human proxies in midair or on walls.
				}
				Foot = Hit.ImpactPoint + FVector(0, 0, 2);
			}
			Group.People.Add(FTransform(Rotation, Foot, FVector(FMath::Clamp(Setting(ProxyHeight, 170, 50), 50.f, 300.f) / 170.f)));
		}
		Group.Status.VisualRepresentatives = Group.People.Num();
		Budget -= Group.People.Num();
		Group.Bodies = CreateInstances(RepresentativeMesh ? RepresentativeMesh.Get() : HumanWaitingMesh ? HumanWaitingMesh.Get() : CylinderMesh.Get());
		if (!RepresentativeMesh && !HumanWaitingMesh) { Group.Heads = CreateInstances(SphereMesh); }
		Group.Label = CreateLabel();
		const FVector LabelOrigin = Group.People.IsEmpty() ? Origin : Group.People[0].GetLocation();
		Group.Label->SetWorldLocation(LabelOrigin + FVector(0, 0, Setting(LabelHeight, 220)));
		UMaterialInterface* Material = !RepresentativeMesh && HumanWaitingMesh && HumanMaterial ? HumanMaterial.Get() : PopulationMaterial.Get();
		Group.Material = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
		if (Group.Material)
		{
			Group.Material->SetFlags(RF_Transient | RF_DuplicateTransient);
			Group.Bodies->SetMaterial(0, Group.Material);
			if (Group.Heads) { Group.Heads->SetMaterial(0, Group.Material); }
		}
	}
	if (bShowNetworkBoard) { NetworkBoard = CreateLabel(); }
	StatusMessage = bMissingFloor ? TEXT("Some figures have no WorldStatic floor. Set an anchor/offset, or disable floor projection and place feet explicitly.")
		: TEXT("Representative figures only. Route access comes from the evacuation manager; no individual escape/death simulation.");
}

void ASonHatPopulationPresenter::BuildPose(FSonHatPopulationGroupVisual& Group)
{
	Group.Bodies->ClearInstances();
	if (Group.Heads) { Group.Heads->ClearInstances(); }
	const bool bHumans = !RepresentativeMesh && HumanWaitingMesh;
	if (bHumans)
	{
		Group.Bodies->SetStaticMesh(SelectHumanMesh(Group.Status.State));
		if (Group.Material) { Group.Bodies->SetMaterial(0, Group.Material); }
	}
	for (const FTransform& Person : Group.People)
	{
		if (RepresentativeMesh || bHumans) { Group.Bodies->AddInstance(Person, true); continue; }
		const auto Limb = [&](FVector From, FVector To, float Radius)
		{
			const FVector Delta = To - From;
			const FTransform Part(FQuat::FindBetweenNormals(FVector::UpVector, Delta.GetSafeNormal()), (From + To) * 0.5,
				FVector(Radius / 50, Radius / 50, Delta.Size() / 100));
			Group.Bodies->AddInstance(Part * Person, true);
		};
		Limb(FVector(0, -10, 0), FVector(0, -10, 53), 7);
		Limb(FVector(0, 10, 0), FVector(0, 10, 53), 7);
		Limb(FVector(0, 0, 50), FVector(0, 0, 120), 19);
		const bool bHelp = Group.Status.State == ESonHatPopulationVisualState::FloodThreatened;
		const bool bStranded = Group.Status.State == ESonHatPopulationVisualState::Stranded;
		const bool bAccess = Group.Status.State == ESonHatPopulationVisualState::SafeAccess;
		Limb(FVector(0, -23, 108), bHelp ? FVector(0, -48, 154) : bStranded ? FVector(0, -58, 108) : FVector(8, -28, 67), 6);
		Limb(FVector(0, 23, 108), bHelp || bAccess ? FVector(0, 48, 154) : bStranded ? FVector(0, 58, 108) : FVector(8, 28, 67), 6);
		Group.Heads->AddInstance(FTransform(FQuat::Identity, FVector(0, 0, 142), FVector(0.34f)) * Person, true);
	}
	Group.bPoseBuilt = true;
	Group.LastAnimatedAt = -1;
}

UStaticMesh* ASonHatPopulationPresenter::SelectHumanMesh(ESonHatPopulationVisualState State) const
{
	switch (State)
	{
	case ESonHatPopulationVisualState::SafeAccess: return HumanSafeMesh ? HumanSafeMesh.Get() : HumanWaitingMesh.Get();
	case ESonHatPopulationVisualState::FloodThreatened: return HumanThreatMesh ? HumanThreatMesh.Get() : HumanWaitingMesh.Get();
	case ESonHatPopulationVisualState::Stranded: return HumanStrandedMesh ? HumanStrandedMesh.Get() : HumanWaitingMesh.Get();
	default: return HumanWaitingMesh;
	}
}

void ASonHatPopulationPresenter::AnimateGroup(FSonHatPopulationGroupVisual& Group, double Now)
{
	if (!bAnimateRepresentatives || Group.People.IsEmpty() || (!RepresentativeMesh && !HumanWaitingMesh) || !GetWorld()->IsGameWorld()) { return; }
	if (Group.LastAnimatedAt >= 0 && Now - Group.LastAnimatedAt < 0.045) { return; }
	Group.LastAnimatedAt = Now;
	TArray<FTransform, TInlineAllocator<16>> Transforms;
	for (int32 Index = 0; Index < Group.People.Num(); ++Index)
	{
		const double Time = Now - AnimationStartedAt;
		const double Phase = Index * 2.39996 + Group.Status.Residents * 0.17;
		const bool bThreat = Group.Status.State == ESonHatPopulationVisualState::FloodThreatened;
		const bool bStranded = Group.Status.State == ESonHatPopulationVisualState::Stranded;
		const double ReactionAge = Now - Group.StateChangedAt;
		const double Reaction = Group.Status.State == ESonHatPopulationVisualState::SafeAccess
			? FMath::Max(0.0, 1.0 - ReactionAge / Setting(SafeReactionDuration, 1.25f, 0.1f)) : 0;
		const double Wave = FMath::Sin(Time * (bThreat ? 5.0 : bStranded ? 0.8 : 1.4) + Phase);
		FTransform Transform = Group.People[Index];
		// Yaw and breathing scale about the authored feet origin: no hop, drift or floating feet.
		Transform.SetRotation(Transform.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(Wave * (bThreat ? 6.0 : bStranded ? 1.5 : 3.0) + Reaction * 5.0 * FMath::Sin(ReactionAge * 9 + Phase))));
		Transform.SetScale3D(Transform.GetScale3D() * FVector(1 + Wave * 0.008, 1 + Wave * 0.008, 1 + Wave * 0.003 + Reaction * 0.025));
		Transforms.Add(Transform);
	}
	Group.Bodies->BatchUpdateInstancesTransforms(0, TArrayView<const FTransform>(Transforms.GetData(), Transforms.Num()), true, true, true);
}

FLinearColor ASonHatPopulationPresenter::StateColor(ESonHatPopulationVisualState State) const
{
	switch (State)
	{
	case ESonHatPopulationVisualState::Waiting: return WaitingColor;
	case ESonHatPopulationVisualState::SafeAccess: return SafeAccessColor;
	case ESonHatPopulationVisualState::FloodThreatened: return ThreatColor;
	case ESonHatPopulationVisualState::Stranded: return StrandedColor;
	default: return FLinearColor(0.35f, 0.35f, 0.35f);
	}
}

void ASonHatPopulationPresenter::HandleNetworkChanged() { RefreshPresentation(); }

void ASonHatPopulationPresenter::RebuildPresentation()
{
	if (!GetWorld() || IsTemplate()) { return; }
	ResolveSource();
	bFinalSnapshot = false;
	AnimationStartedAt = GetWorld()->GetTimeSeconds();
	BuildGroups();
	RefreshPresentation();
}

void ASonHatPopulationPresenter::RefreshPresentation()
{
	if (!GetWorld() || bRefreshing || IsTemplate()) { return; }
	TGuardValue<bool> Guard(bRefreshing, true);
	ResolveSource();
	ASonHatEvacuationManager* Manager = BoundManager.Get();
	if (Manager && Manager->bHasStarted && !bGroupsFromStartedManager) { BuildGroups(); }
	const bool bReady = Manager && Manager->bHasStarted && Manager->ConfigurationStatus == ESonHatEvacuationConfiguration::Ready;
	const bool bFinal = bReady && Manager->bGameEnded;
	ASonHatEvacuationDirector* Source = ObservedDirector.Get();
	const bool bRescue = Source && Source->bRescueSignalMode;
	const bool bWaterVisible = Source && ((Source->Phase == ESonHatEvacuationRoundPhase::Preparing && Source->bEnableRisingFlood)
		|| Source->Phase == ESonHatEvacuationRoundPhase::FloodReveal || Source->Phase == ESonHatEvacuationRoundPhase::Results);
	const float Water = bWaterVisible ? Source->GetFloodHeight() : -MAX_flt;
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	FVector Eye = FVector::ZeroVector;
	FRotator View;
	if (PC) { PC->GetPlayerViewPoint(Eye, View); }
	for (auto& Group : Groups)
	{
		ASonHatEvacuationNode* Node = Group.Status.Node;
		if (!IsValid(Node))
		{
			Group.Bodies->SetVisibility(false, true); if (Group.Heads) { Group.Heads->SetVisibility(false, true); } Group.Label->SetVisibility(false);
			continue;
		}
		const auto PreviousState = Group.Status.State;
		const float FootZ = Group.People.IsEmpty() ? Group.Label->GetComponentLocation().Z - Setting(LabelHeight, 220) : Group.People[0].GetLocation().Z;
		Group.Status.bFloodThreatened = bWaterVisible && Water >= FootZ - Setting(FloodWarningClearance, 250);
		if (!bFinalSnapshot)
		{
			Group.Status.Residents = FMath::Max(0, Node->CivilianCount);
			Group.Status.bHasSafeAccess = bReady && Manager->HasSafeRoute(Node);
			Group.Status.State = !bReady ? ESonHatPopulationVisualState::Unavailable
				: Group.Status.bHasSafeAccess ? ESonHatPopulationVisualState::SafeAccess
				: bFinal ? ESonHatPopulationVisualState::Stranded
				: Group.Status.bFloodThreatened ? ESonHatPopulationVisualState::FloodThreatened : ESonHatPopulationVisualState::Waiting;
		}
		if (!Group.bPoseBuilt || PreviousState != Group.Status.State)
		{
			Group.StateChangedAt = GetWorld()->GetTimeSeconds();
			BuildPose(Group);
		}
		if (!PC || FVector::DistSquared(Eye, Group.Label->GetComponentLocation()) <= FMath::Square(Setting(AnimationDistance, 4500, 100)))
		{
			AnimateGroup(Group, GetWorld()->GetTimeSeconds());
		}
		FLinearColor Color = StateColor(Group.Status.State);
		if (Group.Status.State == ESonHatPopulationVisualState::FloodThreatened)
		{
			Color *= FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.0) < 0.5 ? 1.0f : 0.45f;
		}
		if (Group.Material) { Group.Material->SetVectorParameterValue(ColorParameter, Color); }
		FString State;
		switch (Group.Status.State)
		{
		case ESonHatPopulationVisualState::Waiting: State = TEXT("Waiting"); break;
		case ESonHatPopulationVisualState::SafeAccess: State = bFinal ? TEXT("Safe route") : TEXT("Safe route"); break;
		case ESonHatPopulationVisualState::FloodThreatened: State = TEXT("Flood threat"); break;
		case ESonHatPopulationVisualState::Stranded: State = TEXT("No safe route"); break;
		default: State = TEXT("…"); break;
		}
		if (Group.Status.bFloodThreatened && Group.Status.bHasSafeAccess) { State = TEXT("Safe route (flood nearby)"); }
		const bool bShowLabel = bShowBuildingWorldLabels
			|| Group.Status.State == ESonHatPopulationVisualState::FloodThreatened
			|| Group.Status.State == ESonHatPopulationVisualState::Stranded
			|| bFinal;
		if (!bShowLabel)
		{
			Group.Label->SetVisibility(false);
			continue;
		}
		const FString RoofId = bRescue ? Source->GetRescueGroupId(Node).ToString() : BuildingName(Node);
		const FString Text = FString::Printf(TEXT("ROOF %s — %d %s\n%s"), *RoofId, Group.Status.Residents, bRescue ? TEXT("PEOPLE") : TEXT("RESIDENTS"),
			bRescue ? (Group.Status.bHasSafeAccess ? TEXT("RESCUE SIGNAL RECEIVED") : bFinal ? TEXT("NOT REGISTERED FOR RESCUE")
				: Group.Status.bFloodThreatened ? TEXT("FLOOD APPROACHING — SEND SIGNAL") : TEXT("AWAITING RESCUE SIGNAL"))
			: Group.Status.bHasSafeAccess ? TEXT("SAFE ROUTE TO HIGH GROUND") : bFinal ? TEXT("NO SAFE ROUTE") : TEXT("NEEDS A SAFE ROUTE"));
		FVector LabelPosition = Group.Label->GetComponentLocation();
		LabelPosition.Z = FMath::Max(FootZ + Setting(LabelHeight, 220), bWaterVisible ? Water + 80 : -MAX_flt);
		if (PC && FVector::DistSquared(Eye, LabelPosition) > FMath::Square(6500.f))
		{
			Group.Label->SetVisibility(false);
			continue;
		}
		if (Text != Group.LastLabel) { Group.Label->SetText(FText::FromString(Text)); Group.LastLabel = Text; }
		Group.Label->SetTextRenderColor(Color.ToFColor(true));
		Group.Label->SetWorldLocation(LabelPosition);
		Group.Label->SetVisibility(true);
		if (PC) { Group.Label->SetWorldRotation((Eye - LabelPosition).Rotation()); }
	}
	if (bFinal) { bFinalSnapshot = true; }
	if (!bReady)
	{
		NetworkSummary = bAmbiguousSource ? TEXT("POPULATION: AMBIGUOUS NETWORK\nAssign one Director or standalone Manager.")
			: TEXT("POPULATION: WAITING FOR A VALID ROUND\nRoute access is not yet evaluated.");
	}
	else
	{
		const int32 Safe = FMath::Clamp(bFinal ? Manager->FinalSafePopulation : Manager->EstimatedSafePopulation, 0, Manager->TotalPopulation);
		NetworkSummary = FString::Printf(TEXT("%s\nSAFE-ROUTE ACCESS: %d / %d residents\n%s: %d residents\nRoute access only - no individual rescue/death simulation"),
			bFinal ? TEXT("FINAL POPULATION RESULT") : TEXT("RESIDENTS NEED YOUR CONNECTIONS"), Safe, Manager->TotalPopulation,
			bFinal ? TEXT("LACKED SAFE-ROUTE ACCESS") : TEXT("STILL NEED SAFE ACCESS"), Manager->TotalPopulation - Safe);
		if (bRescue)
		{
			NetworkSummary = FString::Printf(TEXT("%s\nREGISTERED FOR RESCUE: %d / %d\nPEOPLE NOT REACHED: %d\nSignal registration only; no physical evacuation simulated"),
				bFinal ? TEXT("RESCUE OPERATION COMPLETE") : TEXT("ROOFTOP RESCUE SIGNALS"), Safe, Manager->TotalPopulation, Manager->TotalPopulation - Safe);
		}
	}
	if (NetworkBoard)
	{
		if (LastBoardText != NetworkSummary) { NetworkBoard->SetText(FText::FromString(NetworkSummary)); LastBoardText = NetworkSummary; }
		NetworkBoard->SetTextRenderColor(FColor::White);
		FVector Position = GetActorTransform().TransformPosition(BoardOffset);
		if (bWaterVisible) { Position.Z = FMath::Max(Position.Z, Water + 100); }
		NetworkBoard->SetWorldLocation(Position);
		if (PC) { NetworkBoard->SetWorldRotation((Eye - Position).Rotation()); }
	}
}

TArray<FSonHatPopulationBuildingStatus> ASonHatPopulationPresenter::GetBuildingStatuses() const
{
	TArray<FSonHatPopulationBuildingStatus> Result;
	for (const auto& Group : Groups) { if (IsValid(Group.Status.Node)) { Result.Add(Group.Status); } }
	return Result;
}
