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
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GrappleComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SonHatEvacuationDemoBootstrap.h"
#include "SonHatEvacuationHUDWidget.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
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

	FString PlayerBuildingName(const ASonHatEvacuationNode* Node)
	{
		if (!IsValid(Node)) { return TEXT("Building"); }
		if (Node->bIsSafeHub) { return TEXT("High ground"); }
		FString Name = BuildingName(Node);
		Name.RemoveFromStart(TEXT("Node_"));
		Name.RemoveFromStart(TEXT("Building "));
		return TEXT("Roof ") + Name;
	}

	FString PlayerLinkName(const ASonHatEvacuationLink* Link)
	{
		if (!IsValid(Link)) { return TEXT("Terminal"); }
		return Link->HasValidEndpoints() ? PlayerBuildingName(Link->NodeA) + TEXT(" - ") + PlayerBuildingName(Link->NodeB)
			: Link->ConnectionName.ToString();
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConsoleMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RouteMaterial(
		TEXT("/Game/Jam/A/Materials/M_SonHatRouteSpline.M_SonHatRouteSpline"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BeamMesh.Succeeded()) { ConnectionBeamMesh = BeamMesh.Object; }
	if (SphereMesh.Succeeded()) { MarkerSphereMesh = SphereMesh.Object; }
	if (PlaneMesh.Succeeded()) { FloodPlaneMesh = PlaneMesh.Object; }
	TerminalConsoleMesh = ConsoleMesh.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Housing(TEXT("/Game/Jam/A/Materials/M_EmergencyConsoleBody.M_EmergencyConsoleBody"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Trim(TEXT("/Game/Jam/A/Materials/M_EmergencyConsoleTrim.M_EmergencyConsoleTrim"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Screen(TEXT("/Game/Jam/A/Materials/M_EmergencyConsoleScreen.M_EmergencyConsoleScreen"));
	ConsoleHousingMaterial = Housing.Object; ConsoleTrimMaterial = Trim.Object; ConsoleDisplayMaterial = Screen.Object;
	if (RouteMaterial.Succeeded()) { ConnectionLineMaterial = RouteMaterial.Object; }
	else if (BasicMaterial.Succeeded()) { ConnectionLineMaterial = BasicMaterial.Object; }
	if (BasicMaterial.Succeeded())
	{
		MarkerMaterial = BasicMaterial.Object;
		FloodSurfaceMaterial = BasicMaterial.Object;
	}
}

void ASonHatEvacuationDirector::BeginPlay()
{
	Super::BeginPlay();
	ResolveDefaultRoundSounds();
	SuppressGrappleAnchorCosmetics();
	EnsureHudWidget();
}

void ASonHatEvacuationDirector::SuppressGrappleAnchorCosmetics()
{
	if (!GetWorld() || !GetWorld()->IsGameWorld()) { return; }
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || !Actor->ActorHasTag(TEXT("GrappleAnchor"))) { continue; }
		TArray<UStaticMeshComponent*> Meshes;
		Actor->GetComponents(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh)) { continue; }
			// Legacy AnchorMesh is decorative. Tag alone can also be on a real city building:
			// never hide all of that building's collision/visible roof meshes.
			if (Mesh->GetFName() != TEXT("AnchorMesh") && !Mesh->ComponentHasTag(TEXT("GrappleCosmetic"))) { continue; }
			Mesh->SetCastShadow(false);
			Mesh->SetHiddenInGame(true);
			Mesh->SetVisibility(false, true);
		}
	}
}

void ASonHatEvacuationDirector::ResolveDefaultRoundSounds()
{
	auto LoadIfNull = [](TObjectPtr<USoundBase>& Slot, const TCHAR* Path)
	{
		if (!Slot) { Slot = LoadObject<USoundBase>(nullptr, Path); }
	};
	LoadIfNull(FloodWarningSound, TEXT("/Game/Jam/A/Audio/SFX_FloodWarning.SFX_FloodWarning"));
	LoadIfNull(FloodUrgentSound, TEXT("/Game/Jam/A/Audio/SFX_FloodUrgent.SFX_FloodUrgent"));
	LoadIfNull(LinkActivationSound, TEXT("/Game/Jam/A/Audio/SFX_LinkSafe.SFX_LinkSafe"));
	LoadIfNull(LinkActivationUnsafeSound, TEXT("/Game/Jam/A/Audio/SFX_LinkUnsafe.SFX_LinkUnsafe"));
	LoadIfNull(PreparationAmbienceSound, TEXT("/Game/Jam/A/Audio/AMB_RainWindLoop.AMB_RainWindLoop"));
	LoadIfNull(FloodSirenAmbienceSound, TEXT("/Game/Jam/A/Audio/AMB_EmergencySirenLoop.AMB_EmergencySirenLoop"));
	LoadIfNull(FloodRumbleAmbienceSound, TEXT("/Game/Jam/A/Audio/AMB_FloodRumbleLoop.AMB_FloodRumbleLoop"));
	LoadIfNull(TerminalSubmergedSound, TEXT("/Game/Jam/A/Audio/SFX_TerminalSubmerged.SFX_TerminalSubmerged"));
	LoadIfNull(RoundResultsSound, TEXT("/Game/Jam/A/Audio/SFX_RoundResults.SFX_RoundResults"));
	auto MarkLoop = [](USoundBase* Sound)
	{
		if (USoundWave* Wave = Cast<USoundWave>(Sound)) { Wave->bLooping = true; }
	};
	MarkLoop(PreparationAmbienceSound);
	MarkLoop(FloodSirenAmbienceSound);
	MarkLoop(FloodRumbleAmbienceSound);
}

void ASonHatEvacuationDirector::StopRoundAmbience()
{
	auto StopOne = [this](TObjectPtr<UAudioComponent>& Comp)
	{
		if (!Comp) { return; }
		Comp->Stop();
		RemoveInstanceComponent(Comp);
		Comp->DestroyComponent();
		Comp = nullptr;
	};
	StopOne(PreparationAmbience);
	StopOne(FloodSirenAmbience);
	StopOne(FloodRumbleAmbience);
}

void ASonHatEvacuationDirector::StartRoundAmbience()
{
	StopRoundAmbience();
	auto StartLoop = [this](TObjectPtr<UAudioComponent>& Comp, USoundBase* Sound, float Volume)
	{
		if (!Sound) { return; }
		Comp = NewObject<UAudioComponent>(this);
		Comp->SetSound(Sound);
		Comp->bAutoActivate = false;
		Comp->bIsUISound = true;
		Comp->SetVolumeMultiplier(FMath::Clamp(Volume, 0.f, 1.f));
		Comp->RegisterComponent();
		AddInstanceComponent(Comp);
		Comp->Play();
	};
	StartLoop(PreparationAmbience, PreparationAmbienceSound, PreparationAmbienceVolume);
	StartLoop(FloodSirenAmbience, FloodSirenAmbienceSound, FloodSirenAmbienceVolume);
	StartLoop(FloodRumbleAmbience, FloodRumbleAmbienceSound, FloodRumbleAmbienceVolume * 0.35f);
}

void ASonHatEvacuationDirector::StopRoundCues()
{
	for (TObjectPtr<UAudioComponent>* Slot : {&RoundFeedbackCue, &RoundAlertCue})
	{
		if (IsValid(Slot->Get()))
		{
			(*Slot)->Stop(); RemoveInstanceComponent(Slot->Get()); (*Slot)->DestroyComponent();
		}
		*Slot = nullptr;
	}
}

void ASonHatEvacuationDirector::PlayRoundCue(USoundBase* Sound, float Volume, bool bAlert)
{
	if (!Sound) { return; }
	if (bAlert && Sound == TerminalSubmergedSound && IsValid(RoundAlertCue) && RoundAlertCue->IsPlaying()
		&& RoundAlertCue->Sound == FloodUrgentSound) { return; } // A closure must not interrupt the final urgent cue.
	TObjectPtr<UAudioComponent>& Cue = bAlert ? RoundAlertCue : RoundFeedbackCue;
	if (!IsValid(Cue))
	{
		Cue = NewObject<UAudioComponent>(this, MakeUniqueObjectName(this, UAudioComponent::StaticClass(), bAlert ? TEXT("RoundAlertCue") : TEXT("RoundFeedbackCue")));
		Cue->bAutoActivate = false; Cue->bAutoDestroy = false; Cue->bIsUISound = true;
		AddInstanceComponent(Cue); Cue->RegisterComponent();
	}
	// Two bounded channels: replace old feedback/alerts instead of stacking untracked one-shots.
	Cue->Stop(); Cue->SetSound(Sound); Cue->SetVolumeMultiplier(FMath::Clamp(Volume, 0.f, 1.f)); Cue->Play();
	UE_LOG(LogSonHatEvacuationDirector, Display, TEXT("PresentationAudio: %s channel=%s"), *Sound->GetName(), bAlert ? TEXT("alert") : TEXT("feedback"));
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
	if (Phase == ESonHatEvacuationRoundPhase::Results)
	{
		StopRoundAmbience(); StopRoundCues(); PlayRoundCue(RoundResultsSound, 0.55f);
	}
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
	for (TActorIterator<ASonHatEvacuationDirector> It(GetWorld()); It; ++It)
	{
		if (*It != this)
		{
			ConfigurationError(TEXT("Use exactly ONE evacuation Director for the local player's round. Remove the duplicate, including streamed instances."));
			return false;
		}
	}
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
	if ((ExpectedNodeCount > 0 && RoundNodes.Num() != ExpectedNodeCount)
		|| (ExpectedLinkCount > 0 && RoundLinks.Num() != ExpectedLinkCount))
	{
		ConfigurationError(FString::Printf(TEXT("Incomplete network: loaded %d nodes / %d links; expected %d / %d (0 = unchecked). Load the full network before starting, then R to retry."),
			RoundNodes.Num(), RoundLinks.Num(), ExpectedNodeCount, ExpectedLinkCount));
		return false;
	}
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
	if (bRescueSignalMode && !ValidateRescueNetwork()) { return false; }
	FloodCenter = bFitFloodToNetwork ? Bounds.GetCenter() : GetActorLocation();
	FloodCenter.Z = GetActorLocation().Z;
	FloodExtents = bFitFloodToNetwork ? FVector2D(Bounds.GetExtent().X + 1000, Bounds.GetExtent().Y + 1000) : FloodHalfSize;
	FloodExtents.X = FMath::IsFinite(FloodExtents.X) ? FMath::Clamp(FloodExtents.X, 100.0, 100000.0) : 6000.0;
	FloodExtents.Y = FMath::IsFinite(FloodExtents.Y) ? FMath::Clamp(FloodExtents.Y, 100.0, 100000.0) : 6000.0;
	return true;
}

FName ASonHatEvacuationDirector::GetRescueGroupId(const ASonHatEvacuationNode* Node) const
{
	for (const FSonHatRescueGroup& Group : RescueGroups)
	{
		if (Group.PopulationNode == Node) { return Group.GroupId; }
	}
	return NAME_None;
}

FString ASonHatEvacuationDirector::GetRescueBeaconTitle(const ASonHatEvacuationLink* Beacon) const
{
	for (const FSonHatRescueGroup& Group : RescueGroups)
	{
		if (Group.Beacon == Beacon && IsValid(Group.PopulationNode))
		{
			return FString::Printf(TEXT("ROOF %s — %d PEOPLE"), *Group.GroupId.ToString(), Group.PopulationNode->CivilianCount);
		}
	}
	return TEXT("UNCONFIGURED RESCUE BEACON");
}

bool ASonHatEvacuationDirector::ValidateRescueNetwork()
{
	const auto Reject = [this]()
	{
		ConfigurationError(TEXT("Rescue setup: exactly A/B/C/D (35/45/60/20), one zero-population Hub, four unique direct SAFE beacons, population anchors/bounds and roof floor elevations are required. Remove all legacy links in this network."));
		return false;
	};
	if (RoundNodes.Num() != 5 || RoundLinks.Num() != 4 || RescueGroups.Num() != 4) { return Reject(); }
	ASonHatEvacuationNode* Hub = nullptr;
	for (ASonHatEvacuationNode* Node : RoundNodes) { if (Node->bIsSafeHub) { Hub = Node; } }
	if (!Hub || Hub->CivilianCount != 0) { return Reject(); }
	TSet<FName> Ids;
	TSet<ASonHatEvacuationNode*> GroupNodes;
	TSet<ASonHatEvacuationLink*> Beacons;
	for (const FSonHatRescueGroup& Group : RescueGroups)
	{
		const FString Id = Group.GroupId.ToString();
		const int32 Expected = Id == TEXT("A") ? 35 : Id == TEXT("B") ? 45 : Id == TEXT("C") ? 60 : Id == TEXT("D") ? 20 : -1;
		ASonHatEvacuationNode* Node = Group.PopulationNode;
		ASonHatEvacuationLink* Beacon = Group.Beacon;
		if (Expected < 0 || Ids.Contains(Group.GroupId) || !IsValid(Node) || !RoundNodes.Contains(Node) || Node == Hub
			|| Node->CivilianCount != Expected || GroupNodes.Contains(Node) || !IsValid(Beacon) || !RoundLinks.Contains(Beacon)
			|| Beacons.Contains(Beacon) || !Beacon->bFloodSafe || !FMath::IsFinite(Group.FloodClosureElevation)
			|| !IsValid(Group.PopulationAnchor) || Group.PopulationAnchor->GetWorld() != GetWorld()
			|| Group.PopulationAnchor->GetActorLocation().ContainsNaN() || Group.PopulationAnchor->GetActorQuat().ContainsNaN()
			|| FMath::Abs(Group.PopulationAnchor->GetActorLocation().Z - Group.FloodClosureElevation) > 50.f
			|| !FMath::IsFinite(Group.PopulationHalfExtents.X) || !FMath::IsFinite(Group.PopulationHalfExtents.Y)
			|| Group.PopulationHalfExtents.X <= 0 || Group.PopulationHalfExtents.Y <= 0
			|| Group.FloodClosureElevation > Beacon->GetActorLocation().Z
			|| !((Beacon->NodeA == Hub && Beacon->NodeB == Node) || (Beacon->NodeB == Hub && Beacon->NodeA == Node))) { return Reject(); }
		Ids.Add(Group.GroupId); GroupNodes.Add(Node); Beacons.Add(Beacon);
	}
	// Even explicit-reference configurations must not leave hidden legacy interactions in this network.
	for (TActorIterator<ASonHatEvacuationLink> It(GetWorld()); It; ++It)
	{
		if ((NetworkTag.IsNone() || It->ActorHasTag(NetworkTag)) && !Beacons.Contains(*It)) { return Reject(); }
	}
	for (TActorIterator<ASonHatEvacuationManager> It(GetWorld()); It; ++It)
	{
		if (It->bHasStarted) { return Reject(); } // Never run competing signal/legacy authorities.
	}
	return true;
}

bool ASonHatEvacuationDirector::ApplyPlayerReplayReset()
{
	if (!Controller.IsValid()) { return true; }
	APawn* Pawn = Controller->GetPawn();
	if (!IsValid(Pawn)) { return false; }
	const FTransform Target = IsValid(ReplaySpawnAnchor) ? ReplaySpawnAnchor->GetActorTransform()
		: bHasCapturedReplayTransform ? CapturedReplayTransform : Pawn->GetActorTransform();
	if (UGrappleComponent* Grapple = Pawn->FindComponentByClass<UGrappleComponent>())
	{
		if (!bReturnPlayerToStartOnReplay && Grapple->IsAwaitingFallRetry() && !Grapple->AcknowledgeFallRetry())
		{
			ConfigurationError(TEXT("The last rooftop is obstructed. Clear the replay spawn or enable Return Player To Start On Replay."));
			return false;
		}
		Grapple->StopGrapple();
		Grapple->ResetFallRecoveryCheckpoint();
	}
	if (!bReturnPlayerToStartOnReplay) { return true; }
	if (!Pawn->TeleportTo(Target.GetLocation(), Target.Rotator()))
	{
		ConfigurationError(TEXT("Replay spawn is obstructed. Place Replay Spawn Anchor at a clear standing capsule location."));
		return false;
	}
	Controller->SetControlRotation(Target.Rotator());
	if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent()) { Movement->StopMovementImmediately(); }
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
	{
		RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	return true;
}

bool ASonHatEvacuationDirector::RestartRound()
{
	RefreshController();
	if (Controller.IsValid() && Controller->GetPawn())
	{
		if (UGrappleComponent* Grapple = Controller->GetPawn()->FindComponentByClass<UGrappleComponent>())
		{
			Grapple->SetRoundInputEnabled(false);
		}
	}
	const bool bWasReplay = Phase == ESonHatEvacuationRoundPhase::Results || Phase == ESonHatEvacuationRoundPhase::ConfigurationError;
	bRoundRequested = true;
	if (IsValid(Manager))
	{
		Manager->OnRoundEnded.RemoveAll(this);
		Manager->Destroy();
		Manager = nullptr;
	}
	ClearPresentation();
	SubmergedTerminals.Reset();
	bPlayedUrgentFloodSound = false;
	StopRoundAmbience();
	StopRoundCues();
	NextSubmergedCueAt = 0;
	OrientationEndsAt = 0;
	if (!Controller.IsValid() || !IsValid(Controller->GetPawn()))
	{
		StatusMessage = TEXT("Waiting for the level's existing local player pawn. Use Play, not Simulate.");
		SetPhase(ESonHatEvacuationRoundPhase::WaitingForPlayer);
		return false;
	}
	if (bWasReplay && !ApplyPlayerReplayReset()) { return false; }
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
	Manager->MaxActivations = bRescueSignalMode ? 3 : FMath::Max(0, ConnectionBudget);
	const float Duration = bRescueSignalMode ? RescuePreparationDuration : PreparationDuration;
	Manager->PreparationDuration = FMath::IsFinite(Duration) ? FMath::Max(1.0f, Duration) : bRescueSignalMode ? 150.0f : 90.0f;
	RoundPreparationDuration = Manager->PreparationDuration;
	RoundFloodStart = FMath::IsFinite(FloodStartElevation) ? FloodStartElevation : 200.0f;
	RoundFloodFinal = FMath::IsFinite(FloodFinalElevation) ? FMath::Max(RoundFloodStart, FloodFinalElevation) : FMath::Max(RoundFloodStart, 1650.0f);
	RoundFloodExponent = FMath::IsFinite(FloodRiseExponent) ? FMath::Clamp(FloodRiseExponent, 0.25f, 4.0f) : 1.2f;
	RoundFloodSurge = FMath::IsFinite(FloodRevealSurge) ? FMath::Max(0.0f, FloodRevealSurge) : 100.0f;
	PreparationStartedAt = GetWorld()->GetTimeSeconds();
	RevealStartFloodHeight = RoundFloodStart;
	Manager->InteractionDistance = FMath::IsFinite(InteractionDistance) ? FMath::Max(50.0f, InteractionDistance) : 275.0f;
	RoundRevealDuration = FMath::IsFinite(FloodRevealDuration) ? FMath::Max(0.0f, FloodRevealDuration) : 8.0f;
	Manager->OnRoundEnded.AddDynamic(this, &ASonHatEvacuationDirector::HandleRoundEnded);
	Manager->FinishSpawning(Transform);
	CreatePresentation();
	StatusMessage = bRescueSignalMode ? TEXT("REACH ROOFTOP GROUPS AND SEND RESCUE SIGNALS")
		: TEXT("Prepare evacuation links at rooftop terminals before the flood arrives.");
	StatusUntil = 0;
	StatusColor = FColor::White;
	SetPhase(ESonHatEvacuationRoundPhase::Preparing);
	Manager->StartPreparation();
	OrientationLastUpdatedAt = GetWorld()->GetTimeSeconds();
	OrientationEndsAt = OrientationLastUpdatedAt + (FMath::IsFinite(OrientationDuration) ? FMath::Clamp(OrientationDuration, 0.f, 20.f) : 0.f);
	UpdateTerminalFlooding();
	UpdatePresentation();
	PlayRoundCue(FloodWarningSound, 0.55f, true);
	StartRoundAmbience();
	SuppressGrappleAnchorCosmetics();
	if (UGrappleComponent* Grapple = Controller->GetPawn()->FindComponentByClass<UGrappleComponent>())
	{
		if (bUseLevelRecoveryBounds && FMath::IsFinite(LevelRecoveryWorldZ))
		{
			Grapple->bUseAbsoluteFallRecoveryZ = true; Grapple->FallRecoveryWorldZ = LevelRecoveryWorldZ;
			Grapple->bRequireFallRetryAcknowledgment = true; // Director can acknowledge immediately, retaining the manual fallback.
		}
		Grapple->SetRoundInputEnabled(true);
	}
	return true;
}

float ASonHatEvacuationDirector::GetOrientationSecondsRemaining() const
{
	return GetWorld() && Phase == ESonHatEvacuationRoundPhase::Preparing
		? FMath::Max(0.f, static_cast<float>(OrientationEndsAt - GetWorld()->GetTimeSeconds())) : 0.f;
}

void ASonHatEvacuationDirector::UpdateOrientationClock()
{
	if (!IsValid(Manager) || Manager->bGameEnded || Phase != ESonHatEvacuationRoundPhase::Preparing || OrientationEndsAt <= 0) { return; }
	const double Through = FMath::Min(GetWorld()->GetTimeSeconds(), OrientationEndsAt);
	const double Frozen = FMath::Max(0.0, Through - OrientationLastUpdatedAt);
	Manager->ExtendPreparationDeadline(static_cast<float>(Frozen));
	PreparationStartedAt += Frozen; // Same authored flood curve; only its explicit start is delayed.
	OrientationLastUpdatedAt = Through;
	if (GetWorld()->GetTimeSeconds() >= OrientationEndsAt) { OrientationEndsAt = 0; }
}

float ASonHatEvacuationDirector::CalculateFloodHeight(float Progress, float Start, float Final, float Exponent)
{
	const float SafeStart = FMath::IsFinite(Start) ? Start : 200.0f;
	const float SafeFinal = FMath::IsFinite(Final) ? FMath::Max(SafeStart, Final) : SafeStart;
	const float SafeExponent = FMath::IsFinite(Exponent) ? FMath::Clamp(Exponent, 0.25f, 4.0f) : 1.0f;
	const float SafeProgress = FMath::IsFinite(Progress) ? FMath::Clamp(Progress, 0.0f, 1.0f) : 0;
	return FMath::Lerp(SafeStart, SafeFinal, FMath::Pow(SafeProgress, SafeExponent));
}

float ASonHatEvacuationDirector::GetFloodHeight() const
{
	if (Phase == ESonHatEvacuationRoundPhase::FloodReveal || Phase == ESonHatEvacuationRoundPhase::Results)
	{
		return FMath::Lerp(RevealStartFloodHeight, RoundFloodFinal + RoundFloodSurge, GetFloodRevealProgress());
	}
	const float Progress = GetWorld() && Phase == ESonHatEvacuationRoundPhase::Preparing && bEnableRisingFlood
		? static_cast<float>((GetWorld()->GetTimeSeconds() - PreparationStartedAt) / RoundPreparationDuration) : 0;
	return CalculateFloodHeight(Progress, RoundFloodStart, RoundFloodFinal, RoundFloodExponent);
}

bool ASonHatEvacuationDirector::IsTerminalFloodVulnerable(const ASonHatEvacuationLink* Terminal) const
{
	if (!bEnableRisingFlood || !bEnableTerminalFlooding || !IsValid(Terminal) || !RoundLinks.Contains(Terminal)) { return false; }
	if (bRescueSignalMode) { return true; } // Rescue groups cannot inherit protected legacy terminal overrides.
	for (const FSonHatTerminalFloodRule& Rule : TerminalFloodRules)
	{
		if (Rule.Terminal == Terminal) { return Rule.bVulnerable; }
	}
	return true;
}

float ASonHatEvacuationDirector::GetTerminalClosureHeight(const ASonHatEvacuationLink* Terminal) const
{
	if (bRescueSignalMode)
	{
		for (const FSonHatRescueGroup& Group : RescueGroups)
		{
			if (Group.Beacon == Terminal) { return Group.FloodClosureElevation; }
		}
	}
	return Terminal->GetActorLocation().Z;
}

bool ASonHatEvacuationDirector::IsTerminalSubmerged(const ASonHatEvacuationLink* Terminal) const
{
	if (!IsValid(Manager) || !IsTerminalFloodVulnerable(Terminal)) { return false; }
	// Query live height as well as latched closures: a direct Interact() between ticks must also reject.
	return SubmergedTerminals.Contains(TWeakObjectPtr<ASonHatEvacuationLink>(const_cast<ASonHatEvacuationLink*>(Terminal)))
		|| GetFloodHeight() >= GetTerminalClosureHeight(Terminal);
}

float ASonHatEvacuationDirector::GetTerminalSecondsUntilFlood(const ASonHatEvacuationLink* Terminal) const
{
	if (!IsValid(Manager) || !IsTerminalFloodVulnerable(Terminal)) { return -1; }
	if (IsTerminalSubmerged(Terminal)) { return 0; }
	const float Height = GetTerminalClosureHeight(Terminal);
	if (Phase != ESonHatEvacuationRoundPhase::Preparing || Height > RoundFloodFinal || RoundFloodFinal <= RoundFloodStart) { return -1; }
	const float HeightFraction = FMath::Clamp((Height - RoundFloodStart) / (RoundFloodFinal - RoundFloodStart), 0.0f, 1.0f);
	const float ClosingElapsed = FMath::Pow(HeightFraction, 1.0f / RoundFloodExponent) * RoundPreparationDuration;
	return FMath::Max(0.0f, ClosingElapsed - static_cast<float>(GetWorld()->GetTimeSeconds() - PreparationStartedAt));
}

bool ASonHatEvacuationDirector::IsTerminalFloodWarning(const ASonHatEvacuationLink* Terminal) const
{
	const float Seconds = GetTerminalSecondsUntilFlood(Terminal);
	const float Lead = FMath::IsFinite(TerminalFloodWarningSeconds) ? FMath::Max(1.0f, TerminalFloodWarningSeconds) : 12.0f;
	return IsValid(Terminal) && !Terminal->IsActive() && Seconds > 0 && Seconds <= Lead;
}

void ASonHatEvacuationDirector::UpdateTerminalFlooding()
{
	if (!IsValid(Manager)) { return; }
	TArray<FString> NewlyClosed;
	for (ASonHatEvacuationLink* Link : RoundLinks)
	{
		if (!IsValid(Link) || SubmergedTerminals.Contains(Link) || !IsTerminalSubmerged(Link)) { continue; }
		SubmergedTerminals.Add(Link);
		if (!Link->IsActive()) { NewlyClosed.Add(SonHatEvacuationPresentation::LinkName(Link)); }
	}
	if (Phase == ESonHatEvacuationRoundPhase::Preparing && !NewlyClosed.IsEmpty())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (StatusUntil <= Now)
		{
			StatusMessage = Manager->GetRemainingConnections() > 0
				? TEXT("TERMINAL UNDERWATER\nChoose another console. Prepared routes remain.")
				: TEXT("TERMINAL UNDERWATER\nPrepared routes remain. Watch the flood outcome.");
			if (bRescueSignalMode) { StatusMessage = TEXT("ROOFTOP BEACON FLOODED\nExisting registrations are kept. ") + GetNextObjectiveText(); }
			StatusUntil = Now + 4; StatusColor = FColor::Magenta;
		}
		if (Now >= NextSubmergedCueAt)
		{
			PlayRoundCue(TerminalSubmergedSound, 0.5f, true); NextSubmergedCueAt = Now + 1.5;
		}
	}
	if (Phase == ESonHatEvacuationRoundPhase::Preparing && !bPlayedUrgentFloodSound && Manager->GetRemainingTime() <= 15)
	{
		bPlayedUrgentFloodSound = true;
		PlayRoundCue(FloodUrgentSound, 0.65f, true);
	}
}

