#include "SonHatEvacuationDirector.h"

#include "Blueprint/UserWidget.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GrappleComponent.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SonHatEvacuationDemoBootstrap.h"
#include "SonHatEvacuationHUDWidget.h"
#include "SonHatEvacuationLink.h"
#include "SonHatEvacuationNode.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSonHatEvacuationDirector, Log, All);

namespace SonHatEvacuationPresentation
{
	FString BuildingName(const ASonHatEvacuationNode* Node)
	{
		return !IsValid(Node) ? TEXT("Missing building")
			: Node->BuildingName.IsEmpty() ? Node->GetActorNameOrLabel() : Node->BuildingName.ToString();
	}

	FString LinkName(const ASonHatEvacuationLink* Link)
	{
		return !IsValid(Link) ? TEXT("Missing link") : !Link->ConnectionName.IsEmpty() ? Link->ConnectionName.ToString()
			: FString::Printf(TEXT("%s - %s"), *BuildingName(Link->NodeA), *BuildingName(Link->NodeB));
	}

	void SetLabel(UTextRenderComponent* Label, const FString& Text, const FVector& Position, const FVector& Camera, FColor Color)
	{
		if (Label->Text.ToString() != Text) { Label->SetText(FText::FromString(Text)); }
		Label->SetWorldLocation(Position);
		Label->SetWorldRotation((Camera - Position).Rotation());
		Label->SetTextRenderColor(Color);
		Label->SetVisibility(true);
	}

	constexpr int32 DashSegmentCount = 6;
}

ASonHatEvacuationDirector::ASonHatEvacuationDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FloodRevealOrigin")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BeamMesh.Succeeded()) { ConnectionBeamMesh = BeamMesh.Object; }
	if (SphereMesh.Succeeded()) { MarkerSphereMesh = SphereMesh.Object; }
	if (PlaneMesh.Succeeded()) { FloodPlaneMesh = PlaneMesh.Object; }
	if (BasicMaterial.Succeeded())
	{
		ConnectionLineMaterial = BasicMaterial.Object;
		MarkerMaterial = BasicMaterial.Object;
		FloodSurfaceMaterial = BasicMaterial.Object;
	}
}

void ASonHatEvacuationDirector::BeginPlay()
{
	Super::BeginPlay();
	EnsureHudWidget();
}

void ASonHatEvacuationDirector::EnsureHudWidget()
{
	if (!Controller.IsValid()) { RefreshController(); }
	if (HudWidget || !GetWorld()) { return; }
	APlayerController* PC = Controller.Get();
	if (PC && PC->GetLocalPlayer())
	{
		HudWidget = CreateWidget<USonHatEvacuationHUDWidget>(PC, USonHatEvacuationHUDWidget::StaticClass());
	}
	else
	{
		HudWidget = CreateWidget<USonHatEvacuationHUDWidget>(GetWorld(), USonHatEvacuationHUDWidget::StaticClass());
	}
	if (HudWidget) { HudWidget->AddToViewport(0); }
}

void ASonHatEvacuationDirector::RefreshController()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && !PC->IsLocalController()) { PC = nullptr; }
	if (PC != Controller.Get())
	{
		if (Controller.IsValid()) { RemoveTickPrerequisiteActor(Controller.Get()); }
		Controller = PC;
		if (PC) { AddTickPrerequisiteActor(PC); }
		if (HudWidget)
		{
			HudWidget->RemoveFromParent();
			HudWidget = nullptr;
		}
		EnsureHudWidget();
	}
}

void ASonHatEvacuationDirector::SetPhase(ESonHatEvacuationRoundPhase NewPhase)
{
	if (Phase == NewPhase) { return; }
	Phase = NewPhase;
	OnPhaseChanged.Broadcast(Phase);
}

void ASonHatEvacuationDirector::ConfigurationError(const FString& Message)
{
	StatusMessage = Message;
	StatusColor = FColor::Red;
	SetPhase(ESonHatEvacuationRoundPhase::ConfigurationError);
	UE_LOG(LogSonHatEvacuationDirector, Warning, TEXT("%s: %s"), *GetName(), *Message);
}

