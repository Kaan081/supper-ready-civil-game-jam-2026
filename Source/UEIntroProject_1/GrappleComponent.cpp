#include "GrappleComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

UGrappleComponent::UGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UGrappleComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* Character = GetOwningCharacter())
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			// Character movement should run after this component adjusts velocity for the frame.
			MoveComp->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
		}
	}
}

void UGrappleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsGrapplingActive)
	{
		return;
	}

	if (!IsValid(GrappleAnchorActor))
	{
		ClearGrappleState();
		return;
	}

	ApplyGrappleMovement(DeltaTime);

	if (bDrawDebug)
	{
		if (const ACharacter* Character = GetOwningCharacter())
		{
			DrawDebugLine(
				GetWorld(),
				Character->GetActorLocation(),
				GrappleAnchorWorldPoint,
				FColor::Cyan,
				false,
				-1.f,
				0,
				2.f);
		}
	}
}

bool UGrappleComponent::TryStartGrapple()
{
	FHitResult Hit;
	if (!PerformGrappleTrace(Hit))
	{
		return false;
	}

	AActor* HitActor = Hit.GetActor();
	if (!IsValid(HitActor) || !HitActor->ActorHasTag(GrappleAnchorTag))
	{
		return false;
	}

	GrappleAnchorActor = HitActor;
	GrappleAnchorWorldPoint = Hit.ImpactPoint;
	bIsGrapplingActive = true;
	return true;
}

void UGrappleComponent::StopGrapple()
{
	ClearGrappleState();
}

bool UGrappleComponent::IsGrappling() const
{
	return bIsGrapplingActive && IsValid(GrappleAnchorActor);
}

ACharacter* UGrappleComponent::GetOwningCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

bool UGrappleComponent::PerformGrappleTrace(FHitResult& OutHit) const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		return false;
	}

	const AController* Controller = Character->GetController();
	if (!Controller)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * GrappleRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GrappleTrace), false, Character);
	QueryParams.AddIgnoredActor(Character);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->LineTraceSingleByChannel(
		OutHit,
		ViewLocation,
		TraceEnd,
		ECC_Visibility,
		QueryParams);
}

void UGrappleComponent::ApplyGrappleMovement(float DeltaTime)
{
	ACharacter* Character = GetOwningCharacter();
	if (!Character)
	{
		ClearGrappleState();
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		ClearGrappleState();
		return;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	FVector ToAnchor = GrappleAnchorWorldPoint - CharacterLocation;
	const float DistanceToAnchor = ToAnchor.Size();
	if (DistanceToAnchor < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector RadialIn = ToAnchor / DistanceToAnchor;

	FVector Velocity = MoveComp->Velocity;
	const float RadialSpeed = FVector::DotProduct(Velocity, RadialIn);
	FVector TangentialVelocity = Velocity - RadialIn * RadialSpeed;

	float NewRadialSpeed = RadialSpeed + PullAcceleration * DeltaTime;

	if (DistanceToAnchor > RopeLength)
	{
		const float Stretch = DistanceToAnchor - RopeLength;
		NewRadialSpeed += Stretch * (RopeStretchPullAcceleration / RopeLength) * DeltaTime;
	}

	if (DistanceToAnchor >= RopeLength && NewRadialSpeed < 0.f)
	{
		NewRadialSpeed = 0.f;
	}

	FVector NewVelocity = TangentialVelocity + RadialIn * NewRadialSpeed;

	if (AirSteeringAcceleration > 0.f)
	{
		const FVector LocalInput = Character->GetLastMovementInputVector();
		if (!LocalInput.IsNearlyZero())
		{
			FRotator YawOnly(0.f, Character->GetControlRotation().Yaw, 0.f);
			const FVector WorldSteer = YawOnly.RotateVector(LocalInput.GetSafeNormal());
			NewVelocity += WorldSteer * AirSteeringAcceleration * DeltaTime;
		}
	}

	const float Speed = NewVelocity.Size();
	if (Speed > MaxGrappleSpeed)
	{
		NewVelocity *= MaxGrappleSpeed / Speed;
	}

	MoveComp->Velocity = NewVelocity;
}

void UGrappleComponent::ClearGrappleState()
{
	bIsGrapplingActive = false;
	GrappleAnchorActor = nullptr;
	GrappleAnchorWorldPoint = FVector::ZeroVector;
}
