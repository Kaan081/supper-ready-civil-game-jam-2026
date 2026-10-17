#include "SonHatEvacuationDemoBootstrap.h"

#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "SonHatEvacuationLink.h"
#include "SonHatEvacuationNode.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSonHatEvacuationDemo, Log, All);

namespace SonHatEvacuationDemo
{
	constexpr int32 Budget = 2;
	const TCHAR* TerminalNames[] = {TEXT("1  HUB <-> A"), TEXT("2  A <-> B"), TEXT("3  HUB <-> C"), TEXT("4  A <-> C")};
	const TCHAR* RouteDescriptions[] = {
		TEXT("SAFE: gives A (20) a route to the hub"),
		TEXT("SAFE: B (30) also needs A connected to the hub"),
		TEXT("SAFE: gives C (50) a route to the hub"),
		TEXT("UNSAFE: costs a link, carries NO safe evacuation route")};
}

ASonHatEvacuationDemoBootstrap::ASonHatEvacuationDemoBootstrap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DemoOrigin")));
	StageFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DemoDeck"));
	StageFloor->SetupAttachment(RootComponent);
	StageFloor->SetRelativeLocation(FVector(0, 0, -25));
	StageFloor->SetRelativeScale3D(FVector(26, 32, 0.5));
	StageFloor->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = Cube.Object;
	StageFloor->SetStaticMesh(CubeMesh);
}

FVector ASonHatEvacuationDemoBootstrap::DemoPosition(const FVector& LocalPosition) const
{
	return GetActorTransform().TransformPosition(LocalPosition);
}

void ASonHatEvacuationDemoBootstrap::BeginPlay()
{
	Super::BeginPlay();
	// Keep the deck and disjoint interaction radii consistent regardless of accidental editor scaling.
	SetActorScale3D(FVector::OneVector);
	SetActorRotation(FRotator(0, GetActorRotation().Yaw, 0));
	for (TActorIterator<ASonHatEvacuationDemoBootstrap> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->bNetworkCreated)
		{
			UE_LOG(LogSonHatEvacuationDemo, Warning, TEXT("Only one evacuation demo bootstrap is supported per world."));
			StageFloor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			StageFloor->SetVisibility(false);
			SetActorTickEnabled(false);
			return;
		}
	}
	bNetworkCreated = CreateNetwork();
	if (!bNetworkCreated || !CreateManager())
	{
		Feedback = TEXT("Demo setup failed. See LogSonHatEvacuationDemo in Output Log.");
		FeedbackColor = FColor::Red;
		UE_LOG(LogSonHatEvacuationDemo, Error, TEXT("Failed to spawn the evacuation demo network."));
	}
	HUDDrawHandle = UDebugDrawService::Register(TEXT("Game"),
		FDebugDrawDelegate::CreateUObject(this, &ASonHatEvacuationDemoBootstrap::DrawDemoHUD));
}

UTextRenderComponent* ASonHatEvacuationDemoBootstrap::AddLabel(const FVector& Position, float Size)
{
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
	AddInstanceComponent(Label);
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(Position);
	Label->SetWorldSize(Size);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->RegisterComponent();
	return Label;
}

void ASonHatEvacuationDemoBootstrap::AddMarker(const FVector& Position, const FVector& Scale)
{
	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
	AddInstanceComponent(Marker);
	Marker->SetupAttachment(RootComponent);
	Marker->SetStaticMesh(CubeMesh);
	Marker->SetRelativeLocation(Position);
	Marker->SetRelativeScale3D(Scale);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->RegisterComponent();
}

