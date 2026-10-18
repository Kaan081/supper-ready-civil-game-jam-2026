#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "SonHatEvacuationManager.generated.h"

class ASonHatEvacuationLink;
class ASonHatEvacuationNode;

UENUM(BlueprintType)
enum class ESonHatEvacuationActivationResult : uint8
{
	Success,
	NotStarted,
	PreparationEnded,
	InvalidInteractor,
	InvalidLink,
	LinkNotRegistered,
	LinkOwnedByAnotherManager,
	AlreadyActive,
	TooFarAway,
	NoCandidateInRange,
	BudgetExhausted,
	InvalidNetwork
};

UENUM(BlueprintType)
enum class ESonHatEvacuationConfiguration : uint8
{
	NotStarted,
	Ready,
	NoValidNodes,
	MissingHub,
	MultipleHubs
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSonHatEvacuationStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSonHatEvacuationRoundEnded, int32, SafePopulation, int32, TotalPopulation);

/** Single-player round authority. Configure the graph before StartPreparation; restart via a new PIE round. */
UCLASS(Blueprintable)
class UEINTROPROJECT_1_API ASonHatEvacuationManager : public AActor
{
	GENERATED_BODY()

public:
	ASonHatEvacuationManager();

	/** Explicit level references. Duplicate/null entries are ignored. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Son Hat|Network")
	TArray<TObjectPtr<ASonHatEvacuationNode>> Nodes;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Son Hat|Network")
	TArray<TObjectPtr<ASonHatEvacuationLink>> CandidateLinks;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Preparation", meta = (ClampMin = "0"))
	int32 MaxActivations = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Preparation", meta = (ClampMin = "0.0", Units = "s"))
	float PreparationDuration = 120.0f;

	/** Full 3D distance from interactor origin to link actor origin, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Preparation", meta = (ClampMin = "0.0", Units = "cm"))
	float InteractionDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Preparation")
	bool bAutoStartPreparation = true;

	/** Starts once. Returns false for repeat calls or no world. Check ConfigurationStatus for layout errors. */
	UFUNCTION(BlueprintCallable, Category = "Son Hat|Evacuation")
	bool StartPreparation();

	/** Chooses nearest inactive terminal in range; array order breaks exact distance ties. */
	UFUNCTION(BlueprintCallable, Category = "Son Hat|Evacuation")
	ESonHatEvacuationActivationResult TryActivateNearestLink(AActor* Interactor, ASonHatEvacuationLink*& OutLink);

	/** Same checks as nearest interaction, also useful for a targeted Blueprint prompt. */
	UFUNCTION(BlueprintCallable, Category = "Son Hat|Evacuation")
	ESonHatEvacuationActivationResult TryActivateLink(ASonHatEvacuationLink* Link, AActor* Interactor);

	/** Ends early if requested; also called automatically at the deadline. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "Son Hat|Evacuation")
	void FinalizePreparation();

	UFUNCTION(BlueprintPure, Category = "Son Hat|Evacuation")
	float GetRemainingTime() const;

	/** Extends the preparation deadline (e.g. while the player acknowledges a fall retry). */
	void ExtendPreparationDeadline(float ExtraSeconds);

	UFUNCTION(BlueprintPure, Category = "Son Hat|Evacuation")
	int32 GetRemainingConnections() const { return FMath::Max(0, RoundActivationBudget - UsedActivations); }

	/** Authoritative result of the last graph evaluation; frozen with the final round result. */
	UFUNCTION(BlueprintPure, Category = "Son Hat|Evacuation")
	bool HasSafeRoute(ASonHatEvacuationNode* Node) const;

	/** Allows level integration to detect another manager claiming the same connection. */
	UFUNCTION(BlueprintPure, Category = "Son Hat|Evacuation")
	bool OwnsLink(ASonHatEvacuationLink* Link) const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Son Hat|State")
	int32 EstimatedSafePopulation = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Son Hat|State")
	int32 TotalPopulation = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Son Hat|State")
	int32 FinalSafePopulation = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Son Hat|State")
	bool bHasStarted = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Son Hat|State")
	bool bGameEnded = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Son Hat|State")
	ESonHatEvacuationConfiguration ConfigurationStatus = ESonHatEvacuationConfiguration::NotStarted;

	/** Fires on start, successful activation, and every 0.1 seconds during preparation. */
	UPROPERTY(BlueprintAssignable, Category = "Son Hat|Evacuation")
	FSonHatEvacuationStateChanged OnStateChanged;

	/** Fires once, after final values have been frozen. */
	UPROPERTY(BlueprintAssignable, Category = "Son Hat|Evacuation")
	FSonHatEvacuationRoundEnded OnRoundEnded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshPreparation();
	void EvaluateNetwork();
	bool IsUsableLink(const ASonHatEvacuationLink* Link) const;
	bool CheckDeadline();
	bool IsValidInteractor(const AActor* Interactor) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASonHatEvacuationNode>> RegisteredNodes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASonHatEvacuationLink>> RegisteredLinks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASonHatEvacuationNode>> NodesWithSafeAccess;

	FTimerHandle PreparationUpdateTimer;
	double PreparationDeadline = 0.0;
	int32 RoundActivationBudget = 0;
	int32 UsedActivations = 0;
};
