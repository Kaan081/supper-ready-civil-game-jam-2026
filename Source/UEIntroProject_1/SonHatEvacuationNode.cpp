#include "SonHatEvacuationNode.h"

#include "Components/SceneComponent.h"

ASonHatEvacuationNode::ASonHatEvacuationNode()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BuildingLocation")));
}