bool ASonHatEvacuationDirector::ResolveAndValidateNetwork()
{
	RoundNodes.Reset();
	RoundLinks.Reset();
	for (TActorIterator<ASonHatEvacuationDemoBootstrap> It(GetWorld()); It; ++It)
	{
		if (It->HasActorBegunPlay() && It->IsActorTickEnabled())
		{
			ConfigurationError(TEXT("Use the rooftop director OR the fallback bootstrap in a level, not both."));
			return false;
		}
	}
	if (bAutoDiscoverNetwork)
	{
		for (TActorIterator<ASonHatEvacuationNode> It(GetWorld()); It; ++It)
		{
			if (NetworkTag.IsNone() || It->ActorHasTag(NetworkTag)) { RoundNodes.Add(*It); }
		}
		for (TActorIterator<ASonHatEvacuationLink> It(GetWorld()); It; ++It)
		{
			if (NetworkTag.IsNone() || It->ActorHasTag(NetworkTag)) { RoundLinks.Add(*It); }
		}
		RoundNodes.Sort([](const ASonHatEvacuationNode& A, const ASonHatEvacuationNode& B) { return A.GetPathName() < B.GetPathName(); });
		RoundLinks.Sort([](const ASonHatEvacuationLink& A, const ASonHatEvacuationLink& B) { return A.GetPathName() < B.GetPathName(); });
	}
	else
	{
		for (ASonHatEvacuationNode* Node : Nodes) { RoundNodes.AddUnique(Node); }
		for (ASonHatEvacuationLink* Link : CandidateLinks) { RoundLinks.AddUnique(Link); }
	}
	int32 Hubs = 0;
	FBox Bounds(ForceInit);
	for (ASonHatEvacuationNode* Node : RoundNodes)
	{
		if (!IsValid(Node) || Node->GetWorld() != GetWorld() || Node->GetActorLocation().ContainsNaN())
		{
			ConfigurationError(TEXT("Invalid node reference. Check the director's network list."));
			return false;
		}
		Hubs += Node->bIsSafeHub ? 1 : 0;
		Bounds += Node->GetActorLocation();
	}
	if (RoundNodes.IsEmpty() || Hubs != 1 || RoundLinks.IsEmpty())
	{
		ConfigurationError(TEXT("Place nodes, exactly ONE safe hub, and candidate links. Check Network Tag if used."));
		return false;
	}
	for (ASonHatEvacuationLink* Link : RoundLinks)
	{
		if (!IsValid(Link) || Link->GetWorld() != GetWorld() || !Link->HasValidEndpoints()
			|| !RoundNodes.Contains(Link->NodeA) || !RoundNodes.Contains(Link->NodeB) || Link->GetActorLocation().ContainsNaN())
		{
			ConfigurationError(TEXT("A candidate has missing endpoints or endpoints outside this network. Fix its Node A / Node B."));
			return false;
		}
		for (TActorIterator<ASonHatEvacuationManager> It(GetWorld()); It; ++It)
		{
			if (It->OwnsLink(Link))
			{
				ConfigurationError(TEXT("Another manager already owns a level link. Use only the director's spawned manager for this network."));
				return false;
			}
		}
		Bounds += Link->GetActorLocation();
	}
	FloodCenter = bFitFloodToNetwork ? Bounds.GetCenter() : GetActorLocation();
	FloodCenter.Z = GetActorLocation().Z;
	FloodExtents = bFitFloodToNetwork ? FVector2D(Bounds.GetExtent().X + 1000, Bounds.GetExtent().Y + 1000) : FloodHalfSize;
	FloodExtents.X = FMath::IsFinite(FloodExtents.X) ? FMath::Clamp(FloodExtents.X, 100.0, 100000.0) : 6000.0;
	FloodExtents.Y = FMath::IsFinite(FloodExtents.Y) ? FMath::Clamp(FloodExtents.Y, 100.0, 100000.0) : 6000.0;
	return true;
}

