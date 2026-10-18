#pragma once

#include "CoreMinimal.h"

/** Small deterministic helpers shared by the component and its tests. Gravity is integrated by CMC. */
namespace SonHatGrapplePhysics
{
	struct FSettings
	{
		float MinimumLength = 350;
		float SpeedLimit = 2600;
		float SteeringAcceleration = 1400;
		float ReelSpeed = 650;
		float ReelAcceleration = 2400;
		float ReelPullSpeed = 1600;
		float ConstraintAcceleration = 12000;
		float RecoverySpeed = 1400;
		float TautTolerance = 5;
	};

	struct FStep
	{
		FVector Velocity = FVector::ZeroVector;
		float RopeLength = 350;
		float WinchSpeed = 0;
		/** Diagnostic radial control accelerations (cm/s^2), before final total-speed limiting.
		 * Gravity is integrated separately by CMC. Constraint acceleration is rope tension, not a motor. */
		float ReelAccelerationApplied = 0;
		float ConstraintAccelerationApplied = 0;
	};

	inline FVector RemoveOutwardVelocity(FVector Velocity, const FVector& InwardDirection)
	{
		const FVector D = InwardDirection.GetSafeNormal();
		return Velocity - D * FMath::Min(0.0, FVector::DotProduct(Velocity, D));
	}

	inline FVector LimitSpeedPreservingTangent(const FVector& Tangent, double RadialSpeed, const FVector& D, float Limit)
	{
		const FVector LimitedTangent = Tangent.GetClampedToMaxSize(FMath::Max(0.0f, Limit));
		const double RadialBudget = FMath::Sqrt(FMath::Max(0.0, FMath::Square(static_cast<double>(Limit)) - LimitedTangent.SizeSquared()));
		return LimitedTangent + D * FMath::Clamp(RadialSpeed, -RadialBudget, RadialBudget);
	}

	inline FStep Step(const FVector& InitialVelocity, const FVector& ToAnchor, const FVector& Gravity,
		const FVector& WorldInput, float DeltaTime, float RopeLength, float WinchSpeed, bool bReel, const FSettings& Settings)
	{
		FStep Result;
		Result.RopeLength = FMath::IsFinite(RopeLength) ? FMath::Max(Settings.MinimumLength, RopeLength) : Settings.MinimumLength;
		Result.Velocity = InitialVelocity.ContainsNaN() ? FVector::ZeroVector : InitialVelocity.GetClampedToMaxSize(Settings.SpeedLimit);
		if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0 || ToAnchor.ContainsNaN() || Gravity.ContainsNaN() || ToAnchor.SizeSquared() < 1)
		{
			return Result;
		}
		// Bound control/rope changes during hitches. CharacterMovement keeps its own collision substeps.
		const float Dt = FMath::Min(DeltaTime, 0.05f);
		const double Distance = ToAnchor.Size();
		const FVector D = ToAnchor / Distance;
		double RadialSpeed = FVector::DotProduct(Result.Velocity, D);
		FVector Tangent = Result.Velocity - D * RadialSpeed;
		const FVector Input = WorldInput.ContainsNaN() ? FVector::ZeroVector : WorldInput.GetClampedToMaxSize(1);
		Tangent += FVector::VectorPlaneProject(Input, D) * FMath::Max(0.0f, Settings.SteeringAcceleration) * Dt;
		Tangent = Tangent.GetClampedToMaxSize(Settings.SpeedLimit);
		const double RadialBudget = FMath::Sqrt(FMath::Max(0.0, FMath::Square(static_cast<double>(Settings.SpeedLimit)) - Tangent.SizeSquared()));
		const bool bNearTaut = Distance >= Result.RopeLength - Settings.TautTolerance;
		const double BeforeReel = RadialSpeed;
		if (bReel && Result.RopeLength > Settings.MinimumLength)
		{
			Result.WinchSpeed = FMath::FInterpConstantTo(FMath::Max(0.0f, WinchSpeed), FMath::Max(0.0f, Settings.ReelSpeed), Dt,
				FMath::Max(0.0f, Settings.ReelAcceleration));
			if (bNearTaut)
			{
				// Do not demand faster rope retraction than radial speed/acceleration budgets allow.
				Result.WinchSpeed = FMath::Min(Result.WinchSpeed, static_cast<float>(FMath::Max(0.0,
					FMath::Min(RadialBudget, RadialSpeed + Settings.ReelAcceleration * Dt))));
				RadialSpeed += FMath::Clamp(static_cast<double>(Result.WinchSpeed) - RadialSpeed, 0.0,
					static_cast<double>(Settings.ReelAcceleration * Dt));
			}
			Result.RopeLength = FMath::Max(Settings.MinimumLength, Result.RopeLength - Result.WinchSpeed * Dt);
		}
		// Arcade reel motor is LMB-only, including slack take-up. Never brake faster
		// earned inward motion, never spend tangential speed, and stop powering inside
		// the minimum radius. Zero retains the original tension-only winch behavior.
		if (bReel && Settings.ReelPullSpeed > 0 && Distance > Settings.MinimumLength)
		{
			const double Budget = FMath::Max(0.0, static_cast<double>(Settings.ReelAcceleration * Dt) - (RadialSpeed - BeforeReel));
			RadialSpeed += FMath::Clamp(static_cast<double>(Settings.ReelPullSpeed) - RadialSpeed, 0.0, Budget);
		}
		Result.ReelAccelerationApplied = static_cast<float>(RadialSpeed - BeforeReel) / Dt;

		// Solve the end-of-step radial bound while retaining the tangential component. This is
		// tension only: inward travel/slack is free, and RMB alone supplies no constant pull.
		const double GravityRadial = FVector::DotProduct(Gravity, D);
		const FVector TangentialDisplacement = Tangent * Dt + FVector::VectorPlaneProject(Gravity, D) * (0.5 * Dt * Dt);
		const double RadialReach = FMath::Sqrt(FMath::Max(0.0,
			FMath::Square(static_cast<double>(Result.RopeLength)) - TangentialDisplacement.SizeSquared()));
		const double MinimumRadialSpeed = (Distance - RadialReach) / Dt - 0.5 * GravityRadial * Dt;
		if (RadialSpeed < MinimumRadialSpeed)
		{
			const double BeforeTension = RadialSpeed;
			const double Target = FMath::Min(MinimumRadialSpeed, static_cast<double>(Settings.RecoverySpeed));
			RadialSpeed += FMath::Clamp(Target - RadialSpeed, 0.0,
				static_cast<double>(FMath::Max(0.0f, Settings.ConstraintAcceleration) * Dt));
			Result.ConstraintAccelerationApplied = static_cast<float>(RadialSpeed - BeforeTension) / Dt;
		}
		Result.Velocity = LimitSpeedPreservingTangent(Tangent, RadialSpeed, D, Settings.SpeedLimit);
		return Result;
	}
}
