#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SonHatPopulationPresenter.generated.h"

class ASonHatEvacuationDirector;
class ASonHatEvacuationManager;
class ASonHatEvacuationNode;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ESonHatPopulationVisualState : uint8
{
	Unavailable,
	Waiting,
	SafeAccess,
	FloodThreatened,
	Stranded
};

/** All placement is relative to the node, or to an optional designer-placed population anchor. */
USTRUCT(BlueprintType)
struct FSonHatPopulationPlacement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population")
	TObjectPtr<ASonHatEvacuationNode> Node;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population")
	TObjectPtr<AActor> Anchor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population")
	FVector Offset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population")
	float FacingYaw = 0;
	/** -1 uses the presenter's default. Visual figures never change CivilianCount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population", meta = (ClampMin = "-1", ClampMax = "16"))
	int32 Representatives = -1;
};

USTRUCT(BlueprintType)
struct FSonHatPopulationBuildingStatus
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	TObjectPtr<ASonHatEvacuationNode> Node;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	int32 Residents = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	int32 VisualRepresentatives = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	ESonHatPopulationVisualState State = ESonHatPopulationVisualState::Unavailable;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	bool bHasSafeAccess = false;
	/** Independent overlay: flood proximity never revokes a manager-verified safe route. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Population")
	bool bFloodThreatened = false;
};

USTRUCT()
struct FSonHatPopulationGroupVisual
{
	GENERATED_BODY()
	UPROPERTY(Transient) FSonHatPopulationBuildingStatus Status;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Bodies;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Heads;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
	TArray<FTransform> People;
	FString LastLabel;
	bool bPoseBuilt = false;
	double StateChangedAt = 0;
	double LastAnimatedAt = -1;
};

/** Read-only graph presentation. One actor, no Character actors, AI, collision, or per-person Tick. */
UCLASS(Blueprintable)
class UEINTROPROJECT_1_API ASonHatPopulationPresenter : public AActor
{
	GENERATED_BODY()
public:
	ASonHatPopulationPresenter();
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Recommended in multi-network levels. Replay managers are followed through GetRoundManager(). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Population|Source")
	TObjectPtr<ASonHatEvacuationDirector> Director;
	/** Optional standalone-manager mode, e.g. the bootstrap. No flood proximity without a Director. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Population|Source")
	TObjectPtr<ASonHatEvacuationManager> StandaloneManager;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Source")
	bool bAutoFindSource = true;
	/** Matches Director.NetworkTag during auto discovery. Never chooses between ambiguous sources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Source")
	FName NetworkTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement")
	bool bPreviewInEditor = false;
	/** Defaults to a small group beside the node marker. Override for roof edges, doors, balconies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement")
	FVector DefaultGroupOffset = FVector(250, 200, 0);
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Population|Placement")
	TArray<FSonHatPopulationPlacement> BuildingPlacements;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement", meta = (ClampMin = "1", ClampMax = "16"))
	int32 RepresentativesPerBuilding = 6;
	/** Hard runtime cap 256; caps figures only, never the authoritative population/summary. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxTotalRepresentatives = 128;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement", meta = (ClampMin = "1", ClampMax = "8"))
	int32 Columns = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement", meta = (ClampMin = "40", Units = "cm"))
	float Spacing = 90;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement", meta = (ClampMin = "50", ClampMax = "300", Units = "cm"))
	float ProxyHeight = 170;
	/** One-time WorldStatic floor traces at generation; proxies never obstruct traversal or aim rays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement")
	bool bProjectFeetToFloor = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement", meta = (ClampMin = "0", Units = "cm"))
	float ProjectionAbove = 300;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Placement", meta = (ClampMin = "1", Units = "cm"))
	float ProjectionBelow = 2500;

	/** Optional static human/silhouette mesh, origin at feet, authored height 170 cm. Not a skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Assets")
	TObjectPtr<UStaticMesh> RepresentativeMesh;
	/** Default original low-poly, feet-origin 170 cm humans. Existing placements need no map edits.
	 * RepresentativeMesh overrides this pose set when custom art is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Assets|Humans")
	TObjectPtr<UStaticMesh> HumanWaitingMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Assets|Humans")
	TObjectPtr<UStaticMesh> HumanSafeMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Assets|Humans")
	TObjectPtr<UStaticMesh> HumanThreatMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Assets|Humans")
	TObjectPtr<UStaticMesh> HumanStrandedMesh;
	/** Dedicated project material for the built-in humans; old PopulationMaterial overrides stay compatible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Assets|Humans")
	TObjectPtr<UMaterialInterface> HumanMaterial;
	/** Default engine unlit color material; replace with a cooked project material with this vector parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Assets")
	TObjectPtr<UMaterialInterface> PopulationMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Assets")
	FName ColorParameter = TEXT("Color");

	/** One 20 Hz timer / batch per building. No NPC actors, physics or per-person Tick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Reactions")
	bool bAnimateRepresentatives = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Reactions", meta = (ClampMin = "0.1", Units = "s"))
	float SafeReactionDuration = 1.25f;
	/** Beyond this distance groups retain their state pose without updating idle transforms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Reactions", meta = (ClampMin = "100", Units = "cm"))
	float AnimationDistance = 4500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability")
	FLinearColor WaitingColor = FLinearColor(0.8f, 0.7f, 0.3f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability")
	FLinearColor SafeAccessColor = FLinearColor(0.1f, 1, 0.3f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability")
	FLinearColor ThreatColor = FLinearColor(1, 0.3f, 0.02f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability")
	FLinearColor StrandedColor = FLinearColor(1, 0.04f, 0.15f);
	/** Threat reference is the group's foot elevation, not the arbitrary node marker height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability", meta = (ClampMin = "0", Units = "cm"))
	float FloodWarningClearance = 250;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability", meta = (ClampMin = "10", Units = "cm"))
	float LabelWorldSize = 20;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability", meta = (Units = "cm"))
	float LabelHeight = 220;
	/** Off by default — evacuation HUD carries player-facing status; enable for debug layouts only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability")
	bool bShowBuildingWorldLabels = false;
	/** World-space result board at this actor. Uses manager totals; does not create another screen HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability")
	bool bShowNetworkBoard = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Readability")
	FVector BoardOffset = FVector(0, 0, 250);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Population")
	void RebuildPresentation();
	UFUNCTION(BlueprintCallable, Category = "Population")
	void RefreshPresentation();
	UFUNCTION(BlueprintPure, Category = "Population")
	TArray<FSonHatPopulationBuildingStatus> GetBuildingStatuses() const;
	UFUNCTION(BlueprintPure, Category = "Population")
	FString GetNetworkSummary() const { return NetworkSummary; }
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Population")
	FString StatusMessage;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION() void HandleNetworkChanged();
	void ResolveSource();
	void BindManager(ASonHatEvacuationManager* NewManager);
	void BuildGroups();
	void ClearGroups();
	void BuildPose(FSonHatPopulationGroupVisual& Group);
	void AnimateGroup(FSonHatPopulationGroupVisual& Group, double Now);
	UStaticMesh* SelectHumanMesh(ESonHatPopulationVisualState State) const;
	UInstancedStaticMeshComponent* CreateInstances(UStaticMesh* Mesh);
	UTextRenderComponent* CreateLabel();
	FLinearColor StateColor(ESonHatPopulationVisualState State) const;
	UPROPERTY(Transient) TArray<FSonHatPopulationGroupVisual> Groups;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> NetworkBoard;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> TextMaterial;
	TWeakObjectPtr<ASonHatEvacuationDirector> ObservedDirector;
	TWeakObjectPtr<ASonHatEvacuationManager> BoundManager;
	FTimerHandle RefreshTimer;
	FString NetworkSummary;
	FString LastBoardText;
	bool bAmbiguousSource = false;
	bool bRefreshing = false;
	bool bGroupsFromStartedManager = false;
	bool bFinalSnapshot = false;
	double AnimationStartedAt = 0;
};