void ASonHatEvacuationDirector::ApplyPlayerReplayReset()
{
	if (!bReturnPlayerToStartOnReplay || !Controller.IsValid()) { return; }
	APawn* Pawn = Controller->GetPawn();
	if (!IsValid(Pawn)) { return; }
	const FTransform Target = IsValid(ReplaySpawnAnchor) ? ReplaySpawnAnchor->GetActorTransform()
		: bHasCapturedReplayTransform ? CapturedReplayTransform : Pawn->GetActorTransform();
	if (UGrappleComponent* Grapple = Pawn->FindComponentByClass<UGrappleComponent>())
	{
		Grapple->StopGrapple();
	}
	Pawn->SetActorTransform(Target, false, nullptr, ETeleportType::TeleportPhysics);
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
	{
		RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

bool ASonHatEvacuationDirector::RestartRound()
{
	RefreshController();
	const bool bWasReplay = Phase == ESonHatEvacuationRoundPhase::Results || Phase == ESonHatEvacuationRoundPhase::ConfigurationError;
	bRoundRequested = true;
	if (IsValid(Manager))
	{
		Manager->OnRoundEnded.RemoveAll(this);
		Manager->Destroy();
		Manager = nullptr;
	}
	ClearPresentation();
	if (!Controller.IsValid() || !IsValid(Controller->GetPawn()))
	{
		StatusMessage = TEXT("Waiting for the level's existing local player pawn. Use Play, not Simulate.");
		SetPhase(ESonHatEvacuationRoundPhase::WaitingForPlayer);
		return false;
	}
	if (bWasReplay) { ApplyPlayerReplayReset(); }
	if (!ResolveAndValidateNetwork()) { return false; }
	if (!bHasCapturedReplayTransform && !IsValid(ReplaySpawnAnchor))
	{
		CapturedReplayTransform = Controller->GetPawn()->GetActorTransform();
		bHasCapturedReplayTransform = true;
	}
	const FTransform Transform(GetActorRotation(), GetActorLocation());
	Manager = GetWorld()->SpawnActorDeferred<ASonHatEvacuationManager>(ASonHatEvacuationManager::StaticClass(),
		Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Manager)
	{
		ConfigurationError(TEXT("Could not create round manager."));
		return false;
	}
	Manager->SetFlags(RF_Transient);
	Manager->bAutoStartPreparation = false;
	Manager->Nodes = RoundNodes;
	Manager->CandidateLinks = RoundLinks;
	Manager->MaxActivations = FMath::Max(0, ConnectionBudget);
	Manager->PreparationDuration = FMath::IsFinite(PreparationDuration) ? FMath::Max(1.0f, PreparationDuration) : 90.0f;
	Manager->InteractionDistance = FMath::IsFinite(InteractionDistance) ? FMath::Max(50.0f, InteractionDistance) : 275.0f;
	RoundRevealDuration = FMath::IsFinite(FloodRevealDuration) ? FMath::Max(0.0f, FloodRevealDuration) : 8.0f;
	Manager->OnRoundEnded.AddDynamic(this, &ASonHatEvacuationDirector::HandleRoundEnded);
	Manager->FinishSpawning(Transform);
	CreatePresentation();
	StatusMessage = TEXT("Reach rooftop terminals with your grapple. Read SAFE / UNSAFE, then press E.");
	StatusColor = FColor::White;
	SetPhase(ESonHatEvacuationRoundPhase::Preparing);
	Manager->StartPreparation();
	return true;
}

bool ASonHatEvacuationDirector::HasLineOfSight(const ASonHatEvacuationLink* Link) const
{
	if (!bRequireLineOfSight) { return true; }
	if (!Controller.IsValid() || !Controller->GetPawn()) { return false; }
	FVector Camera;
	FRotator Rotation;
	Controller->GetPlayerViewPoint(Camera, Rotation);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SonHatEvacuationTerminal), false, Controller->GetPawn());
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Camera, Link->GetActorLocation(), ECC_Visibility, Params)
		|| Hit.GetActor() == Link;
}

ASonHatEvacuationLink* ASonHatEvacuationDirector::ResolveFocusedLinkForTests(
	const TArray<TObjectPtr<ASonHatEvacuationLink>>& Links,
	const FVector& PawnLocation,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	float InInteractionDistance,
	float AimHalfAngleDegrees,
	TFunctionRef<bool(const ASonHatEvacuationLink*)> LineOfSightFn)
{
	ASonHatEvacuationLink* Best = nullptr;
	double BestDot = -1.0;
	const double RangeSq = FMath::Square(static_cast<double>(InInteractionDistance));
	const FVector ViewDir = ViewDirection.GetSafeNormal();
	const double MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(AimHalfAngleDegrees, 5.0f, 75.0f)));
	for (ASonHatEvacuationLink* Link : Links)
	{
		if (!IsValid(Link)) { continue; }
		if (FVector::DistSquared(PawnLocation, Link->GetActorLocation()) > RangeSq) { continue; }
		const FVector ToTerminal = Link->GetActorLocation() - ViewLocation;
		const double Dist = ToTerminal.Size();
		if (Dist < KINDA_SMALL_NUMBER) { continue; }
		const double Dot = FVector::DotProduct(ViewDir, ToTerminal / Dist);
		if (Dot < MinDot) { continue; }
		if (!LineOfSightFn(Link)) { continue; }
		if (!Best || Dot > BestDot) { Best = Link; BestDot = Dot; }
	}
	return Best;
}

ASonHatEvacuationLink* ASonHatEvacuationDirector::ResolveFocusedLink() const
{
	if (!IsValid(Manager) || !Controller.IsValid() || !IsValid(Controller->GetPawn())) { return nullptr; }
	FVector Camera;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(Camera, ViewRotation);
	const FVector ViewDirection = Controller->GetControlRotation().Vector().GetSafeNormal();
	return ResolveFocusedLinkForTests(RoundLinks, Controller->GetPawn()->GetActorLocation(), Camera, ViewDirection,
		Manager->InteractionDistance, TerminalAimHalfAngleDegrees, [this](const ASonHatEvacuationLink* Link) { return HasLineOfSight(Link); });
}

