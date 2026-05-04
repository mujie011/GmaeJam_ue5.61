// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Character/GsPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UEGameJam.h"

namespace
{
	const FName WallRunSurfaceTag(TEXT("WallRunSurface"));

	bool TryGetWallRunSurfaceNormal(const FHitResult& Hit, FVector& OutWallNormal)
	{
		const AActor* HitActor = Hit.GetActor();
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		const bool bHasWallRunSurfaceTag =
			(HitActor && HitActor->ActorHasTag(WallRunSurfaceTag))
			|| (HitComponent && HitComponent->ComponentHasTag(WallRunSurfaceTag));
		if (!bHasWallRunSurfaceTag)
		{
			return false;
		}

		FVector WallNormal = Hit.ImpactNormal.IsNearlyZero() ? Hit.Normal : Hit.ImpactNormal;
		WallNormal = FVector::VectorPlaneProject(WallNormal, FVector::UpVector);
		if (!WallNormal.Normalize())
		{
			return false;
		}

		OutWallNormal = WallNormal;
		return true;
	}
}

void AGsPlayer::StartWallRunDetectionDelay()
{
	bCanCheckWallRun = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		EnableWallRunDetection();
		return;
	}

	World->GetTimerManager().ClearTimer(WallRunDetectionDelayTimer);

	const float WallRunCheckDelay = GetPlayerTuning().WallRunCheckDelay;
	if (WallRunCheckDelay <= KINDA_SMALL_NUMBER)
	{
		EnableWallRunDetection();
		return;
	}

	World->GetTimerManager().SetTimer(
		WallRunDetectionDelayTimer,
		this,
		&AGsPlayer::EnableWallRunDetection,
		WallRunCheckDelay,
		false);
}

void AGsPlayer::EnableWallRunDetection()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WallRunDetectionDelayTimer);
	}

	bCanCheckWallRun = true;
}

void AGsPlayer::ResetWallRunDetection()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WallRunDetectionDelayTimer);
	}

	bCanCheckWallRun = false;
	bHasTriggeredWallRunThisJump = false;
}

void AGsPlayer::UpdateWallRunDetection()
{
	if (!bCanCheckWallRun || bHasTriggeredWallRunThisJump || bIsDead)
	{
		return;
	}

	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	if (!PlayerMovementComponent || !PlayerMovementComponent->IsFalling())
	{
		return;
	}

	FHitResult WallHit;
	FVector WallNormal = FVector::ZeroVector;
	if (!TryFindWallRunSurface(WallHit, WallNormal) || !CanTriggerWallRun(WallNormal))
	{
		return;
	}

	if (!StartWallRun(WallNormal))
	{
		return;
	}

	const bool bIsRightWall = FVector::DotProduct(GetActorRightVector(), -WallNormal) >= 0.0f;
	const FString DebugMessage = FString::Printf(TEXT("Wall Run Triggered: %s"), bIsRightWall ? TEXT("Right") : TEXT("Left"));

	UE_LOG(LogUEGameJam, Log, TEXT("%s"), *DebugMessage);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, DebugMessage);
	}
}

bool AGsPlayer::TryFindWallRunSurface(FHitResult& OutWallHit, FVector& OutWallNormal) const
{
	OutWallHit = FHitResult();
	OutWallNormal = FVector::ZeroVector;

	const UCapsuleComponent* PlayerCapsuleComponent = GetCapsuleComponent();
	UWorld* World = GetWorld();
	if (!PlayerCapsuleComponent || !World || GetPlayerTuning().WallRunSideTraceDistance <= 0.0f)
	{
		return false;
	}

	const FVector TraceStart = GetActorLocation();
	const FVector RightDirection = GetActorRightVector().GetSafeNormal2D();
	if (RightDirection.IsNearlyZero())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallRunSideTrace), false, this);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult BestHit;
	FVector BestNormal = FVector::ZeroVector;
	float BestDistance = TNumericLimits<float>::Max();

	const auto TryTraceSide = [&](const FVector& TraceDirection)
	{
		TArray<FHitResult> SideHits;
		const FVector TraceEnd = TraceStart + (TraceDirection * GetPlayerTuning().WallRunSideTraceDistance);
		if (!World->LineTraceMultiByObjectType(SideHits, TraceStart, TraceEnd, ObjectQueryParams, QueryParams))
		{
			return;
		}

		for (const FHitResult& SideHit : SideHits)
		{
			FVector WallNormal = FVector::ZeroVector;
			if (!TryGetWallRunSurfaceNormal(SideHit, WallNormal))
			{
				continue;
			}

			if (SideHit.Distance < BestDistance)
			{
				BestHit = SideHit;
				BestNormal = WallNormal;
				BestDistance = SideHit.Distance;
			}
		}
	};

	TryTraceSide(RightDirection);
	TryTraceSide(-RightDirection);

	if (BestDistance == TNumericLimits<float>::Max())
	{
		return false;
	}

	OutWallHit = BestHit;
	OutWallNormal = BestNormal;
	return true;
}