bool ASonHatEvacuationDemoBootstrap::CreateNetwork()
{
	const FVector NodePositions[] = {FVector(950, -1250, 100), FVector(950, -450, 100), FVector(950, 350, 100), FVector(950, 1200, 100)};
	const int32 Populations[] = {0, 20, 30, 50};
	const TCHAR* NodeNames[] = {TEXT("SAFE HUB"), TEXT("A: 20 civilians"), TEXT("B: 30 civilians"), TEXT("C: 50 civilians")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FTransform Transform(GetActorRotation(), DemoPosition(NodePositions[Index]));
		ASonHatEvacuationNode* Node = GetWorld()->SpawnActorDeferred<ASonHatEvacuationNode>(
			ASonHatEvacuationNode::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Node) { return false; }
		DemoNodes.Add(Node);
		Node->SetFlags(RF_Transient);
		Node->CivilianCount = Populations[Index];
		Node->bIsSafeHub = Index == 0;
		Node->FinishSpawning(Transform);
		AddMarker(NodePositions[Index] - FVector(0, 0, 40), FVector(1.2, 1.2, 1.2));
		UTextRenderComponent* Label = AddLabel(NodePositions[Index] + FVector(0, 0, 140), 27.0f);
		Label->SetText(FText::FromString(NodeNames[Index]));
		Label->SetTextRenderColor(Index == 0 ? FColor::Cyan : FColor::White);
		NodeLabels.Add(Label);
	}
	const int32 From[] = {0, 1, 0, 1};
	const int32 To[] = {1, 2, 3, 3};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector Position(0, -975 + 650 * Index, 100);
		const FTransform Transform(GetActorRotation(), DemoPosition(Position));
		ASonHatEvacuationLink* Link = GetWorld()->SpawnActorDeferred<ASonHatEvacuationLink>(
			ASonHatEvacuationLink::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Link) { return false; }
		DemoLinks.Add(Link);
		Link->SetFlags(RF_Transient);
		Link->NodeA = DemoNodes[From[Index]];
		Link->NodeB = DemoNodes[To[Index]];
		Link->bFloodSafe = Index != 3;
		Link->FinishSpawning(Transform);
		AddMarker(Position - FVector(0, 0, 45), FVector(0.8, 0.8, 1.1));
		TerminalLabels.Add(AddLabel(Position + FVector(0, 0, 140), 25.0f));
	}
	return true;
}

bool ASonHatEvacuationDemoBootstrap::CreateManager()
{
	if (IsValid(Manager))
	{
		Manager->OnRoundEnded.RemoveAll(this);
		Manager->Destroy(); // The manager's EndPlay releases/resets its links; no private state writes here.
	}
	const FTransform Transform(GetActorRotation(), GetActorLocation());
	Manager = GetWorld()->SpawnActorDeferred<ASonHatEvacuationManager>(
		ASonHatEvacuationManager::StaticClass(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Manager) { return false; }
	Manager->SetFlags(RF_Transient);
	// FinishSpawning can immediately invoke BeginPlay in a playing world. Configure everything FIRST.
	Manager->bAutoStartPreparation = false;
	Manager->Nodes = DemoNodes;
	Manager->CandidateLinks = DemoLinks;
	Manager->MaxActivations = SonHatEvacuationDemo::Budget;
	Manager->PreparationDuration = FMath::IsFinite(PreparationDuration) ? FMath::Clamp(PreparationDuration, 1.0f, 300.0f) : 45.0f;
	Manager->InteractionDistance = FMath::IsFinite(InteractionDistance) ? FMath::Clamp(InteractionDistance, 100.0f, 280.0f) : 250.0f;
	Manager->OnRoundEnded.AddDynamic(this, &ASonHatEvacuationDemoBootstrap::HandleRoundEnded);
	Manager->FinishSpawning(Transform);
	return true; // The first player-ready Tick calls StartPreparation, never the manager's BeginPlay.
}

bool ASonHatEvacuationDemoBootstrap::PreparePlayer()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!IsValid(PC) || !PC->IsLocalController()) { return false; }
	if (bPlayerReady && DemoController.Get() == PC && PC->GetPawn() == (FallbackCharacter ? FallbackCharacter.Get() : OriginalPawn.Get()))
	{
		return true;
	}
	DemoController = PC;
	OriginalPawn = PC->GetPawn();
	OriginalPlayerTransform = PC->GetPawn() ? PC->GetPawn()->GetActorTransform() : FTransform::Identity;
	OriginalControlRotation = PC->GetControlRotation();
	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	if (!Character)
	{
		// A blank level may supply a flying DefaultPawn. Supply a temporary walking pawn instead.
		const FTransform Transform(GetActorRotation(), DemoPosition(FVector(-800, 0, 105)));
		FallbackCharacter = GetWorld()->SpawnActorDeferred<ACharacter>(ACharacter::StaticClass(), Transform,
			this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!FallbackCharacter) { return false; }
		FallbackCharacter->SetFlags(RF_Transient);
		FallbackCharacter->bUseControllerRotationYaw = true;
		FallbackCharacter->GetCharacterMovement()->MaxWalkSpeed = 600.0f;
		FallbackCharacter->FinishSpawning(Transform);
		UCameraComponent* Camera = NewObject<UCameraComponent>(FallbackCharacter, TEXT("DemoFirstPersonCamera"));
		FallbackCharacter->AddInstanceComponent(Camera);
		Camera->SetupAttachment(FallbackCharacter->GetCapsuleComponent());
		Camera->SetRelativeLocation(FVector(0, 0, 64));
		Camera->bUsePawnControlRotation = true;
		Camera->RegisterComponent();
		PC->Possess(FallbackCharacter);
		if (PC->GetPawn() != FallbackCharacter) { return false; }
		PC->SetViewTarget(FallbackCharacter);
		Character = FallbackCharacter;
	}
	if (bMovePlayerToDemo || Character == FallbackCharacter)
	{
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Character->SetActorLocationAndRotation(DemoPosition(FVector(-800, 0, HalfHeight + 5)), GetActorRotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
		Character->GetCharacterMovement()->StopMovementImmediately();
		PC->SetControlRotation(GetActorRotation());
	}
	AddTickPrerequisiteActor(PC); // Read key edges after PlayerInput has processed this frame.
	bPlayerReady = true;
	return true;
}