ASonHatEvacuationLink* ASonHatEvacuationDirector::GetFocusedLink() const
{
	return ResolveFocusedLink();
}

ESonHatEvacuationActivationResult ASonHatEvacuationDirector::Interact()
{
	using EResult = ESonHatEvacuationActivationResult;
	if (!IsValid(Manager)) { return EResult::NotStarted; }
	if (!Controller.IsValid() || !IsValid(Controller->GetPawn())) { return EResult::InvalidInteractor; }
	ASonHatEvacuationLink* Target = GetFocusedLink();
	const EResult Result = Manager->bGameEnded || Manager->GetRemainingTime() <= 0
		? Manager->TryActivateLink(nullptr, Controller->GetPawn())
		: !Target ? EResult::NoCandidateInRange : Manager->TryActivateLink(Target, Controller->GetPawn());
	StatusColor = Result == EResult::Success ? FColor::Cyan : FColor::Yellow;
	switch (Result)
	{
	case EResult::Success:
		StatusMessage = FString::Printf(TEXT("Prepared %s | %s | Safe access: %d/%d"),
			*SonHatEvacuationPresentation::LinkName(Target), Target->bFloodSafe ? TEXT("SAFE") : TEXT("UNSAFE: budget spent, will FAIL at flood"),
			Manager->EstimatedSafePopulation, Manager->TotalPopulation);
		if (!Target->bFloodSafe) { StatusColor = FColor::Orange; }
		break;
	case EResult::AlreadyActive: StatusMessage = TEXT("Already selected. No additional connection spent."); break;
	case EResult::BudgetExhausted: StatusMessage = TEXT("Connection budget spent. Review your network before the flood arrives."); break;
	case EResult::NoCandidateInRange:
	case EResult::TooFarAway: StatusMessage = TEXT("Aim at a visible rooftop terminal within range until the E prompt appears."); break;
	case EResult::PreparationEnded: StatusMessage = TEXT("Preparation ended. Routes are locked; inspect the flood outcome."); break;
	default: StatusMessage = FString::Printf(TEXT("Activation rejected: %s"), *UEnum::GetValueAsString(Result)); break;
	}
	UE_LOG(LogSonHatEvacuationDirector, Display, TEXT("%s"), *StatusMessage);
	return Result;
}

void ASonHatEvacuationDirector::HandleRoundEnded(int32 AccessPopulation, int32 TotalPopulation)
{
	RevealStartedAt = GetWorld()->GetTimeSeconds();
	StatusMessage = FString::Printf(TEXT("FLOOD ARRIVES: %d/%d residents have SAFE-ROUTE ACCESS. No NPC evacuation simulated."), AccessPopulation, TotalPopulation);
	StatusColor = FColor::Yellow;
	SetPhase(RoundRevealDuration > 0 ? ESonHatEvacuationRoundPhase::FloodReveal : ESonHatEvacuationRoundPhase::Results);
	UE_LOG(LogSonHatEvacuationDirector, Display, TEXT("%s"), *StatusMessage);
}

float ASonHatEvacuationDirector::GetFloodRevealProgress() const
{
	if (Phase == ESonHatEvacuationRoundPhase::Results) { return 1.0f; }
	return Phase == ESonHatEvacuationRoundPhase::FloodReveal && GetWorld() && RoundRevealDuration > 0
		? FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - RevealStartedAt) / RoundRevealDuration), 0.0f, 1.0f) : 0.0f;
}

void ASonHatEvacuationDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshController();
	if (Phase == ESonHatEvacuationRoundPhase::WaitingForPlayer && (bAutoStart || bRoundRequested)
		&& Controller.IsValid() && IsValid(Controller->GetPawn())) { RestartRound(); }
	if (Phase == ESonHatEvacuationRoundPhase::Preparing)
	{
		if (!IsValid(Manager)) { ConfigurationError(TEXT("Round manager was removed. R retries this level network.")); }
		else if (Manager->GetRemainingTime() <= 0 && !Manager->bGameEnded) { Manager->FinalizePreparation(); }
	}
	if (Phase == ESonHatEvacuationRoundPhase::FloodReveal && GetFloodRevealProgress() >= 1)
	{
		SetPhase(ESonHatEvacuationRoundPhase::Results);
	}
	if (Controller.IsValid())
	{
		if (Controller->WasInputKeyJustPressed(EKeys::R)
			&& (Phase == ESonHatEvacuationRoundPhase::Results || Phase == ESonHatEvacuationRoundPhase::ConfigurationError)) { RestartRound(); }
		else if (Phase == ESonHatEvacuationRoundPhase::Preparing && bAllowEarlyFinishForTesting
			&& Controller->WasInputKeyJustPressed(EKeys::F)) { Manager->FinalizePreparation(); }
		else if (Controller->WasInputKeyJustPressed(EKeys::E)) { Interact(); }
	}
	UpdatePresentation();
	UpdateHUD();
}

