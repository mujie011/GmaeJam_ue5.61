// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Character/GsPlayer.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"

void AGsPlayer::DoMove(float Right, float Forward)
{
	if (bIsDead || !GetController())
	{
		return;
	}

	constexpr float SlideInputDeadZone = 0.1f;
	const FVector2D MoveVector(Right, Forward);
	CachedMoveInput = MoveVector.SizeSquared() > FMath::Square(SlideInputDeadZone) ? MoveVector : FVector2D::ZeroVector;

	if (IsSliding() || IsDashing() || IsWallRunning())
	{
		return;
	}

	AddMovementInput(GetActorRightVector(), Right);
	AddMovementInput(GetActorForwardVector(), Forward);
}

void AGsPlayer::DoJumpStart()
{
	if (bIsDead)
	{
		return;
	}

	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	const bool bWasMovingOnGround = PlayerMovementComponent && PlayerMovementComponent->IsMovingOnGround();

	if (IsDashing())
	{
		return;
	}

	if (IsWallRunning())
	{
		TryWallRunJump();
		return;
	}

	if (IsSliding())
	{
		if (!StopSlide(false))
		{
			return;
		}
	}

	Jump();

	if (bWasMovingOnGround)
	{
		ResetWallRunDetection();
		StartWallRunDetectionDelay();
	}
}

void AGsPlayer::DoJumpEnd()
{
	if (bIsDead)
	{
		return;
	}

	StopJumping();
}

void AGsPlayer::OnMoveInputCompleted(const FInputActionValue& Value)
{
	(void)Value;
	CachedMoveInput = FVector2D::ZeroVector;
}

bool AGsPlayer::StartSlide()
{
	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	UCapsuleComponent* PlayerCapsuleComponent = GetCapsuleComponent();

	if (bIsDead || !PlayerMovementComponent || !PlayerCapsuleComponent || !PlayerMovementComponent->IsMovingOnGround())
	{
		return false;
	}

	if (!TryGetSlideInputDirection(SlideDirection))
	{
		return false;
	}

	FVector ActorForward = GetActorForwardVector();
	ActorForward.Z = 0.0f;
	if (!ActorForward.Normalize() || FVector::DotProduct(SlideDirection, ActorForward) < 0.0f)
	{
		return false;
	}

	if (!TryStartCharacterAction(EUEGameJamPlayerAction::Slide, -1.0f))
	{
		return false;
	}

	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	OriginalSlideCapsuleHalfHeight = PlayerCapsuleComponent->GetUnscaledCapsuleHalfHeight();
	OriginalSlideMaxWalkSpeed = PlayerMovementComponent->MaxWalkSpeed;

	const float TargetCapsuleHalfHeight = FMath::Clamp(
		PlayerTuning.SlideCapsuleHalfHeight,
		PlayerCapsuleComponent->GetUnscaledCapsuleRadius(),
		OriginalSlideCapsuleHalfHeight);
	const float HalfHeightDelta = OriginalSlideCapsuleHalfHeight - TargetCapsuleHalfHeight;
	const float WorldHalfHeightDelta = HalfHeightDelta * PlayerCapsuleComponent->GetShapeScale();

	PlayerCapsuleComponent->SetCapsuleHalfHeight(TargetCapsuleHalfHeight, true);
	AddActorWorldOffset(FVector(0.0f, 0.0f, -WorldHalfHeightDelta), false);

	PlayerMovementComponent->MaxWalkSpeed = FMath::Max(PlayerTuning.SlideSpeed, PlayerTuning.SlideMaxSpeed);
	CurrentSlideSpeed = PlayerTuning.SlideSpeed;
	bIsWaitingToStopSlideWhenCanStand = false;

	FVector NewVelocity = SlideDirection * CurrentSlideSpeed;
	NewVelocity.Z = PlayerMovementComponent->Velocity.Z;
	PlayerMovementComponent->Velocity = NewVelocity;

	return true;
}