void ASonHatEvacuationDemoBootstrap::UpdateFallbackMovement(float DeltaSeconds)
{
	APlayerController* PC = DemoController.Get();
	if (!IsValid(FallbackCharacter) || !PC || PC->GetPawn() != FallbackCharacter) { return; }
	const float Forward = (PC->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f);
	const float Right = (PC->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);
	const FRotator Yaw(0, PC->GetControlRotation().Yaw, 0);
	FallbackCharacter->AddMovementInput(Yaw.Vector(), Forward);
	FallbackCharacter->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Right);
	float MouseX = 0, MouseY = 0;
	PC->GetInputMouseDelta(MouseX, MouseY);
	FRotator Look = PC->GetControlRotation();
	Look.Yaw += MouseX * 0.15f;
	Look.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Look.Pitch) - MouseY * 0.15f, -80.0f, 80.0f);
	PC->SetControlRotation(Look);
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar)) { FallbackCharacter->Jump(); }
	if (PC->WasInputKeyJustReleased(EKeys::SpaceBar)) { FallbackCharacter->StopJumping(); }
}

int32 ASonHatEvacuationDemoBootstrap::FindNearestTerminal() const
{
	const APlayerController* PC = DemoController.Get();
	if (!PC || !IsValid(PC->GetPawn()) || !IsValid(Manager)) { return INDEX_NONE; }
	int32 Best = INDEX_NONE;
	double BestDistance = FMath::Square(static_cast<double>(Manager->InteractionDistance));
	for (int32 Index = 0; Index < DemoLinks.Num(); ++Index)
	{
		if (!IsValid(DemoLinks[Index])) { continue; }
		const double Distance = FVector::DistSquared(PC->GetPawn()->GetActorLocation(), DemoLinks[Index]->GetActorLocation());
		if (Distance <= BestDistance && (Best == INDEX_NONE || Distance < BestDistance))
		{
			Best = Index;
			BestDistance = Distance;
		}
	}
	return Best;
}