UTextRenderComponent* ASonHatEvacuationDirector::CreateLabel()
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	AddInstanceComponent(Label);
	Label->SetupAttachment(RootComponent);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(FMath::IsFinite(LabelWorldSize) ? FMath::Max(10.0f, LabelWorldSize) : 24.0f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->RegisterComponent();
	return Label;
}

USplineMeshComponent* ASonHatEvacuationDirector::CreateSplineSegment(const FName& Name)
{
	USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this, Name);
	AddInstanceComponent(Segment);
	Segment->SetupAttachment(RootComponent);
	Segment->SetMobility(EComponentMobility::Movable);
	Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Segment->SetCastShadow(false);
	if (ConnectionBeamMesh) { Segment->SetStaticMesh(ConnectionBeamMesh); }
	Segment->RegisterComponent();
	return Segment;
}

UStaticMeshComponent* ASonHatEvacuationDirector::CreateMarker(const FName& Name, float Radius)
{
	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this, Name);
	AddInstanceComponent(Marker);
	Marker->SetupAttachment(RootComponent);
	Marker->SetMobility(EComponentMobility::Movable);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCastShadow(false);
	if (MarkerSphereMesh) { Marker->SetStaticMesh(MarkerSphereMesh); }
	const float Diameter = FMath::Max(5.0f, Radius * 2.0f);
	Marker->SetWorldScale3D(FVector(Diameter / 100.0f));
	Marker->RegisterComponent();
	return Marker;
}

void ASonHatEvacuationDirector::EnsureMarkerMaterial(TObjectPtr<UMaterialInstanceDynamic>& Slot, const FLinearColor& Color)
{
	if (!Slot && MarkerMaterial) { Slot = UMaterialInstanceDynamic::Create(MarkerMaterial, this); }
	if (Slot) { Slot->SetVectorParameterValue(TEXT("Color"), Color); }
}

void ASonHatEvacuationDirector::SetSplineSegment(USplineMeshComponent* Segment, TObjectPtr<UMaterialInstanceDynamic>& MaterialSlot,
	const FVector& Start, const FVector& End, const FLinearColor& Color, bool bVisible)
{
	if (!Segment) { return; }
	Segment->SetVisibility(bVisible, true);
	if (!bVisible) { return; }
	const FVector Tangent = End - Start;
	Segment->SetStartAndEnd(Start, Tangent, End, Tangent);
	const float Radius = FMath::Max(0.5f, ConnectionBeamRadius);
	Segment->SetStartScale(FVector2D(Radius / 50.0f, Radius / 50.0f));
	Segment->SetEndScale(FVector2D(Radius / 50.0f, Radius / 50.0f));
	if (!MaterialSlot && ConnectionLineMaterial) { MaterialSlot = UMaterialInstanceDynamic::Create(ConnectionLineMaterial, this); }
	if (MaterialSlot)
	{
		MaterialSlot->SetVectorParameterValue(TEXT("Color"), Color);
		Segment->SetMaterial(0, MaterialSlot);
	}
}

void ASonHatEvacuationDirector::CreatePresentation()
{
	for (int32 Index = 0; Index < RoundNodes.Num(); ++Index)
	{
		NodeLabels.Add(CreateLabel());
		NodeMarkers.Add(CreateMarker(*FString::Printf(TEXT("NodeMarker_%d"), Index), NodeMarkerRadius));
		NodeMarkerMaterials.Add(nullptr);
	}
	for (int32 Index = 0; Index < RoundLinks.Num(); ++Index)
	{
		LinkLabels.Add(CreateLabel());
		FLinkPresentation Visual;
		Visual.SolidLine = CreateSplineSegment(*FString::Printf(TEXT("LinkSolid_%d"), Index));
		Visual.BrokenSegmentA = CreateSplineSegment(*FString::Printf(TEXT("LinkBreakA_%d"), Index));
		Visual.BrokenSegmentB = CreateSplineSegment(*FString::Printf(TEXT("LinkBreakB_%d"), Index));
		Visual.FailCrossA = CreateSplineSegment(*FString::Printf(TEXT("LinkCrossA_%d"), Index));
		Visual.FailCrossB = CreateSplineSegment(*FString::Printf(TEXT("LinkCrossB_%d"), Index));
		Visual.DashSegments.Reserve(SonHatEvacuationPresentation::DashSegmentCount);
		for (int32 DashIndex = 0; DashIndex < SonHatEvacuationPresentation::DashSegmentCount; ++DashIndex)
		{
			Visual.DashSegments.Add(CreateSplineSegment(*FString::Printf(TEXT("LinkDash_%d_%d"), Index, DashIndex)));
		}
		Visual.TerminalMarker = CreateMarker(*FString::Printf(TEXT("Terminal_%d"), Index), TerminalMarkerRadius);
		Visual.TerminalMaterial = nullptr;
		Visual.ConnectionMaterial = nullptr;
		LinkPresentation.Add(MoveTemp(Visual));
	}
	FloodPlane = CreateMarker(TEXT("FloodPlane"), 50.0f);
	if (FloodPlane && FloodPlaneMesh)
	{
		FloodPlane->SetStaticMesh(FloodPlaneMesh);
		FloodPlane->SetWorldScale3D(FVector(FloodExtents.X / 50.0f, FloodExtents.Y / 50.0f, 1.0f));
	}
	if (FloodSurfaceMaterial)
	{
		FloodMaterialInstance = UMaterialInstanceDynamic::Create(FloodSurfaceMaterial, this);
		if (FloodPlane && FloodMaterialInstance)
		{
			FloodPlane->SetMaterial(0, FloodMaterialInstance);
			FloodMaterialInstance->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.35f, 0.9f, 0.65f));
		}
	}
	if (FloodPlane) { FloodPlane->SetVisibility(false, true); }
}