bool AGsPlayer::StartDash()
{
	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	if (bIsDead || !PlayerMovementComponent)
	{
		return false;
	}

	if (IsCharacterActionActive())
	{
		return false;
	}

	UWorld* World = GetWorld();
	const float CurrentWorldTime = World ? World->GetTimeSeconds() : 0.0f;
	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	if ((CurrentWorldTime - LastDashTime) < PlayerTuning.DashCooldown)
	{
		return false;
	}

	const bool bIsAirborne = PlayerMovementComponent->IsFalling();
	if (bIsAirborne && bHasDashedSinceLanded)
	{
		return false;
	}

	FVector ForwardDirection = FVector::VectorPlaneProject(GetActorForwardVector(), FVector::UpVector);
	if (!ForwardDirection.Normalize())
	{
		return false;
	}

	if (!TryStartCharacterAction(EUEGameJamPlayerAction::Dash, -1.0f))
	{
		return false;
	}

	DashDirection = ForwardDirection;
	LastDashTime = CurrentWorldTime;
	PreDashVelocity = PlayerMovementComponent->Velocity;
	PreDashMovementMode = PlayerMovementComponent->MovementMode;
	PreDashCustomMovementMode = PlayerMovementComponent->CustomMovementMode;
	DashStartLocation = GetActorLocation();
	DashTargetLocation = DashStartLocation + (DashDirection * (PlayerTuning.DashSpeed * PlayerTuning.DashDuration));
	DashTargetLocation.Z = DashStartLocation.Z;
	CurrentDashElapsedTime = 0.0f;

	if (bIsAirborne)
	{
		bHasDashedSinceLanded = true;
	}

	PlayerMovementComponent->StopMovementImmediately();
	PlayerMovementComponent->StopActiveMovement();
	PlayerMovementComponent->DisableMovement();

	if (PlayerTuning.DashDuration <= KINDA_SMALL_NUMBER)
	{
		FinishDash();
	}

	return true;
}

bool AGsPlayer::TryGetSlideInputDirection(FVector& OutSlideDirection) const
{
	constexpr float SlideInputDeadZone = 0.1f;

	OutSlideDirection = FVector::ZeroVector;

	const float RightInput = CachedMoveInput.X;
	const float ForwardInput = CachedMoveInput.Y;
	if (ForwardInput < -SlideInputDeadZone)
	{
		return false;
	}

	FVector LocalSlideDirection = FVector::ZeroVector;
	if (ForwardInput > SlideInputDeadZone)
	{
		LocalSlideDirection.X = 1.0f;

		if (RightInput < -SlideInputDeadZone)
		{
			LocalSlideDirection.Y = -1.0f;
		}
		else if (RightInput > SlideInputDeadZone)
		{
			LocalSlideDirection.Y = 1.0f;
		}
	}
	else if (RightInput < -SlideInputDeadZone)
	{
		LocalSlideDirection.Y = -1.0f;
	}
	else if (RightInput > SlideInputDeadZone)
	{
		LocalSlideDirection.Y = 1.0f;
	}
	else
	{
		return false;
	}

	OutSlideDirection = (GetActorForwardVector() * LocalSlideDirection.X) + (GetActorRightVector() * LocalSlideDirection.Y);
	OutSlideDirection.Z = 0.0f;
	return OutSlideDirection.Normalize();
}

bool AGsPlayer::StopSlide(bool bForceRestore)
{
	if (!IsSliding())
	{
		bIsWaitingToStopSlideWhenCanStand = false;
		return true;
	}

	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	UCapsuleComponent* PlayerCapsuleComponent = GetCapsuleComponent();

	if (!PlayerMovementComponent || !PlayerCapsuleComponent)
	{
		CurrentSlideSpeed = 0.0f;
		bIsSlideInputHeld = false;
		bIsWaitingToStopSlideWhenCanStand = false;
		FinishCharacterAction();
		return true;
	}

	if (!bForceRestore && !CanRestoreSlideCapsule())
	{
		bIsWaitingToStopSlideWhenCanStand = true;
		return false;
	}

	const float CurrentCapsuleHalfHeight = PlayerCapsuleComponent->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightDelta = OriginalSlideCapsuleHalfHeight - CurrentCapsuleHalfHeight;
	const float WorldHalfHeightDelta = HalfHeightDelta * PlayerCapsuleComponent->GetShapeScale();

	if (HalfHeightDelta > 0.0f)
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, WorldHalfHeightDelta), false);
		PlayerCapsuleComponent->SetCapsuleHalfHeight(OriginalSlideCapsuleHalfHeight, true);
	}

	PlayerMovementComponent->MaxWalkSpeed = OriginalSlideMaxWalkSpeed;
	CurrentSlideSpeed = 0.0f;
	bIsSlideInputHeld = false;
	bIsWaitingToStopSlideWhenCanStand = false;
	FinishCharacterAction();

	return true;
}

bool AGsPlayer::CanRestoreSlideCapsule() const
{
	UCapsuleComponent* PlayerCapsuleComponent = GetCapsuleComponent();
	UWorld* World = GetWorld();

	if (!PlayerCapsuleComponent || !World)
	{
		return true;
	}

	const float CurrentCapsuleHalfHeight = PlayerCapsuleComponent->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightDelta = OriginalSlideCapsuleHalfHeight - CurrentCapsuleHalfHeight;
	if (HalfHeightDelta <= 0.0f)
	{
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlideCapsuleRestore), false, this);
	FCollisionResponseParams ResponseParams;
	PlayerCapsuleComponent->InitSweepCollisionParams(QueryParams, ResponseParams);

	const float ShapeScale = PlayerCapsuleComponent->GetShapeScale();
	const float RestoredCapsuleRadius = PlayerCapsuleComponent->GetUnscaledCapsuleRadius() * ShapeScale;
	const float RestoredCapsuleHalfHeight = OriginalSlideCapsuleHalfHeight * ShapeScale;
	const FVector RestoreLocation = GetActorLocation() + FVector(0.0f, 0.0f, HalfHeightDelta * ShapeScale);
	const FCollisionShape RestoredCapsuleShape = FCollisionShape::MakeCapsule(RestoredCapsuleRadius, RestoredCapsuleHalfHeight);

	return !World->OverlapBlockingTestByChannel(
		RestoreLocation,
		GetActorQuat(),
		PlayerCapsuleComponent->GetCollisionObjectType(),
		RestoredCapsuleShape,
		QueryParams,
		ResponseParams);
}