FVector ASonHatEvacuationDirector::GetTerminalDisplayPosition(const ASonHatEvacuationLink* Link) const
{
	FVector Position = Link->GetActorLocation();
	if (IsTerminalSubmerged(Link)) { Position.Z = FMath::Max(Position.Z, static_cast<double>(GetFloodHeight() + 35)); }
	return Position;
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
	return !GetWorld()->LineTraceSingleByChannel(Hit, Camera, GetTerminalDisplayPosition(Link), ECC_Visibility, Params)
		|| Hit.GetActor() == Link;
}

ASonHatEvacuationLink* ASonHatEvacuationDirector::ResolveFocusedLinkForTests(
	const TArray<TObjectPtr<ASonHatEvacuationLink>>& Links,
	const FVector& PawnLocation,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	float InInteractionDistance,
	float AimHalfAngleDegrees,
	TFunctionRef<bool(const ASonHatEvacuationLink*)> LineOfSightFn,
	TFunction<FVector(const ASonHatEvacuationLink*)> AimPositionFn)
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
		const FVector ToTerminal = (AimPositionFn ? AimPositionFn(Link) : Link->GetActorLocation()) - ViewLocation;
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
		Manager->InteractionDistance, TerminalAimHalfAngleDegrees, [this](const ASonHatEvacuationLink* Link) { return HasLineOfSight(Link); },
		[this](const ASonHatEvacuationLink* Link) { return GetTerminalDisplayPosition(Link); });
}