void ASonHatEvacuationDirector::ClearPresentation()
{
	for (UTextRenderComponent* Label : NodeLabels) { if (IsValid(Label)) { RemoveInstanceComponent(Label); Label->DestroyComponent(); } }
	for (UTextRenderComponent* Label : LinkLabels) { if (IsValid(Label)) { RemoveInstanceComponent(Label); Label->DestroyComponent(); } }
	for (UStaticMeshComponent* Marker : NodeMarkers) { if (IsValid(Marker)) { RemoveInstanceComponent(Marker); Marker->DestroyComponent(); } }
	for (FLinkPresentation& Visual : LinkPresentation)
	{
		auto DestroyComp = [this](UActorComponent* Comp)
		{
			if (IsValid(Comp)) { RemoveInstanceComponent(Comp); Comp->DestroyComponent(); }
		};
		DestroyComp(Visual.SolidLine);
		DestroyComp(Visual.BrokenSegmentA);
		DestroyComp(Visual.BrokenSegmentB);
		DestroyComp(Visual.FailCrossA);
		DestroyComp(Visual.FailCrossB);
		for (USplineMeshComponent* Dash : Visual.DashSegments) { DestroyComp(Dash); }
		DestroyComp(Visual.TerminalMarker);
	}
	if (IsValid(FloodPlane)) { RemoveInstanceComponent(FloodPlane); FloodPlane->DestroyComponent(); }
	NodeLabels.Reset();
	LinkLabels.Reset();
	NodeMarkers.Reset();
	NodeMarkerMaterials.Reset();
	LinkPresentation.Reset();
	FloodPlane = nullptr;
	FloodMaterialInstance = nullptr;
}

void ASonHatEvacuationDirector::UpdateFloodVisual()
{
	if (!FloodPlane) { return; }
	const bool bShow = Phase == ESonHatEvacuationRoundPhase::FloodReveal || Phase == ESonHatEvacuationRoundPhase::Results;
	FloodPlane->SetVisibility(bShow, true);
	if (!bShow) { return; }
	const float Rise = FMath::IsFinite(FloodRiseDistance) ? FMath::Max(0.0f, FloodRiseDistance) : 800.0f;
	FVector Center = FloodCenter;
	Center.Z -= Rise * (1.0f - GetFloodRevealProgress());
	FloodPlane->SetWorldLocation(Center);
	FloodPlane->SetWorldRotation(FRotator(0, 0, 0));
}