bool AGsPlayer::TryFindWallRunSurfaceAlongNormal(const FVector& ExpectedWallNormal, FHitResult& OutWallHit, FVector& OutWallNormal) const
{
	OutWallHit = FHitResult();
	OutWallNormal = FVector::ZeroVector;

	const UCapsuleComponent* PlayerCapsuleComponent = GetCapsuleComponent();
	UWorld* World = GetWorld();
	if (!PlayerCapsuleComponent || !World || GetPlayerTuning().WallRunSideTraceDistance <= 0.0f)
	{
		return false;
	}

	const FVector TraceDirection = -ExpectedWallNormal.GetSafeNormal2D();
	if (TraceDirection.IsNearlyZero())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WallRunNormalTrace), false, this);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	const FVector TraceStart = GetActorLocation();
	const FVector TraceEnd = TraceStart + (TraceDirection * GetPlayerTuning().WallRunSideTraceDistance);
	TArray<FHitResult> WallHits;
	if (!World->LineTraceMultiByObjectType(WallHits, TraceStart, TraceEnd, ObjectQueryParams, QueryParams))
	{
		return false;
	}

	float BestDistance = TNumericLimits<float>::Max();
	for (const FHitResult& WallHit : WallHits)
	{
		FVector WallNormal = FVector::ZeroVector;
		if (!TryGetWallRunSurfaceNormal(WallHit, WallNormal))
		{
			continue;
		}

		if (WallHit.Distance < BestDistance)
		{
			OutWallHit = WallHit;
			OutWallNormal = WallNormal;
			BestDistance = WallHit.Distance;
		}
	}

	return BestDistance != TNumericLimits<float>::Max();
}

bool AGsPlayer::CanTriggerWallRun(const FVector& WallNormal) const
{
	if (!FirstPersonCameraComponent || CachedMoveInput.Y <= 0.1f)
	{
		return false;
	}

	FVector CameraForward = FirstPersonCameraComponent->GetForwardVector().GetSafeNormal2D();
	FVector HorizontalWallNormal = WallNormal.GetSafeNormal2D();
	if (CameraForward.IsNearlyZero() || HorizontalWallNormal.IsNearlyZero())
	{
		return false;
	}

	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	const float CameraWallDot = FMath::Abs(FVector::DotProduct(CameraForward, HorizontalWallNormal));
	if (CameraWallDot > PlayerTuning.WallRunMaxCameraWallNormalDot)
	{
		return false;
	}

	FVector HorizontalVelocity = GetVelocity();
	HorizontalVelocity.Z = 0.0f;
	if (!HorizontalVelocity.Normalize())
	{
		return false;
	}

	const float ForwardCameraDot = FVector::DotProduct(HorizontalVelocity, CameraForward);
	return ForwardCameraDot >= PlayerTuning.WallRunMinForwardCameraDot;
}

bool AGsPlayer::StartWallRun(const FVector& WallNormal)
{
	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	if (bIsDead || !PlayerMovementComponent || PlayerTuning.WallRunSpeed <= 0.0f || !PlayerMovementComponent->IsFalling())
	{
		return false;
	}

	if (!TryStartCharacterAction(EUEGameJamPlayerAction::WallRun, -1.0f))
	{
		return false;
	}

	const FVector HorizontalWallNormal = WallNormal.GetSafeNormal2D();
	if (HorizontalWallNormal.IsNearlyZero())
	{
		FinishCharacterAction();
		return false;
	}

	const bool bIsRightWall = FVector::DotProduct(GetActorRightVector(), -HorizontalWallNormal) >= 0.0f;
	FVector NewWallRunDirection = bIsRightWall
		? FVector::CrossProduct(FVector::UpVector, HorizontalWallNormal)
		: FVector::CrossProduct(HorizontalWallNormal, FVector::UpVector);
	NewWallRunDirection.Z = 0.0f;
	if (!NewWallRunDirection.Normalize())
	{
		FinishCharacterAction();
		return false;
	}

	PreWallRunGravityScale = PlayerMovementComponent->GravityScale;
	PreWallRunAirControl = PlayerMovementComponent->AirControl;
	PreWallRunMovementMode = PlayerMovementComponent->MovementMode;
	PreWallRunCustomMovementMode = PlayerMovementComponent->CustomMovementMode;

	WallRunDirection = NewWallRunDirection;
	WallRunSurfaceNormal = HorizontalWallNormal;
	bHasTriggeredWallRunThisJump = true;
	bCanCheckWallRun = false;
	SetWallRunCameraTiltTarget(bIsRightWall ? -PlayerTuning.WallRunCameraTiltAngle : PlayerTuning.WallRunCameraTiltAngle);

	PlayerMovementComponent->GravityScale = 0.0f;
	PlayerMovementComponent->AirControl = 0.0f;
	PlayerMovementComponent->SetMovementMode(MOVE_Falling);
	PlayerMovementComponent->Velocity = WallRunDirection * PlayerTuning.WallRunSpeed;

	return true;
}

