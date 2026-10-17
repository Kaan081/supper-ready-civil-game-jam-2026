#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SonHatEvacuationLink.generated.h"

class ASonHatEvacuationManager;
class ASonHatEvacuationNode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSonHatEvacuationLinkChanged, bool, bActive);

/** Candidate edge. Its actor origin is the interaction terminal, not the edge midpoint. */
UCLASS(Blueprintable)
class UEINTROPROJECT_1_API ASonHatEvacuationLink : public AActor
{
	GENERATED_BODY()

public:
	ASonHatEvacuationLink();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Son Hat|Evacuation")
	TObjectPtr<ASonHatEvacuationNode> NodeA;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Son Hat|Evacuation")
	TObjectPtr<ASonHatEvacuationNode> NodeB;

	/** Designer-authored fictional technology rating; never inferred from building height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Evacuation")
	bool bFloodSafe = true;

	UFUNCTION(BlueprintPure, Category = "Son Hat|Evacuation")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Son Hat|Evacuation")
	bool HasValidEndpoints() const;

	UPROPERTY(BlueprintAssignable, Category = "Son Hat|Evacuation")
	FSonHatEvacuationLinkChanged OnActivationChanged;

private:
	friend class ASonHatEvacuationManager;

	UPROPERTY(Transient)
	bool bActive = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASonHatEvacuationManager> NetworkOwner;
};