ASonHatEvacuationLink* ASonHatEvacuationDirector::GetFocusedLink() const
{
	return ResolveFocusedLink();
}

ASonHatEvacuationLink* ASonHatEvacuationDirector::GetSuggestedTerminal() const
{
	if (Phase != ESonHatEvacuationRoundPhase::Preparing || !IsValid(Manager) || Manager->bGameEnded
		|| Manager->GetRemainingConnections() <= 0 || !Controller.IsValid() || !Controller->GetPawn()) { return nullptr; }
	if (const UGrappleComponent* Grapple = Controller->GetPawn()->FindComponentByClass<UGrappleComponent>(); Grapple && Grapple->IsAwaitingFallRetry()) { return nullptr; }
	ASonHatEvacuationLink* Best = nullptr;
	int32 BestPriority = MAX_int32;
	double BestDistance = MAX_dbl;
	for (ASonHatEvacuationLink* Link : RoundLinks)
	{
		if (!IsValid(Link) || !Link->HasValidEndpoints() || !Manager->OwnsLink(Link) || Link->IsActive() || IsTerminalSubmerged(Link)) { continue; }
		const bool bExtendsAccess = Manager->HasSafeRoute(Link->NodeA) != Manager->HasSafeRoute(Link->NodeB);
		if (!Link->bFloodSafe || !bExtendsAccess) { continue; } // Never make a wasted/redundant connection the main objective.
		const int32 Priority = 0;
		const double Distance = FVector::DistSquared(Controller->GetPawn()->GetActorLocation(), Link->GetActorLocation());
		if (Priority < BestPriority || (Priority == BestPriority && Distance < BestDistance))
		{
			Best = Link; BestPriority = Priority; BestDistance = Distance;
		}
	}
	return Best;
}

