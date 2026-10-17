#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SonHatEvacuationNode.generated.h"

/** A building in the fictional, pre-installed evacuation network. */
UCLASS(Blueprintable)
class UEINTROPROJECT_1_API ASonHatEvacuationNode : public AActor
{
	GENERATED_BODY()

public:
	ASonHatEvacuationNode();

	/** Includes residents already at the hub. Negative values are treated as zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Evacuation", meta = (ClampMin = "0"))
	int32 CivilianCount = 0;

	/** Configure exactly one hub in the manager's Nodes array. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Son Hat|Evacuation")
	bool bIsSafeHub = false;
};
