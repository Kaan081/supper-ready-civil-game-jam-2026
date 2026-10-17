// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrappleComponent.generated.h"

class ACharacter;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UEINTROPROJECT_1_API UGrappleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrappleComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Line trace from the player camera; attaches to the first visible GrappleAnchor in range. */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	bool TryStartGrapple();

	/** Detaches without changing velocity. Safe to call when not grappling. */
	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void StopGrapple();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Grapple")
	bool IsGrappling() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Targeting", meta = (ClampMin = "100.0"))
	float GrappleRange = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Targeting")
	FName GrappleAnchorTag = TEXT("GrappleAnchor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "0.0"))
	float PullAcceleration = 3200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "100.0"))
	float MaxGrappleSpeed = 2200.f;

	/** Soft maximum distance from the anchor while attached; stretched rope adds extra inward pull. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "100.0"))
	float RopeLength = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "0.0"))
	float RopeStretchPullAcceleration = 4800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Movement", meta = (ClampMin = "0.0"))
	float AirSteeringAcceleration = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple|Debug")
	bool bDrawDebug = false;

protected:
	bool PerformGrappleTrace(FHitResult& OutHit) const;
	ACharacter* GetOwningCharacter() const;
	void ApplyGrappleMovement(float DeltaTime);
	void ClearGrappleState();

	UPROPERTY(Transient)
	TObjectPtr<AActor> GrappleAnchorActor;

	FVector GrappleAnchorWorldPoint = FVector::ZeroVector;
	bool bIsGrapplingActive = false;
};