void ASonHatEvacuationDirector::UpdatePresentation()
{
	using namespace SonHatEvacuationPresentation;
	if (!IsValid(Manager)) { return; }
	const bool bFlood = Manager->bGameEnded;
	FVector Camera = GetActorLocation();
	FRotator Rotation;
	if (Controller.IsValid()) { Controller->GetPlayerViewPoint(Camera, Rotation); }
	for (int32 Index = 0; Index < RoundNodes.Num(); ++Index)
	{
		ASonHatEvacuationNode* Node = RoundNodes[Index];
		if (!IsValid(Node))
		{
			NodeLabels[Index]->SetVisibility(false);
			NodeMarkers[Index]->SetVisibility(false);
			continue;
		}
		const bool bAccess = Manager->HasSafeRoute(Node);
		const FLinearColor Color = Node->bIsSafeHub ? ColorHub : bAccess ? ColorSafeAccess : bFlood ? ColorFloodNoAccess : ColorNoAccess;
		const FString Text = FString::Printf(TEXT("%s | %d residents\n%s"), *BuildingName(Node), FMath::Max(0, Node->CivilianCount),
			Node->bIsSafeHub ? TEXT("HIGH-GROUND SAFETY HUB") : bAccess ? TEXT("SAFE-ROUTE ACCESS") : TEXT("NO SAFE ROUTE TO HUB"));
		SetLabel(NodeLabels[Index], Text, Node->GetActorLocation() + FVector(0, 0, 180), Camera, Color.ToFColor(true));
		NodeMarkers[Index]->SetWorldLocation(Node->GetActorLocation());
		EnsureMarkerMaterial(NodeMarkerMaterials[Index], Color);
		if (NodeMarkerMaterials[Index]) { NodeMarkers[Index]->SetMaterial(0, NodeMarkerMaterials[Index]); }
		NodeMarkers[Index]->SetVisibility(true, true);
	}
	ASonHatEvacuationLink* Focused = GetFocusedLink();
	for (int32 Index = 0; Index < RoundLinks.Num(); ++Index)
	{
		ASonHatEvacuationLink* Link = RoundLinks[Index];
		FLinkPresentation& Visual = LinkPresentation[Index];
		if (!IsValid(Link))
		{
			LinkLabels[Index]->SetVisibility(false);
			if (Visual.TerminalMarker) { Visual.TerminalMarker->SetVisibility(false, true); }
			continue;
		}
		const bool bSelected = Link->IsActive();
		const bool bAccess = Link->HasValidEndpoints() && Manager->HasSafeRoute(Link->NodeA) && Manager->HasSafeRoute(Link->NodeB);
		FLinearColor Color = !Link->bFloodSafe ? ColorUnsafe : bSelected ? ColorSelectedSafe : ColorUnselectedSafe;
		if (bFlood && bSelected) { Color = !Link->bFloodSafe ? ColorFailed : bAccess ? ColorSafeAccess : ColorIntactNoHub; }
		const TCHAR* State = !bSelected ? TEXT("NOT SELECTED") : bFlood && !Link->bFloodSafe ? TEXT("FAILED AT FLOOD")
			: bFlood && !bAccess ? TEXT("INTACT, BUT NO HUB PATH") : bFlood ? TEXT("INTACT: SAFE ACCESS") : TEXT("SELECTED");
		SetLabel(LinkLabels[Index], FString::Printf(TEXT("%s\n%s | %s"), *LinkName(Link), Link->bFloodSafe ? TEXT("SAFE") : TEXT("UNSAFE"), State),
			Link->GetActorLocation() + FVector(0, 0, 110), Camera, Color.ToFColor(true));
		const float Radius = Link == Focused ? FocusedTerminalRadius : TerminalMarkerRadius;
		Visual.TerminalMarker->SetWorldLocation(Link->GetActorLocation());
		const float Diameter = FMath::Max(5.0f, Radius * 2.0f);
		Visual.TerminalMarker->SetWorldScale3D(FVector(Diameter / 100.0f));
		const FLinearColor MarkerColor = Link == Focused ? ColorFocusRing : Color;
		EnsureMarkerMaterial(Visual.TerminalMaterial, MarkerColor);
		if (Visual.TerminalMaterial) { Visual.TerminalMarker->SetMaterial(0, Visual.TerminalMaterial); }
		Visual.TerminalMarker->SetVisibility(true, true);
		if (!Link->HasValidEndpoints()) { continue; }
		const FVector From = Link->NodeA->GetActorLocation();
		const FVector To = Link->NodeB->GetActorLocation();
		const bool bShowBroken = bSelected && bFlood && !Link->bFloodSafe;
		SetSplineSegment(Visual.SolidLine, Visual.ConnectionMaterial, From, To, Color, bSelected && !bShowBroken);
		SetSplineSegment(Visual.BrokenSegmentA, Visual.ConnectionMaterial, From, FMath::Lerp(From, To, 0.4f), ColorFailed, bShowBroken);
		SetSplineSegment(Visual.BrokenSegmentB, Visual.ConnectionMaterial, FMath::Lerp(From, To, 0.6f), To, ColorFailed, bShowBroken);
		const FVector Mid = (From + To) * 0.5f;
		SetSplineSegment(Visual.FailCrossA, Visual.ConnectionMaterial, Mid + FVector(-70, 0, -70), Mid + FVector(70, 0, 70), ColorFailed, bShowBroken);
		SetSplineSegment(Visual.FailCrossB, Visual.ConnectionMaterial, Mid + FVector(-70, 0, 70), Mid + FVector(70, 0, -70), ColorFailed, bShowBroken);
		for (int32 DashIndex = 0; DashIndex < Visual.DashSegments.Num(); ++DashIndex)
		{
			const float StartAlpha = (DashIndex * 2.0f) / (DashSegmentCount * 2.0f);
			const float EndAlpha = (DashIndex * 2.0f + 1.0f) / (DashSegmentCount * 2.0f);
			SetSplineSegment(Visual.DashSegments[DashIndex], Visual.ConnectionMaterial, FMath::Lerp(From, To, StartAlpha), FMath::Lerp(From, To, EndAlpha), Color, !bSelected);
		}
	}
	UpdateFloodVisual();
}