ESonHatEvacuationActivationResult ASonHatEvacuationDemoBootstrap::Interact()
{
	using EResult = ESonHatEvacuationActivationResult;
	APlayerController* PC = DemoController.Get();
	if (!PC || !IsValid(PC->GetPawn())) { return EResult::InvalidInteractor; }
	if (!IsValid(Manager)) { return EResult::InvalidNetwork; }
	const int32 Index = FindNearestTerminal();
	// Target the same terminal shown in the prompt, including when it is already active.
	const EResult Result = Manager->bGameEnded || Manager->GetRemainingTime() <= 0.0f
		? Manager->TryActivateLink(nullptr, PC->GetPawn())
		: Index == INDEX_NONE ? EResult::NoCandidateInRange : Manager->TryActivateLink(DemoLinks[Index], PC->GetPawn());
	FeedbackColor = Result == EResult::Success ? FColor::Green : FColor::Yellow;
	switch (Result)
	{
	case EResult::Success:
		Feedback = FString::Printf(TEXT("Prepared %s. %s | Safe now: %d/%d"), SonHatEvacuationDemo::TerminalNames[Index],
			DemoLinks[Index]->bFloodSafe ? TEXT("Safe link") : TEXT("UNSAFE - resource spent, no safe route!"),
			Manager->EstimatedSafePopulation, Manager->TotalPopulation);
		if (!DemoLinks[Index]->bFloodSafe) { FeedbackColor = FColor::Orange; }
		break;
	case EResult::AlreadyActive: Feedback = TEXT("Already prepared. No extra connection spent."); break;
	case EResult::BudgetExhausted: Feedback = TEXT("No connections left. F: evaluate now; R: try another plan."); break;
	case EResult::NoCandidateInRange:
	case EResult::TooFarAway: Feedback = TEXT("Move closer to ONE terminal until its [E] prompt appears."); break;
	case EResult::PreparationEnded: Feedback = TEXT("Preparation ended. Final result is frozen. R: new round."); break;
	default: Feedback = FString::Printf(TEXT("Activation rejected: %s"), *UEnum::GetValueAsString(Result)); break;
	}
	UE_LOG(LogSonHatEvacuationDemo, Display, TEXT("%s"), *Feedback);
	return Result;
}

void ASonHatEvacuationDemoBootstrap::RestartDemo()
{
	if (!bNetworkCreated) { return; }
	if (!CreateManager())
	{
		Feedback = TEXT("Could not restart the demo manager.");
		FeedbackColor = FColor::Red;
		return;
	}
	Feedback = TEXT("New round: choose TWO connections. Read SAFE / UNSAFE before E.");
	FeedbackColor = FColor::White;
	if (bPlayerReady)
	{
		if (APlayerController* PC = DemoController.Get())
		{
			if (ACharacter* Character = Cast<ACharacter>(PC->GetPawn()))
			{
				Character->SetActorLocation(DemoPosition(FVector(-800, 0,
					Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5)), false, nullptr, ETeleportType::TeleportPhysics);
				Character->GetCharacterMovement()->StopMovementImmediately();
				PC->SetControlRotation(GetActorRotation());
			}
		}
		Manager->StartPreparation();
	}
	UpdateVisuals();
}

void ASonHatEvacuationDemoBootstrap::HandleRoundEnded(int32 SafePopulation, int32 TotalPopulation)
{
	Feedback = FString::Printf(TEXT("ROUND ENDED: %d/%d civilians have a safe route. R: try a different plan."), SafePopulation, TotalPopulation);
	FeedbackColor = SafePopulation >= 70 ? FColor::Green : FColor::Orange;
	UE_LOG(LogSonHatEvacuationDemo, Display, TEXT("%s"), *Feedback);
}

void ASonHatEvacuationDemoBootstrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bNetworkCreated || !IsValid(Manager)) { return; }
	if (PreparePlayer())
	{
		if (!Manager->bHasStarted)
		{
			Feedback = TEXT("Choose TWO connections. Read SAFE / UNSAFE before pressing E.");
			Manager->StartPreparation();
		}
		APlayerController* PC = DemoController.Get();
		UpdateFallbackMovement(DeltaSeconds);
		// Raw key state works with Enhanced Input and requires no mapping context or input-stack changes.
		if (PC->WasInputKeyJustPressed(EKeys::R)) { RestartDemo(); }
		else if (PC->WasInputKeyJustPressed(EKeys::F)) { Manager->FinalizePreparation(); }
		else if (PC->WasInputKeyJustPressed(EKeys::E)) { Interact(); }
	}
	UpdateVisuals();
}

