#include "SonHatEvacuationLink.h"

#include "Components/SceneComponent.h"
#include "SonHatEvacuationNode.h"

ASonHatEvacuationLink::ASonHatEvacuationLink()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("InteractionTerminal")));
}

bool ASonHatEvacuationLink::HasValidEndpoints() const
{
	return IsValid(NodeA) && IsValid(NodeB) && NodeA != NodeB
		&& NodeA->GetWorld() == GetWorld() && NodeB->GetWorld() == GetWorld();
}