void ASonHatEvacuationDirector::UpdateHUD()
{
	EnsureHudWidget();
	if (!HudWidget) { return; }
	HudLines.Reset();
	HudLineColors.Reset();
	const auto AddLine = [this](const FString& Text, const FLinearColor& Color)
	{
		HudLines.Add(Text);
		HudLineColors.Add(Color);
	};
	const TCHAR* PhaseName = Phase == ESonHatEvacuationRoundPhase::Preparing ? TEXT("PREPARE")
		: Phase == ESonHatEvacuationRoundPhase::FloodReveal ? TEXT("FLOOD REVEAL - ROUTES LOCKED")
		: Phase == ESonHatEvacuationRoundPhase::Results ? TEXT("RESULTS - R TO REPLAY")
		: Phase == ESonHatEvacuationRoundPhase::ConfigurationError ? TEXT("LEVEL SETUP ERROR - R TO RETRY") : TEXT("WAITING TO START");
	AddLine(FString::Printf(TEXT("SON HAT | %s"), PhaseName), FLinearColor(0, 1, 1));
	if (IsValid(Manager))
	{
		AddLine(FString::Printf(TEXT("Time %.1fs | Connections %d/%d | Safe-route access %d/%d residents"), Manager->GetRemainingTime(),
			Manager->GetRemainingConnections(), Manager->MaxActivations, Manager->EstimatedSafePopulation, Manager->TotalPopulation), FLinearColor::White);
		if (Manager->bGameEnded)
		{
			AddLine(FString::Printf(TEXT("FINAL ACCESS: %d/%d | WITHOUT ACCESS: %d | NPC evacuation NOT simulated"),
				Manager->FinalSafePopulation, Manager->TotalPopulation, Manager->TotalPopulation - Manager->FinalSafePopulation), FLinearColor(1, 1, 0));
		}
		else
		{
			ASonHatEvacuationLink* Target = GetFocusedLink();
			AddLine(Target ? FString::Printf(TEXT("[E] %s | %s | %s"), *SonHatEvacuationPresentation::LinkName(Target),
				Target->bFloodSafe ? TEXT("SAFE") : TEXT("UNSAFE: fails at flood"), Target->IsActive() ? TEXT("already selected") : TEXT("costs 1 connection"))
				: TEXT("Aim at a rooftop terminal within range. E appears when your crosshair is on target."),
				Target && !Target->bFloodSafe ? ColorUnsafe : FLinearColor::White);
		}
	}
	AddLine(StatusMessage, StatusColor.ReinterpretAsLinear());
	AddLine(TEXT("Dashed = not selected | Solid = selected | Orange/red = unsafe/fails"), FLinearColor(0.75f, 0.75f, 0.75f));
	AddLine(TEXT("Green buildings = safe access | Red buildings at flood = no safe hub route"), ColorSafeAccess);
	if (Phase == ESonHatEvacuationRoundPhase::Preparing)
	{
		AddLine(bAllowEarlyFinishForTesting ? TEXT("Existing grapple controls | E prepare | F test flood early")
			: TEXT("Use existing grapple controls to reach terminals before the timer expires."), FLinearColor::White);
	}
	else
	{
		AddLine(bReturnPlayerToStartOnReplay ? TEXT("Inspect routes and buildings. R replays and returns you to the round start.")
			: TEXT("Inspect routes and buildings. R replays after reveal, using your current position."), FLinearColor::White);
	}
	AddLine(TEXT("Fictional pre-installed network. Water is illustrative; safety is designer-authored."), FLinearColor(0.75f, 0.75f, 0.75f));
	HudWidget->SetHudLines(MoveTemp(HudLines), HudLineColors);
}

void ASonHatEvacuationDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Controller.IsValid()) { RemoveTickPrerequisiteActor(Controller.Get()); }
	if (HudWidget)
	{
		HudWidget->RemoveFromParent();
		HudWidget = nullptr;
	}
	if (IsValid(Manager))
	{
		Manager->OnRoundEnded.RemoveAll(this);
		Manager->Destroy();
	}
	ClearPresentation();
	Super::EndPlay(EndPlayReason);
}