void ASonHatEvacuationDemoBootstrap::UpdateVisuals()
{
	FVector CameraLocation = DemoPosition(FVector(-800, 0, 160));
	FRotator CameraRotation;
	if (DemoController.IsValid()) { DemoController->GetPlayerViewPoint(CameraLocation, CameraRotation); }
	for (UTextRenderComponent* Label : NodeLabels)
	{
		if (IsValid(Label)) { Label->SetWorldRotation((CameraLocation - Label->GetComponentLocation()).Rotation()); }
	}
	for (int32 Index = 0; Index < DemoLinks.Num(); ++Index)
	{
		ASonHatEvacuationLink* Link = DemoLinks[Index];
		if (!IsValid(Link) || !TerminalLabels.IsValidIndex(Index)) { continue; }
		const FColor Color = !Link->bFloodSafe ? FColor::Red : Link->IsActive() ? FColor::Cyan : FColor::Green;
		UTextRenderComponent* Label = TerminalLabels[Index];
		const FString Text = FString::Printf(TEXT("%s\n%s\n%s"), SonHatEvacuationDemo::TerminalNames[Index],
			Link->bFloodSafe ? TEXT("SAFE") : TEXT("UNSAFE: NO EVACUATION ROUTE"),
			Link->IsActive() ? TEXT("PREPARED") : TEXT("[E] near terminal - costs 1"));
		if (!Label->Text.EqualTo(FText::FromString(Text))) { Label->SetText(FText::FromString(Text)); }
		Label->SetTextRenderColor(Color);
		Label->SetWorldRotation((CameraLocation - Label->GetComponentLocation()).Rotation());
		DrawDebugSphere(GetWorld(), Link->GetActorLocation(), 45, 12, Color, false, 0, 0, 3);
		if (Link->HasValidEndpoints())
		{
			DrawDebugLine(GetWorld(), Link->NodeA->GetActorLocation(), Link->NodeB->GetActorLocation(),
				Color, false, 0, 0, Link->IsActive() ? 6.0f : 1.0f);
		}
	}
}

void ASonHatEvacuationDemoBootstrap::DrawDemoHUD(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!Canvas || !GEngine || !PlayerController || PlayerController->GetWorld() != GetWorld()) { return; }
	const float Scale = FMath::Clamp(Canvas->ClipX / 1280.0f, 0.5f, 1.2f);
	FCanvasTileItem Backdrop(FVector2D(12, 12), FVector2D(Canvas->ClipX - 24, 268 * Scale), FLinearColor(0, 0, 0, 0.85f));
	Backdrop.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Backdrop);
	float Y = 22;
	const auto Line = [&](const FString& Text, const FColor& Color)
	{
		Canvas->SetDrawColor(Color);
		Canvas->DrawText(GEngine->GetMediumFont(), FStringView(Text), 24, Y, Scale, Scale);
		Y += 26 * Scale;
	};
	Line(TEXT("SON HAT | PREPARE BEFORE THE FLOOD"), FColor::Cyan);
	if (IsValid(Manager))
	{
		Line(FString::Printf(TEXT("Time: %.1fs | Connections: %d/2 | Safe estimate: %d/%d"), Manager->GetRemainingTime(),
			Manager->GetRemainingConnections(), Manager->EstimatedSafePopulation, Manager->TotalPopulation), FColor::White);
		if (Manager->bGameEnded)
		{
			Line(FString::Printf(TEXT("FINAL: %d/%d have safe route ACCESS | %d without access"), Manager->FinalSafePopulation,
				Manager->TotalPopulation, Manager->TotalPopulation - Manager->FinalSafePopulation), FColor::Yellow);
		}
		else { Line(TEXT("A:20 | B:30 (needs A-Hub) | C:50 | Choose TWO connections."), FColor::White); }
	}
	const int32 Nearby = FindNearestTerminal();
	Line(Nearby == INDEX_NONE ? TEXT("Approach a labeled terminal for its [E] prompt.")
		: FString::Printf(TEXT("[E] %s | %s"), SonHatEvacuationDemo::TerminalNames[Nearby], SonHatEvacuationDemo::RouteDescriptions[Nearby]),
		Nearby == 3 ? FColor::Orange : FColor::Green);
	Line(Feedback, FeedbackColor);
	Line(TEXT("Red UNSAFE links still spend a connection. Preparation choices change outcomes."), FColor::Orange);
	Line(TEXT("WASD + mouse: move/look | E: prepare | F: finish early | R: restart round"), FColor::White);
	Line(TEXT("Fictional pre-installed technology; not real-world flood evacuation advice."), FColor::Silver);
}