FString ASonHatEvacuationDirector::GetNextObjectiveText() const
{
	if (!IsValid(Manager)) { return FString(); }
	if (bRescueSignalMode)
	{
		if (Manager->bGameEnded) { return TEXT("SIGNAL WINDOW CLOSED — RESULTS"); }
		if (Manager->GetRemainingConnections() <= 0) { return TEXT("ALL 3 SIGNALS SENT — WAIT FOR RESULTS"); }
		if (ASonHatEvacuationLink* Next = GetSuggestedTerminal()) { return TEXT("NEXT: ") + GetRescueBeaconTitle(Next); }
		return TEXT("NO AVAILABLE ROOFTOP GROUPS — WAIT FOR RESULTS");
	}
	if (Manager->GetRemainingConnections() <= 0) { return TEXT("CONNECTIONS USED — WATCH YOUR ROUTES HOLD UP"); }
	if (ASonHatEvacuationLink* Next = GetSuggestedTerminal())
	{
		return TEXT("NEXT: MARKED ") + SonHatEvacuationPresentation::PlayerLinkName(Next).ToUpper() + TEXT(" CONSOLE");
	}
	return TEXT("NO USEFUL SAFE CONSOLE LEFT — WAIT FOR THE FLOOD");
}

void ASonHatEvacuationDirector::UpdateTerminalWayfinding()
{
	TArray<FSonHatTerminalWaypoint> Waypoints;
	ASonHatEvacuationLink* Suggested = GetSuggestedTerminal();
	if (Suggested && Controller.IsValid())
	{
		FVector Eye; FRotator Rotation;
		Controller->GetPlayerViewPoint(Eye, Rotation);
		int32 Width = 0, Height = 0;
		Controller->GetViewportSize(Width, Height);
		if (Width > 0 && Height > 0)
		{
			TArray<ASonHatEvacuationLink*> Candidates{Suggested};
			for (ASonHatEvacuationLink* Link : Candidates)
			{
				const FVector To = GetTerminalDisplayPosition(Link) - Eye;
				FVector2D Pixel;
				const bool bInFront = Controller->ProjectWorldLocationToScreen(GetTerminalDisplayPosition(Link), Pixel, true);
				FVector2D Position = bInFront ? FVector2D(Pixel.X / Width, Pixel.Y / Height)
					: FVector2D(0.5 + FVector::DotProduct(To.GetSafeNormal(), Rotation.RotateVector(FVector::RightVector)),
						0.8 - FVector::DotProduct(To.GetSafeNormal(), Rotation.RotateVector(FVector::UpVector)));
				const bool bOffscreen = !bInFront || Position.X < 0.12 || Position.X > 0.88 || Position.Y < 0.24 || Position.Y > 0.74;
				if (Link != Suggested && (bOffscreen || To.SizeSquared() > FMath::Square(6500.0) || Waypoints.Num() >= 3)) { continue; }
				Position.X = FMath::Clamp(Position.X, 0.12, 0.88);
				Position.Y = FMath::Clamp(Position.Y, 0.24, 0.74);
				bool bOverlaps = false;
				for (const auto& Other : Waypoints)
				{
					if (FMath::Abs(Other.Position.X - Position.X) < 0.19 && FMath::Abs(Other.Position.Y - Position.Y) < 0.09) { bOverlaps = true; }
				}
				if (bOverlaps) { continue; }
				FSonHatTerminalWaypoint Hint;
				Hint.Position = Position; Hint.bPrimary = Link == Suggested; Hint.bOffscreen = bOffscreen;
				const float Meters = FVector::Distance(Controller->GetPawn()->GetActorLocation(), Link->GetActorLocation()) / 100;
				Hint.Title = SonHatEvacuationPresentation::PlayerLinkName(Link) + FString::Printf(TEXT(" | %.0fm"), Meters);
				Hint.Detail = bOffscreen ? TEXT("NEXT CONSOLE — TURN THIS WAY") : TEXT("REACH THE CONSOLE — PRESS E");
				if (bRescueSignalMode)
				{
					Hint.Title = GetRescueBeaconTitle(Link) + FString::Printf(TEXT(" | %.0fm"), Meters);
					Hint.Detail = bOffscreen ? TEXT("NEXT GROUP — TURN THIS WAY") : TEXT("REACH BEACON — E TO SIGNAL");
				}
				Hint.Color = Link->bFloodSafe ? FLinearColor(0.35f, 1.f, 0.55f) : ColorUnsafe;
				if (IsTerminalFloodWarning(Link))
				{
					Hint.Detail = FString::Printf(TEXT("SAFE — CLOSES IN %.0fs"), FMath::CeilToFloat(GetTerminalSecondsUntilFlood(Link)));
					if (bRescueSignalMode) { Hint.Detail = FString::Printf(TEXT("FLOOD CLOSES BEACON IN %.0fs"), FMath::CeilToFloat(GetTerminalSecondsUntilFlood(Link))); }
					Hint.Color = ColorUnsafe;
				}
				Waypoints.Add(MoveTemp(Hint));
			}
		}
	}
	HudWidget->SetTerminalWaypoints(MoveTemp(Waypoints));
}