void AGsPlayer::UpdateWallRun(float DeltaSeconds)
{
	(void)DeltaSeconds;

	if (!IsWallRunning())
	{
		return;
	}

	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	if (!PlayerMovementComponent)
	{
		StopWallRun();
		return;
	}

	if (PlayerMovementComponent->IsMovingOnGround() || CachedMoveInput.Y <= 0.1f)
	{
		StopWallRun();
		return;
	}

	FHitResult WallHit;
	FVector CurrentWallNormal = FVector::ZeroVector;
	if (!TryFindWallRunSurfaceAlongNormal(WallRunSurfaceNormal, WallHit, CurrentWallNormal))
	{
		StopWallRun();
		return;
	}

	const FVector HorizontalWallNormal = CurrentWallNormal.GetSafeNormal2D();
	if (HorizontalWallNormal.IsNearlyZero() || FVector::DotProduct(HorizontalWallNormal, WallRunSurfaceNormal) < 0.8f)
	{
		StopWallRun();
		return;
	}

	WallRunSurfaceNormal = HorizontalWallNormal;
	PlayerMovementComponent->GravityScale = 0.0f;
	PlayerMovementComponent->AirControl = 0.0f;
	PlayerMovementComponent->SetMovementMode(MOVE_Falling);
	PlayerMovementComponent->Velocity = WallRunDirection * GetPlayerTuning().WallRunSpeed;
	PlayerMovementComponent->Velocity.Z = 0.0f;
}

void AGsPlayer::StopWallRun()
{
	SetWallRunCameraTiltTarget(0.0f);

	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	if (PlayerMovementComponent)
	{
		PlayerMovementComponent->GravityScale = PreWallRunGravityScale;
		PlayerMovementComponent->AirControl = PreWallRunAirControl;
		if (!PlayerMovementComponent->IsMovingOnGround())
		{
			PlayerMovementComponent->SetMovementMode(PreWallRunMovementMode, PreWallRunCustomMovementMode);
		}

		FVector RestoredVelocity = PlayerMovementComponent->Velocity;
		RestoredVelocity.Z = 0.0f;
		PlayerMovementComponent->Velocity = RestoredVelocity;
	}

	WallRunDirection = FVector::ZeroVector;
	WallRunSurfaceNormal = FVector::ZeroVector;
	PreWallRunGravityScale = 1.0f;
	PreWallRunAirControl = 0.0f;
	PreWallRunMovementMode = MOVE_Falling;
	PreWallRunCustomMovementMode = 0;

	if (IsWallRunning())
	{
		FinishCharacterAction();
	}
}

bool AGsPlayer::TryWallRunJump()
{
	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	if (!IsWallRunning()
		|| bIsDead
		|| (PlayerTuning.WallRunJumpHorizontalStrength <= 0.0f && PlayerTuning.WallRunJumpVerticalStrength <= 0.0f))
	{
		return false;
	}

	FVector ForwardDirection = GetActorForwardVector().GetSafeNormal2D();
	const FVector WallJumpOutDirection = WallRunSurfaceNormal.GetSafeNormal2D();
	if (WallJumpOutDirection.IsNearlyZero())
	{
		return false;
	}

	FVector HorizontalJumpDirection = WallJumpOutDirection;
	if (!ForwardDirection.IsNearlyZero())
	{
		HorizontalJumpDirection = ForwardDirection + WallJumpOutDirection;
		if (!HorizontalJumpDirection.Normalize())
		{
			HorizontalJumpDirection = WallJumpOutDirection;
		}
	}

	const FVector LaunchVelocity =
		(HorizontalJumpDirection * PlayerTuning.WallRunJumpHorizontalStrength)
		+ (FVector::UpVector * PlayerTuning.WallRunJumpVerticalStrength);

	StopWallRun();
	LaunchCharacter(LaunchVelocity, true, true);
	ResetWallRunDetection();
	StartWallRunDetectionDelay();
	return true;
}

void AGsPlayer::UpdateWallRunCameraTilt(float DeltaSeconds)
{
	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	const float DesiredRoll = FMath::Clamp(TargetWallRunCameraRoll, -PlayerTuning.WallRunCameraTiltAngle, PlayerTuning.WallRunCameraTiltAngle);
	CurrentWallRunCameraRoll = PlayerTuning.WallRunCameraTiltInterpSpeed > 0.0f
		? FMath::FInterpTo(CurrentWallRunCameraRoll, DesiredRoll, DeltaSeconds, PlayerTuning.WallRunCameraTiltInterpSpeed)
		: DesiredRoll;
}

void AGsPlayer::SetWallRunCameraTiltTarget(float InTargetRoll)
{
	const float WallRunCameraTiltAngle = GetPlayerTuning().WallRunCameraTiltAngle;
	TargetWallRunCameraRoll = FMath::Clamp(InTargetRoll, -WallRunCameraTiltAngle, WallRunCameraTiltAngle);
}