void ASonHatEvacuationDemoBootstrap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDDrawHandle.IsValid()) { UDebugDrawService::Unregister(HUDDrawHandle); }
	if (IsValid(Manager))
	{
		Manager->OnRoundEnded.RemoveAll(this);
		Manager->Destroy();
	}
	for (ASonHatEvacuationLink* Link : DemoLinks) { if (IsValid(Link)) { Link->Destroy(); } }
	for (ASonHatEvacuationNode* Node : DemoNodes) { if (IsValid(Node)) { Node->Destroy(); } }
	if (APlayerController* PC = DemoController.Get())
	{
		RemoveTickPrerequisiteActor(PC);
		if (EndPlayReason == EEndPlayReason::Destroyed && IsValid(FallbackCharacter)
			&& PC->GetPawn() == FallbackCharacter && OriginalPawn.IsValid())
		{
			PC->Possess(OriginalPawn.Get());
			PC->SetViewTarget(OriginalPawn.Get());
		}
		if (EndPlayReason == EEndPlayReason::Destroyed && OriginalPawn.IsValid() && PC->GetPawn() == OriginalPawn.Get())
		{
			OriginalPawn->SetActorTransform(OriginalPlayerTransform, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(OriginalControlRotation);
		}
	}
	if (IsValid(FallbackCharacter)) { FallbackCharacter->Destroy(); }
	Super::EndPlay(EndPlayReason);
}

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"

namespace SonHatEvacuationDemoTests
{
	struct FWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ASonHatEvacuationDemoBootstrap* Demo = nullptr;
		APlayerController* PC = nullptr;

		FWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
		}

		~FWorld()
		{
			if (IsValid(Demo)) { Demo->Destroy(); }
			World->EndPlay(EEndPlayReason::EndPlayInEditor);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		void AddPlayer(bool bCharacter)
		{
			PC = World->SpawnActor<APlayerController>();
			// Transient test worlds have no GameMode login to designate the local controller.
			PC->SetAsLocalPlayerController();
			if (!PC->PlayerInput) { PC->PlayerInput = NewObject<UPlayerInput>(PC); }
			APawn* Pawn = bCharacter ? static_cast<APawn*>(World->SpawnActor<ACharacter>())
				: static_cast<APawn*>(World->SpawnActor<ADefaultPawn>());
			PC->Possess(Pawn);
		}

		void Key(FKey Key, EInputEvent Event)
		{
			PC->PlayerInput->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, Key, Event, FPlatformTime::Cycles64()));
			PC->PlayerInput->ProcessInputStack(TArray<UInputComponent*>(), 1.0f / 60.0f, false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatDemoGraphTest, "SonHat.Evacuation.Demo.DeferredStartChoicesAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatDemoGraphTest::RunTest(const FString& Parameters)
{
	using namespace SonHatEvacuationDemoTests;
	using EResult = ESonHatEvacuationActivationResult;
	FWorld Fixture;
	Fixture.Demo = Fixture.World->SpawnActor<ASonHatEvacuationDemoBootstrap>();
	ASonHatEvacuationManager* Manager = Fixture.Demo->GetDemoManager();
	if (!TestNotNull(TEXT("Manager created"), Manager)) { return false; }
	TestFalse(TEXT("Manager BeginPlay did not start early"), Manager->bHasStarted);
	TestFalse(TEXT("Auto-start disabled before FinishSpawning"), Manager->bAutoStartPreparation);
	TestEqual(TEXT("Four configured nodes"), Manager->Nodes.Num(), 4);
	TestEqual(TEXT("Four configured terminals"), Manager->CandidateLinks.Num(), 4);
	Fixture.AddPlayer(true);
	Fixture.Demo->Tick(1.0f / 60.0f);
	TestTrue(TEXT("Player-ready tick starts round"), Manager->bHasStarted);
	TestEqual(TEXT("Total population"), Manager->TotalPopulation, 100);
	TestEqual(TEXT("Demo budget"), Manager->GetRemainingConnections(), 2);
	TestEqual(TEXT("Short demo duration"), Manager->GetRemainingTime(), 45.0f);
	for (int32 Index : {0, 2})
	{
		Fixture.PC->GetPawn()->SetActorLocation(Manager->CandidateLinks[Index]->GetActorLocation());
		TestEqual(TEXT("Safe terminal activates via facade"), Fixture.Demo->Interact(), EResult::Success);
	}
	Manager->FinalizePreparation();
	TestEqual(TEXT("Safe plan protects A and C"), Manager->FinalSafePopulation, 70);
	const TArray<TObjectPtr<ASonHatEvacuationLink>> Links = Manager->CandidateLinks;
	Fixture.Demo->RestartDemo();
	Manager = Fixture.Demo->GetDemoManager();
	TestEqual(TEXT("Restart restores budget through a new manager"), Manager->GetRemainingConnections(), 2);
	for (ASonHatEvacuationLink* Link : Links) { TestFalse(TEXT("Manager cleanup resets terminals"), Link->IsActive()); }
	for (int32 Index : {0, 3})
	{
		Fixture.PC->GetPawn()->SetActorLocation(Links[Index]->GetActorLocation());
		TestEqual(TEXT("Risk plan can be chosen"), Fixture.Demo->Interact(), EResult::Success);
	}
	Manager->FinalizePreparation();
	TestEqual(TEXT("Unsafe choice protects only A"), Manager->FinalSafePopulation, 20);
	TestEqual(TEXT("Unsafe option also spent budget"), Manager->GetRemainingConnections(), 0);
	Fixture.Demo->Destroy();
	TestFalse(TEXT("Bootstrap cleans up manager"), IsValid(Manager));
	for (ASonHatEvacuationLink* Link : Links) { TestFalse(TEXT("Bootstrap cleans up terminals"), IsValid(Link)); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSonHatDemoInputTest, "SonHat.Evacuation.Demo.RawEInputAndWalkingFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSonHatDemoInputTest::RunTest(const FString& Parameters)
{
	using namespace SonHatEvacuationDemoTests;
	FWorld Fixture;
	Fixture.AddPlayer(false);
	APawn* OriginalPawn = Fixture.PC->GetPawn();
	Fixture.Demo = Fixture.World->SpawnActor<ASonHatEvacuationDemoBootstrap>();
	Fixture.Demo->Tick(1.0f / 60.0f);
	ACharacter* WalkingPawn = Cast<ACharacter>(Fixture.PC->GetPawn());
	if (!TestNotNull(TEXT("Blank-level DefaultPawn gets a walking fallback"), WalkingPawn)) { return false; }
	ASonHatEvacuationManager* Manager = Fixture.Demo->GetDemoManager();
	if (!TestNotNull(TEXT("Manager exists"), Manager)) { return false; }
	WalkingPawn->SetActorLocation(Manager->CandidateLinks[0]->GetActorLocation());
	Fixture.Key(EKeys::E, IE_Pressed);
	TestTrue(TEXT("Raw E edge visible without a mapping context"), Fixture.PC->WasInputKeyJustPressed(EKeys::E));
	Fixture.Demo->Tick(1.0f / 60.0f);
	TestTrue(TEXT("E tick activates associated terminal"), Manager->CandidateLinks[0]->IsActive());
	TestEqual(TEXT("E spends one connection"), Manager->GetRemainingConnections(), 1);
	Fixture.Key(EKeys::E, IE_Released);
	Fixture.Key(EKeys::W, IE_Pressed);
	Fixture.Demo->Tick(1.0f / 60.0f);
	TestFalse(TEXT("Fallback supplies walking input"), WalkingPawn->GetPendingMovementInputVector().IsNearlyZero());
	Fixture.Key(EKeys::W, IE_Released);
	Fixture.Demo->Destroy();
	TestTrue(TEXT("Original pawn possession restored on demo removal"), Fixture.PC->GetPawn() == OriginalPawn);
	TestFalse(TEXT("Fallback pawn cleaned up"), IsValid(WalkingPawn));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