ESonHatEvacuationActivationResult ASonHatEvacuationDirector::Interact()
{
	using EResult = ESonHatEvacuationActivationResult;
	UpdateOrientationClock();
	if (!IsValid(Manager)) { return EResult::NotStarted; }
	if (!Controller.IsValid() || !IsValid(Controller->GetPawn())) { return EResult::InvalidInteractor; }
	if (UGrappleComponent* Grapple = Controller->GetPawn()->FindComponentByClass<UGrappleComponent>(); Grapple && Grapple->IsAwaitingFallRetry())
	{
		return EResult::InvalidInteractor;
	}
	StatusUntil = GetWorld()->GetTimeSeconds() + 5;
	const int32 BeforeAccess = Manager->EstimatedSafePopulation;
	TSet<ASonHatEvacuationNode*> PreviouslyConnected;
	for (ASonHatEvacuationNode* Node : RoundNodes) { if (Manager->HasSafeRoute(Node)) { PreviouslyConnected.Add(Node); } }
	ASonHatEvacuationLink* Target = GetFocusedLink();
	if (!Manager->bGameEnded && Manager->GetRemainingTime() > 0 && Target && IsTerminalSubmerged(Target))
	{
		StatusMessage = TEXT("SUBMERGED TERMINAL — CANNOT CONNECT\nNo connection used. Choose another SAFE console.");
		if (bRescueSignalMode) { StatusMessage = TEXT("BEACON FLOODED — SIGNAL NOT SENT\nNo signal charge used. ") + GetNextObjectiveText(); }
		StatusColor = FColor::Magenta;
		return EResult::InvalidLink; // Director access rejection; the authoritative manager is not called.
	}
	const EResult Result = Manager->bGameEnded || Manager->GetRemainingTime() <= 0
		? Manager->TryActivateLink(nullptr, Controller->GetPawn())
		: !Target ? EResult::NoCandidateInRange : Manager->TryActivateLink(Target, Controller->GetPawn());
	StatusColor = Result == EResult::Success ? FColor::Cyan : FColor::Yellow;
	switch (Result)
	{
	case EResult::Success:
	{
		OrientationEndsAt = 0; StatusUntil = GetWorld()->GetTimeSeconds() + 8;
		FString Roofs;
		for (ASonHatEvacuationNode* Node : RoundNodes)
		{
			if (IsValid(Node) && !Node->bIsSafeHub && !PreviouslyConnected.Contains(Node) && Manager->HasSafeRoute(Node))
			{ if (!Roofs.IsEmpty()) { Roofs += TEXT(", "); } Roofs += SonHatEvacuationPresentation::PlayerBuildingName(Node).ToUpper(); }
		}
		StatusMessage = !Target->bFloodSafe ? TEXT("RISK CONNECTION USED\nNO PEOPLE GAINED A SAFE ROUTE — 1 CONNECTION SPENT")
			: Manager->EstimatedSafePopulation > BeforeAccess ? FString::Printf(TEXT("SAFE ROUTE CONNECTED: %s\n%d PEOPLE NOW HAVE A ROUTE TO HIGH GROUND\n%d CONNECTIONS LEFT — %s"),
				*Roofs, Manager->EstimatedSafePopulation - BeforeAccess, Manager->GetRemainingConnections(),
				Manager->GetRemainingConnections() > 0 ? TEXT("FOLLOW THE NEXT MARKER") : TEXT("WATCH YOUR ROUTES HOLD UP"))
			: TEXT("SAFE CONNECTION SET — NO NEW ACCESS YET\nConnect this route to high ground to help its residents.");
		StatusColor = Target->bFloodSafe ? FColor::Green : FColor::Orange;
		PlayRoundCue(Target->bFloodSafe ? LinkActivationSound : LinkActivationUnsafeSound);
		break;
	}
	case EResult::AlreadyActive: StatusMessage = TEXT("Already selected. No additional connection spent."); break;
	case EResult::BudgetExhausted: StatusMessage = TEXT("NO CONNECTIONS LEFT\nWatch how your prepared routes hold up."); break;
	case EResult::NoCandidateInRange:
	case EResult::TooFarAway: StatusMessage = TEXT("Reach a visible console. Aim until its E prompt appears."); break;
	case EResult::PreparationEnded: StatusMessage = TEXT("Preparation ended. Routes are locked; inspect the flood outcome."); break;
	default: StatusMessage = FString::Printf(TEXT("Activation rejected: %s"), *UEnum::GetValueAsString(Result)); break;
	}
	if (bRescueSignalMode)
	{
		switch (Result)
		{
		case EResult::Success:
		{
			const ASonHatEvacuationNode* Group = Target->NodeA->bIsSafeHub ? Target->NodeB.Get() : Target->NodeA.Get();
			StatusMessage = FString::Printf(TEXT("ROOF %s SIGNAL RECEIVED\n+%d PEOPLE REGISTERED FOR RESCUE\n%d SIGNALS LEFT"),
				*GetRescueGroupId(Group).ToString(), Manager->EstimatedSafePopulation - BeforeAccess, Manager->GetRemainingConnections());
			break;
		}
		case EResult::AlreadyActive: StatusMessage = TEXT("GROUP ALREADY REGISTERED — NO EXTRA SIGNAL USED"); break;
		case EResult::BudgetExhausted: StatusMessage = TEXT("NO SIGNALS LEFT — YOUR REGISTRATIONS ARE KEPT"); break;
		case EResult::NoCandidateInRange:
		case EResult::TooFarAway: StatusMessage = TEXT("REACH A ROOFTOP BEACON — AIM UNTIL THE E PROMPT APPEARS"); break;
		case EResult::PreparationEnded: StatusMessage = TEXT("SIGNAL WINDOW CLOSED — REGISTRATIONS LOCKED"); break;
		default: break;
		}
	}
	UE_LOG(LogSonHatEvacuationDirector, Display, TEXT("%s"), *StatusMessage);
	return Result;
}