void AGsPlayer::UpdateSlide(float DeltaSeconds)
{
	if (!IsSliding())
	{
		return;
	}

	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	if (!PlayerMovementComponent)
	{
		StopSlide(true);
		return;
	}

	if (!PlayerMovementComponent->IsMovingOnGround())
	{
		StopSlide(true);
		return;
	}

	if (bIsWaitingToStopSlideWhenCanStand)
	{
		if (!CanRestoreSlideCapsule())
		{
			FVector NewVelocity = SlideDirection * CurrentSlideSpeed;
			NewVelocity.Z = PlayerMovementComponent->Velocity.Z;
			PlayerMovementComponent->Velocity = NewVelocity;
			return;
		}

		if (!bIsSlideInputHeld)
		{
			StopSlide(false);
			return;
		}

		bIsWaitingToStopSlideWhenCanStand = false;
	}

	bool bShouldTryStopForLowSpeed = true;
	const FFindFloorResult& CurrentFloor = PlayerMovementComponent->CurrentFloor;
	if (CurrentFloor.IsWalkableFloor())
	{
		FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal.IsNearlyZero()
			? CurrentFloor.HitResult.Normal
			: CurrentFloor.HitResult.ImpactNormal;
		FloorNormal = FloorNormal.GetSafeNormal();

		FVector DownhillDirection = FVector::VectorPlaneProject(FVector::DownVector, FloorNormal);
		DownhillDirection = DownhillDirection.GetSafeNormal();
		const float DownhillAlignment = FVector::DotProduct(SlideDirection, DownhillDirection);

		if (DownhillAlignment > KINDA_SMALL_NUMBER && bIsSlideInputHeld)
		{
			if (PlayerTuning.SlideSlopeAcceleration > 0.0f)
			{
				CurrentSlideSpeed = FMath::Min(PlayerTuning.SlideMaxSpeed, CurrentSlideSpeed + (PlayerTuning.SlideSlopeAcceleration * DownhillAlignment * DeltaSeconds));
			}
			bShouldTryStopForLowSpeed = false;
		}
		else
		{
			CurrentSlideSpeed = FMath::Max(0.0f, CurrentSlideSpeed - (PlayerTuning.SlideDeceleration * DeltaSeconds));
		}
	}
	else
	{
		CurrentSlideSpeed = FMath::Max(0.0f, CurrentSlideSpeed - (PlayerTuning.SlideDeceleration * DeltaSeconds));
	}

	if (bShouldTryStopForLowSpeed && CurrentSlideSpeed < PlayerTuning.SlideStopSpeed)
	{
		if (StopSlide(false))
		{
			return;
		}
	}

	FVector NewVelocity = SlideDirection * CurrentSlideSpeed;
	NewVelocity.Z = PlayerMovementComponent->Velocity.Z;
	PlayerMovementComponent->Velocity = NewVelocity;
}

void AGsPlayer::UpdateDash(float DeltaSeconds)
{
	if (!IsDashing())
	{
		return;
	}

	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	if (!PlayerMovementComponent)
	{
		AbortDash();
		return;
	}

	CurrentDashElapsedTime += DeltaSeconds;
	const float DashDuration = GetPlayerTuning().DashDuration;
	const float DashAlpha = DashDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(CurrentDashElapsedTime / DashDuration, 0.0f, 1.0f)
		: 1.0f;
	const bool bReachedDestination = DashAlpha >= 1.0f;

	FVector DesiredLocation = FMath::Lerp(DashStartLocation, DashTargetLocation, DashAlpha);
	DesiredLocation.Z = DashStartLocation.Z;

	FHitResult SweepHit;
	SetActorLocation(DesiredLocation, true, &SweepHit, ETeleportType::None);

	if (SweepHit.bBlockingHit || bReachedDestination)
	{
		FinishDash();
	}
}

void AGsPlayer::ClearDashState()
{
	DashDirection = FVector::ForwardVector;
	PreDashVelocity = FVector::ZeroVector;
	PreDashMovementMode = MOVE_Walking;
	PreDashCustomMovementMode = 0;
	DashStartLocation = FVector::ZeroVector;
	DashTargetLocation = FVector::ZeroVector;
	CurrentDashElapsedTime = 0.0f;
}