void ASonHatEvacuationDirector::HandleRoundEnded(int32 AccessPopulation, int32 TotalPopulation)
{
	if (Controller.IsValid() && Controller->GetPawn())
	{
		if (UGrappleComponent* Grapple = Controller->GetPawn()->FindComponentByClass<UGrappleComponent>()) { Grapple->SetRoundInputEnabled(false); }
	}
	StopRoundCues();
	// Keep the storm/rumble through the visible flood arrival. Results owns its cue and cleanup.
	RevealStartFloodHeight = GetFloodHeight(); // Continuous even when F finishes early.
	RevealStartedAt = GetWorld()->GetTimeSeconds();
	StatusMessage = FString::Printf(TEXT("FLOOD ARRIVES: %d/%d residents have SAFE-ROUTE ACCESS. No NPC evacuation simulated."), AccessPopulation, TotalPopulation);
	if (bRescueSignalMode) { StatusMessage = FString::Printf(TEXT("SIGNALS LOCKED: %d/%d PEOPLE REGISTERED FOR RESCUE"), AccessPopulation, TotalPopulation); }
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
	UpdateOrientationClock();
	if (Phase == ESonHatEvacuationRoundPhase::Preparing && IsValid(Manager) && bEnableRisingFlood)
	{
		const float Progress = FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - PreparationStartedAt) / RoundPreparationDuration), 0.f, 1.f);
		if (FloodRumbleAmbience)
		{
			FloodRumbleAmbience->SetVolumeMultiplier(FMath::Clamp(FloodRumbleAmbienceVolume * (0.25f + 0.75f * Progress), 0.f, 1.f));
		}
		if (PreparationAmbience)
		{
			float Volume = PreparationAmbienceVolume;
			if (Manager->GetRemainingTime() <= 15.f) { Volume = FMath::Min(1.f, Volume * 1.35f); }
			PreparationAmbience->SetVolumeMultiplier(Volume);
		}
		if (FloodSirenAmbience)
		{
			FloodSirenAmbience->SetVolumeMultiplier(FMath::Clamp(FloodSirenAmbienceVolume * (0.65f + 0.75f * Progress), 0.f, 1.f));
		}
	}
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
	UpdateTerminalFlooding(); // Must precede native E processing at this frame's waterline.
	UGrappleComponent* Grapple = nullptr;
	if (Controller.IsValid() && IsValid(Controller->GetPawn()))
	{
		Grapple = Controller->GetPawn()->FindComponentByClass<UGrappleComponent>();
	}
	if (Phase == ESonHatEvacuationRoundPhase::Preparing && bUseLevelRecoveryBounds && bAutomaticallyRecoverBelowBounds
		&& Grapple && Grapple->IsAwaitingFallRetry() && Grapple->AcknowledgeFallRetry())
	{
		StatusMessage = TEXT("BELOW THE CITY — RETURNED TO YOUR LAST ROOFTOP\nYour routes and connections are kept. Time continues.");
		if (bRescueSignalMode) { StatusMessage = TEXT("RETURNED TO YOUR LAST ROOFTOP\nRegistrations and signals kept. Time continues."); }
		StatusColor = FColor::Yellow; StatusUntil = GetWorld()->GetTimeSeconds() + 3;
	}
	if (Controller.IsValid())
	{
		if (Controller->WasInputKeyJustPressed(EKeys::R))
		{
			if (Phase == ESonHatEvacuationRoundPhase::Results || Phase == ESonHatEvacuationRoundPhase::ConfigurationError) { RestartRound(); }
			else if (Phase == ESonHatEvacuationRoundPhase::Preparing && Grapple && Grapple->IsAwaitingFallRetry()) { Grapple->AcknowledgeFallRetry(); }
		}
		else if (Phase == ESonHatEvacuationRoundPhase::Preparing && bAllowEarlyFinishForTesting
			&& Controller->WasInputKeyJustPressed(EKeys::F)) { Manager->FinalizePreparation(); }
		else if (Controller->WasInputKeyJustPressed(EKeys::E) && (!Grapple || !Grapple->IsAwaitingFallRetry())) { Interact(); }
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
	Label->SetTextMaterial(LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/UnlitText.UnlitText")));
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
		if (!bRescueSignalMode)
		{
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
		}
		ASonHatEvacuationLink* Link = RoundLinks[Index];
		const auto ConsolePart = [this, Link](const FName& Name, UMaterialInterface* Material)
		{
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Link, MakeUniqueObjectName(Link, UStaticMeshComponent::StaticClass(), Name));
			Link->AddInstanceComponent(Part); Part->SetupAttachment(Link->GetRootComponent());
			Part->SetMobility(EComponentMobility::Movable); Part->SetStaticMesh(TerminalConsoleMesh);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetGenerateOverlapEvents(false);
			Part->SetCanEverAffectNavigation(false); Part->SetCastShadow(false); Part->ComponentTags.Add(TEXT("SonHatTerminalCosmetic"));
			if (Material) { Part->SetMaterial(0, Material); }
			Part->RegisterComponent(); return Part;
		};
		Visual.TerminalMarker = ConsolePart(*FString::Printf(TEXT("Terminal_%d"), Index), ConsoleHousingMaterial);
		Visual.ConsoleParts.Add(ConsolePart(TEXT("ConsoleHead"), ConsoleTrimMaterial));
		Visual.ConsoleParts.Add(ConsolePart(TEXT("ConsoleScreen"), ConsoleDisplayMaterial));
		Visual.ConsoleParts.Add(ConsolePart(TEXT("ConsoleStatusLight"), ConsoleDisplayMaterial));
		Visual.ConsoleParts.Add(ConsolePart(TEXT("ConsoleControls"), ConsoleTrimMaterial));
		if (bRescueSignalMode)
		{
			Visual.TerminalMarker->SetStaticMesh(ConnectionBeamMesh);
			Visual.ConsoleParts[0]->SetStaticMesh(ConnectionBeamMesh);
			Visual.ConsoleParts[3]->SetStaticMesh(ConnectionBeamMesh);
		}
		// One placement trace per console. Its origin remains the existing Link interaction point.
		const FVector Origin = RoundLinks[Index]->GetActorLocation();
		FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(SonHatTerminalFloor), false, this);
		FloorParams.AddIgnoredActor(RoundLinks[Index]);
		FHitResult Floor;
		if (GetWorld()->LineTraceSingleByObjectType(Floor, Origin + FVector(0, 0, 50), Origin - FVector(0, 0, 400),
			FCollisionObjectQueryParams(ECC_WorldStatic), FloorParams) && Floor.ImpactNormal.Z > 0.5)
		{
			Visual.TerminalGroundOffset.Z = Floor.ImpactPoint.Z + 45 - Origin.Z;
		}
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
			if (IsValid(Comp)) { if (AActor* Owner = Comp->GetOwner()) { Owner->RemoveInstanceComponent(Comp); } Comp->DestroyComponent(); }
		};
		DestroyComp(Visual.SolidLine);
		DestroyComp(Visual.BrokenSegmentA);
		DestroyComp(Visual.BrokenSegmentB);
		DestroyComp(Visual.FailCrossA);
		DestroyComp(Visual.FailCrossB);
		for (USplineMeshComponent* Dash : Visual.DashSegments) { DestroyComp(Dash); }
		DestroyComp(Visual.TerminalMarker);
		for (UStaticMeshComponent* Part : Visual.ConsoleParts) { DestroyComp(Part); }
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
	const bool bShow = (bEnableRisingFlood && Phase == ESonHatEvacuationRoundPhase::Preparing)
		|| Phase == ESonHatEvacuationRoundPhase::FloodReveal || Phase == ESonHatEvacuationRoundPhase::Results;
	FloodPlane->SetVisibility(bShow, true);
	if (!bShow) { return; }
	FVector Center = FloodCenter;
	Center.Z = GetFloodHeight();
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
		const bool bShowNodeBillboard = !bRescueSignalMode && !bCompactWorldPresentation; // Population groups communicate compact-level results without spheres at the camera.
		if (bShowNodeBillboard)
		{
			const FString Text = Node->bIsSafeHub ? TEXT("SAFE HUB")
				: FString::Printf(TEXT("%s | %d"), *BuildingName(Node), FMath::Max(0, Node->CivilianCount));
			SetLabel(NodeLabels[Index], Text, Node->GetActorLocation() + FVector(0, 0, 140), Camera, Color.ToFColor(true));
			NodeLabels[Index]->SetVisibility(true);
		}
		else
		{
			NodeLabels[Index]->SetVisibility(false);
		}
		NodeMarkers[Index]->SetWorldLocation(Node->GetActorLocation());
		EnsureMarkerMaterial(NodeMarkerMaterials[Index], Color);
		if (NodeMarkerMaterials[Index]) { NodeMarkers[Index]->SetMaterial(0, NodeMarkerMaterials[Index]); }
		NodeMarkers[Index]->SetVisibility(bShowNodeBillboard && !Node->bIsSafeHub, true);
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
		if (bRescueSignalMode)
		{
			const bool bRegistered = Link->IsActive();
			const bool bClosed = !bRegistered && (bFlood || IsTerminalSubmerged(Link));
			const bool bWarning = !bRegistered && !bClosed && IsTerminalFloodWarning(Link);
			if (bRegistered && Visual.RegisteredAt < 0) { Visual.RegisteredAt = GetWorld()->GetTimeSeconds(); }
			const float PulseAge = Visual.RegisteredAt < 0 ? 10.f : static_cast<float>(GetWorld()->GetTimeSeconds() - Visual.RegisteredAt);
			const float Pulse = PulseAge < 1.2f ? 1.f + 0.25f * FMath::Sin(PulseAge / 1.2f * PI) : 1.f;
			const FLinearColor Color = bRegistered ? FLinearColor(.12f,.8f,.24f) * Pulse
				: bClosed ? FLinearColor(.32f,.12f,.4f) : bWarning ? FLinearColor(.95f,.24f,.06f) : FLinearColor(.9f,.55f,.08f);
			if (!Visual.TerminalMaterial && ConsoleDisplayMaterial) { Visual.TerminalMaterial = UMaterialInstanceDynamic::Create(ConsoleDisplayMaterial, this); }
			if (Visual.TerminalMaterial) { Visual.TerminalMaterial->SetVectorParameterValue(TEXT("Color"), Color); }
			const FVector Origin = Link->GetActorLocation() + Visual.TerminalGroundOffset;
			const FRotator BeaconRotation = Link->GetActorRotation();
			Visual.TerminalMarker->SetWorldLocation(Origin);
			Visual.TerminalMarker->SetWorldScale3D(FVector(.12,.12,.9));
			Visual.TerminalMarker->SetVisibility(true, true);
			const FVector Offsets[] = {FVector(0,0,49), FVector(0,0,65), FVector(0,0,65), FVector(0,0,-41)};
			const FVector Scales[] = {FVector(.42,.42,.08), FVector(.08,.38,.12), FVector(.38,.08,.12), FVector(.55,.55,.08)};
			for (int32 PartIndex = 0; PartIndex < Visual.ConsoleParts.Num(); ++PartIndex)
			{
				UStaticMeshComponent* Part = Visual.ConsoleParts[PartIndex];
				Part->SetWorldLocation(Origin + BeaconRotation.RotateVector(Offsets[PartIndex])); Part->SetWorldRotation(BeaconRotation);
				Part->SetWorldScale3D(Scales[PartIndex] * (PartIndex < 3 ? Pulse : 1.f));
				if (PartIndex < 3 && Visual.TerminalMaterial) { Part->SetMaterial(0, Visual.TerminalMaterial); }
			}
			FVector LabelPosition = Origin + FVector(0,0,145);
			LabelPosition.Z = FMath::Max(LabelPosition.Z, static_cast<double>(GetFloodHeight() + 70));
			const TCHAR* State = bRegistered ? TEXT("SIGNAL RECEIVED") : bClosed ? TEXT("BEACON UNAVAILABLE")
				: Link == Focused && Manager->GetRemainingConnections() > 0 ? TEXT("E — SEND RESCUE SIGNAL") : TEXT("RESCUE SIGNAL BEACON");
			SetLabel(LinkLabels[Index], GetRescueBeaconTitle(Link) + TEXT("\n") + State, LabelPosition, Camera, Color.ToFColor(true));
			LinkLabels[Index]->SetWorldSize(16);
			LinkLabels[Index]->SetVisibility(FVector::DistSquared(Camera, LabelPosition) <= FMath::Square(6500.0));
			continue; // No route beams, SAFE/RISK labels or old console shapes in rescue mode.
		}
		const bool bSelected = Link->IsActive();
		const bool bSubmerged = IsTerminalSubmerged(Link);
		const bool bClosing = IsTerminalFloodWarning(Link);
		const bool bAccess = Link->HasValidEndpoints() && Manager->HasSafeRoute(Link->NodeA) && Manager->HasSafeRoute(Link->NodeB);
		FLinearColor Color = !Link->bFloodSafe ? ColorUnsafe : bSelected ? ColorSelectedSafe : ColorUnselectedSafe;
		if (bFlood && bSelected) { Color = !Link->bFloodSafe ? ColorFailed : bAccess ? ColorSafeAccess : ColorIntactNoHub; }
		const TCHAR* State = !bSelected ? TEXT("NOT SELECTED") : bFlood && !Link->bFloodSafe ? TEXT("FAILED AT FLOOD")
			: bFlood && !bAccess ? TEXT("INTACT, BUT NO HUB PATH") : bFlood ? TEXT("INTACT: SAFE ACCESS") : TEXT("SELECTED");
		const FString FloodState = bSubmerged ? (bSelected ? TEXT("SUBMERGED TERMINAL | PREPARATION RETAINED") : TEXT("SUBMERGED TERMINAL | E LOCKED"))
			: bClosing ? FString::Printf(TEXT("CLOSES IN %.0fs - PREPARE NOW"), FMath::CeilToFloat(GetTerminalSecondsUntilFlood(Link))) : TEXT("");
		const FVector MarkerPosition = GetTerminalDisplayPosition(Link);
		const FLinearColor TerminalColor = bSubmerged ? ColorSubmergedTerminal : bClosing ? ColorUnsafe : Color;
		const FVector ConsolePosition = Link->GetActorLocation() + Visual.TerminalGroundOffset;
		const bool bNearby = FVector::DistSquared(Camera, ConsolePosition) < FMath::Square(3000.0);
		const bool bActionable = !bFlood && !bSubmerged && !bSelected && Manager->GetRemainingConnections() > 0;
		if (bCompactWorldPresentation && bNearby)
		{
			const TCHAR* Caption = bSubmerged ? TEXT("CLOSED") : bFlood && bSelected && !Link->bFloodSafe ? TEXT("FAILED")
				: bSelected ? TEXT("SET")
				: Link == Focused && bActionable ? TEXT("[E]") : Link->bFloodSafe ? TEXT("SAFE") : TEXT("RISK");
			LinkLabels[Index]->SetWorldSize(12);
			FVector Position = ConsolePosition + FVector(0, 0, 105);
			if (bSubmerged) { Position.Z = FMath::Max(Position.Z, static_cast<double>(GetFloodHeight() + 60)); }
			const FString Name = PlayerLinkName(Link);
			const FString RouteKind = Link->bFloodSafe ? TEXT("SAFE") : TEXT("RISK");
			const FString ConsoleText = Name + TEXT("\n") + RouteKind + (FString(Caption) == RouteKind ? TEXT("") : TEXT(" | ") + FString(Caption));
			SetLabel(LinkLabels[Index], ConsoleText, Position, Camera, TerminalColor.ToFColor(true));
		}
		else if (!bCompactWorldPresentation && (bFlood || Link == Focused || bClosing || bSelected))
		{
			LinkLabels[Index]->SetWorldSize(FMath::Max(10.f, LabelWorldSize));
			const FString Short = Link == Focused
				? FString::Printf(TEXT("[E] %s | %s"), *LinkName(Link), Link->bFloodSafe ? TEXT("SAFE") : TEXT("UNSAFE"))
				: bClosing ? FString::Printf(TEXT("%s closes in %.0fs"), *LinkName(Link), FMath::CeilToFloat(GetTerminalSecondsUntilFlood(Link)))
				: FString::Printf(TEXT("%s | %s"), *LinkName(Link), Link->bFloodSafe ? TEXT("SAFE") : TEXT("UNSAFE"));
			SetLabel(LinkLabels[Index], Short, MarkerPosition + FVector(0, 0, 90), Camera, TerminalColor.ToFColor(true));
			LinkLabels[Index]->SetVisibility(true);
		}
		else
		{
			LinkLabels[Index]->SetVisibility(false);
		}
		Visual.TerminalMarker->SetWorldLocation(ConsolePosition);
		const FRotator ConsoleRotation = Link->GetActorRotation();
		Visual.TerminalMarker->SetWorldRotation(ConsoleRotation);
		Visual.TerminalMarker->SetWorldScale3D(FVector(0.32f, 0.42f, 0.90f));
		const FLinearColor MarkerColor = bSubmerged ? ColorSubmergedTerminal : !Link->bFloodSafe ? (bSelected ? FLinearColor(.38f,.18f,.025f) : ColorUnsafe)
			: bSelected ? FLinearColor(0.1f, 0.45f, 0.2f) : FLinearColor(0.15f, 0.95f, 0.32f);
		if (!Visual.TerminalMaterial && ConsoleDisplayMaterial) { Visual.TerminalMaterial = UMaterialInstanceDynamic::Create(ConsoleDisplayMaterial, this); }
		if (Visual.TerminalMaterial) { Visual.TerminalMaterial->SetVectorParameterValue(TEXT("Color"), MarkerColor); }
		const FVector Offsets[] = {FVector(0,0,51), FVector(9,0,53), FVector(0,0,73), FVector(18,0,20)};
		const FVector Scales[] = {FVector(.18,.48,.30), FVector(.025,.38,.22), FVector(.12,.22,.045), FVector(.04,.24,.10)};
		for (int32 PartIndex=0; PartIndex<Visual.ConsoleParts.Num(); ++PartIndex)
		{
			UStaticMeshComponent* Part = Visual.ConsoleParts[PartIndex];
			Part->SetWorldLocation(ConsolePosition + ConsoleRotation.RotateVector(Offsets[PartIndex]));
			Part->SetWorldRotation(ConsoleRotation); Part->SetWorldScale3D(Scales[PartIndex]);
			if ((PartIndex==1 || PartIndex==2) && Visual.TerminalMaterial) { Part->SetMaterial(0, Visual.TerminalMaterial); }
		}
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
	HudLines.Reset(); HudLineColors.Reset();
	const auto AddLine = [this](const FString& Text, const FLinearColor& Color)
	{
		TArray<FString> Parts; Text.ParseIntoArrayLines(Parts, true);
		for (const FString& Part : Parts) { HudLines.Add(Part); HudLineColors.Add(Color); }
	};
	if (!IsValid(Manager))
	{
		AddLine(Phase == ESonHatEvacuationRoundPhase::ConfigurationError ? TEXT("LEVEL SETUP NEEDS ATTENTION") : TEXT("Waiting for evacuation round…"), FLinearColor::White);
		AddLine(StatusMessage, FLinearColor(1, 0.65f, 0.3f));
		if (Phase == ESonHatEvacuationRoundPhase::ConfigurationError) { AddLine(TEXT("R: retry after the level setup is corrected"), FLinearColor::White); }
		HudWidget->SetHudLines(MoveTemp(HudLines), HudLineColors);
		HudWidget->SetInteractionPrompt(FString(), FLinearColor::White);
		HudWidget->SetTerminalWaypoints({});
		return;
	}
	const bool bUrgent = !Manager->bGameEnded && Manager->GetRemainingTime() <= 15;
	const UGrappleComponent* Grapple = Controller.IsValid() && Controller->GetPawn()
		? Controller->GetPawn()->FindComponentByClass<UGrappleComponent>() : nullptr;
	const bool bWaitingForRetry = Grapple && Grapple->IsAwaitingFallRetry();
	FString InteractionPrompt;
	FLinearColor InteractionColor = FLinearColor::White;
	if (bRescueSignalMode)
	{
		const bool bPreparing = Phase == ESonHatEvacuationRoundPhase::Preparing && !Manager->bGameEnded;
		const int32 Registered = bPreparing ? Manager->EstimatedSafePopulation : Manager->FinalSafePopulation;
		const int32 Left = Manager->GetRemainingConnections();
		if (bPreparing)
		{
			const int32 Seconds = FMath::CeilToInt(Manager->GetRemainingTime());
			AddLine(FString::Printf(TEXT("TIME LEFT: %02d:%02d  |  SIGNALS LEFT: %d / 3"), Seconds / 60, Seconds % 60, Left),
				bUrgent ? FLinearColor(1,.4f,.2f) : FLinearColor::White);
			AddLine(FString::Printf(TEXT("REGISTERED FOR RESCUE: %d / %d"), Registered, Manager->TotalPopulation), FLinearColor(.35f,1,.5f));
			AddLine(FString::Printf(TEXT("PEOPLE NOT YET REACHED: %d"), Manager->TotalPopulation - Registered), FLinearColor::White);
			AddLine(bWaitingForRetry ? TEXT("R — RETURN TO LAST ROOFTOP | TIME CONTINUES") : GetNextObjectiveText(), FLinearColor(1,.85f,.4f));
			if (StatusUntil > GetWorld()->GetTimeSeconds()) { AddLine(StatusMessage, StatusColor.ReinterpretAsLinear()); }
			else if (Left == 3)
			{
				AddLine(FString::Printf(TEXT("%d PEOPLE STRANDED ON ROOFTOPS"), Manager->TotalPopulation), FLinearColor::White);
				AddLine(TEXT("REACH THEM AND SEND RESCUE SIGNALS"), FLinearColor(1,.85f,.4f));
				AddLine(TEXT("3 SIGNALS AVAILABLE — FLOOD APPROACHING"), FLinearColor::White);
			}
			if (GetOrientationSecondsRemaining() > 0)
			{
				AddLine(FString::Printf(TEXT("LOOK AROUND — COUNTDOWN STARTS IN %.0fs"), FMath::CeilToFloat(GetOrientationSecondsRemaining())), FLinearColor(.85f,.9f,1));
			}
			AddLine(TEXT("RMB Grapple | LMB Reel | Space Jump/Air Boost | E Signal"), FLinearColor(.85f,.9f,1));
			ASonHatEvacuationLink* Target = bWaitingForRetry ? nullptr : GetFocusedLink();
			if (Target)
			{
				InteractionPrompt = GetRescueBeaconTitle(Target);
				if (Target->IsActive()) { InteractionPrompt += TEXT("\nSIGNAL RECEIVED — GROUP REGISTERED FOR RESCUE"); }
				else if (IsTerminalSubmerged(Target)) { InteractionPrompt += TEXT("\nBEACON FLOODED — UNAVAILABLE"); InteractionColor = ColorSubmergedTerminal; }
				else if (Left <= 0) { InteractionPrompt += TEXT("\nNO SIGNALS LEFT"); }
				else { InteractionPrompt += TEXT("\nE — SEND RESCUE SIGNAL"); InteractionColor = FLinearColor(1,.8f,.3f); }
			}
		}
		else
		{
			AddLine(Phase == ESonHatEvacuationRoundPhase::FloodReveal ? TEXT("FLOOD ARRIVING — SIGNAL WINDOW CLOSED") : TEXT("RESCUE OPERATION COMPLETE"), FLinearColor::White);
			AddLine(FString::Printf(TEXT("%d / %d PEOPLE REGISTERED FOR RESCUE"), Registered, Manager->TotalPopulation), FLinearColor(.35f,1,.5f));
			AddLine(FString::Printf(TEXT("%d PEOPLE NOT REACHED"), Manager->TotalPopulation - Registered), FLinearColor(1,.6f,.3f));
			AddLine(FString::Printf(TEXT("%d / 3 SIGNALS USED"), 3 - Left), FLinearColor::White);
			AddLine(TEXT("Signals registered groups. Physical evacuation is not simulated."), FLinearColor(.8f,.85f,.9f));
			if (Phase == ESonHatEvacuationRoundPhase::Results)
			{
				AddLine(TEXT("3 SIGNALS. CHOOSE YOUR NEXT ROUTE — BEST POSSIBLE: 140/160"), FLinearColor(1,.85f,.4f));
				AddLine(TEXT("PRESS R TO TRY AGAIN"), FLinearColor(1,1,.5f));
			}
		}
		HudWidget->SetHudLines(MoveTemp(HudLines), HudLineColors);
		HudWidget->SetInteractionPrompt(InteractionPrompt, InteractionColor);
		UpdateTerminalWayfinding();
		return;
	}
	if (Phase == ESonHatEvacuationRoundPhase::Preparing && !Manager->bGameEnded)
	{
		const int32 Seconds = FMath::CeilToInt(Manager->GetRemainingTime());
		const float Orientation = GetOrientationSecondsRemaining();
		AddLine(Orientation > 0 ? FString::Printf(TEXT("THE FLOOD IS COMING | STARTS IN %.0fs"), FMath::CeilToFloat(Orientation))
			: FString::Printf(TEXT("FLOOD IN %02d:%02d  |  %d CONNECTIONS LEFT"), Seconds / 60, Seconds % 60,
			Manager->GetRemainingConnections()), bUrgent ? FLinearColor(1, 0.4f, 0.2f) : FLinearColor::White);
		AddLine(FString::Printf(TEXT("SAFE-ROUTE ACCESS   %d / %d residents"), Manager->EstimatedSafePopulation, Manager->TotalPopulation),
			Manager->EstimatedSafePopulation > 0 ? ColorSafeAccess : FLinearColor::White);
		if (bWaitingForRetry)
		{
			AddLine(TEXT("R: return to last rooftop. Round time keeps running."), FLinearColor(1, 0.65f, 0.3f));
		}
		else if (Manager->GetRemainingConnections() == Manager->MaxActivations)
		{
			AddLine(FString::Printf(TEXT("CONNECT SAFE ROUTES FOR %d ROOFTOP RESIDENTS"), Manager->TotalPopulation), FLinearColor(0.95f, 0.9f, 0.65f));
			AddLine(FString::Printf(TEXT("ONLY %d CONNECTIONS | %.0f-SECOND CHALLENGE"), Manager->MaxActivations, RoundPreparationDuration), FLinearColor::White);
			if (ASonHatEvacuationLink* First = GetSuggestedTerminal())
			{ AddLine(TEXT("FIRST GREEN CONSOLE: ") + SonHatEvacuationPresentation::PlayerLinkName(First).ToUpper() + TEXT(" [E]"), ColorSafeAccess); }
			else { AddLine(GetNextObjectiveText(), ColorUnsafe); }
		}
		else if (!bWaitingForRetry)
		{
			AddLine(GetNextObjectiveText(), FLinearColor(0.95f, 0.9f, 0.65f));
			if (GetSuggestedTerminal()) { AddLine(TEXT("FOLLOW THE GREEN MARKER. REACH ITS CONSOLE, PRESS E."), ColorSafeAccess); }
		}
		if (!bWaitingForRetry && Manager->GetRemainingConnections() == Manager->MaxActivations)
		{
			AddLine(TEXT("RMB Grapple | LMB Reel | Space Jump/Air Boost | E Connect"), FLinearColor(0.85f, 0.9f, 1));
		}
		ASonHatEvacuationLink* Target = bWaitingForRetry ? nullptr : GetFocusedLink();
		if (Target && IsTerminalSubmerged(Target))
		{
			InteractionPrompt = TEXT("Terminal underwater — choose another rooftop");
			InteractionColor = ColorSubmergedTerminal;
		}
		else if (Target)
		{
			InteractionPrompt = TEXT("ROUTE: ") + SonHatEvacuationPresentation::PlayerLinkName(Target);
			if (Target->IsActive())
			{
				InteractionPrompt += !Target->bFloodSafe ? TEXT("\nRISK CONNECTION USED — FAILS IN FLOOD")
					: Manager->HasSafeRoute(Target->NodeA) && Manager->HasSafeRoute(Target->NodeB)
					? TEXT("\nCONNECTED — SAFE ACCESS TO HIGH GROUND") : TEXT("\nSAFE CONNECTION SET — STILL NEEDS HIGH GROUND");
				InteractionColor = FLinearColor(0.75f, 0.85f, 0.8f);
			}
			else if (Manager->GetRemainingConnections() <= 0) { InteractionPrompt += TEXT("\nNo connections left"); }
			else
			{
				InteractionPrompt += Target->bFloodSafe ? TEXT("\n[E] CONNECT SAFE ROUTE | USES 1 CONNECTION")
					: TEXT("\n[E] RISK ROUTE | USES 1 CONNECTION, NO NEW SAFE ACCESS");
				InteractionColor = Target->bFloodSafe ? FLinearColor(0.35f, 1.f, 0.5f) : ColorUnsafe;
			}
		}
		if (StatusUntil > GetWorld()->GetTimeSeconds()) { AddLine(StatusMessage, StatusColor.ReinterpretAsLinear()); }
		ASonHatEvacuationLink* Closing = nullptr;
		for (ASonHatEvacuationLink* Link : RoundLinks)
		{
			if (IsValid(Link) && !Link->IsActive() && !IsTerminalSubmerged(Link) && IsTerminalFloodWarning(Link)
				&& (!Closing || GetTerminalSecondsUntilFlood(Link) < GetTerminalSecondsUntilFlood(Closing))) { Closing = Link; }
		}
		if (Closing)
		{
			AddLine(FString::Printf(TEXT("CLOSES IN %.0fs: %s"), FMath::CeilToFloat(GetTerminalSecondsUntilFlood(Closing)),
				*SonHatEvacuationPresentation::PlayerLinkName(Closing)), ColorUnsafe);
		}
	}
	else if (Phase == ESonHatEvacuationRoundPhase::FloodReveal)
	{
		AddLine(TEXT("FLOOD ARRIVING — your routes are locked"), FLinearColor(0.4f, 0.85f, 1.f));
		AddLine(FString::Printf(TEXT("Safe-route access: %d / %d residents"), Manager->FinalSafePopulation, Manager->TotalPopulation), ColorSafeAccess);
		AddLine(TEXT("Green: access to high ground | Red: no safe route"), FLinearColor::White);
	}
	else if (Phase == ESonHatEvacuationRoundPhase::Results || Manager->bGameEnded)
	{
		AddLine(TEXT("FLOOD RESULT — YOUR ROUTES MATTERED"), FLinearColor::White);
		AddLine(FString::Printf(TEXT("SAFE-ROUTE ACCESS: %d / %d residents"), Manager->FinalSafePopulation, Manager->TotalPopulation), FLinearColor(0.35f, 1.f, 0.5f));
		AddLine(FString::Printf(TEXT("LACKED SAFE ACCESS: %d residents"), Manager->TotalPopulation - Manager->FinalSafePopulation), FLinearColor(1.f, 0.5f, 0.3f));
		int32 FailedRoutes = 0;
		for (const ASonHatEvacuationLink* Link : RoundLinks) { FailedRoutes += IsValid(Link) && Link->IsActive() && !Link->bFloodSafe ? 1 : 0; }
		AddLine(FString::Printf(TEXT("CONNECTIONS USED: %d / %d  |  RISK ROUTES FAILED: %d"),
			Manager->MaxActivations - Manager->GetRemainingConnections(), Manager->MaxActivations, FailedRoutes), FLinearColor::White);
		AddLine(TEXT("Safe routes must connect rooftops to high ground."), FLinearColor(0.85f, 0.85f, 0.85f));
		if (FailedRoutes > 0) { AddLine(TEXT("RISK LINKS USED CHANCES WITHOUT ADDING SAFE ACCESS."), ColorUnsafe); }
		AddLine(TEXT("TRY AGAIN: SAFE CHOICES CONNECT DIFFERENT ROOFTOPS."), FLinearColor(0.75f, 0.8f, 0.85f));
		AddLine(FString::Printf(TEXT("R: restart the full %.0f-second round"), RoundPreparationDuration), FLinearColor(1, 1, 0.5f));
	}
	else { AddLine(StatusMessage, FLinearColor::White); }
	HudWidget->SetHudLines(MoveTemp(HudLines), HudLineColors);
	HudWidget->SetInteractionPrompt(InteractionPrompt, InteractionColor);
	UpdateTerminalWayfinding();
}

void ASonHatEvacuationDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Controller.IsValid()) { RemoveTickPrerequisiteActor(Controller.Get()); }
	StopRoundAmbience();
	StopRoundCues();
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
