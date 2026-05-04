// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Character/GsPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "TimerManager.h"
#include "UEGameJam.h"
#include "Player/Character/GsPlayerResourceDataAsset.h"
#include "RealmRevealerComponent.h"

AGsPlayer::AGsPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = false;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;
	DefaultFirstPersonCameraRelativeTransform = FirstPersonCameraComponent->GetRelativeTransform();

	MeleeDamageCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("MeleeDamageCollision"));
	MeleeDamageCollision->SetupAttachment(GetRootComponent());
	MeleeDamageCollision->SetRelativeLocation(FVector(140.0f, 0.0f, 0.0f));
	MeleeDamageCollision->InitBoxExtent(FVector(70.0f, 50.0f, 50.0f));
	MeleeDamageCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeleeDamageCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeleeDamageCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	MeleeDamageCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	MeleeDamageCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	MeleeDamageCollision->SetGenerateOverlapEvents(true);

	if (USkeletalMeshComponent* WorldMesh = GetMesh())
	{
		WorldMesh->SetVisibility(false);
		WorldMesh->SetHiddenInGame(true);
		WorldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WorldMesh->SetGenerateOverlapEvents(false);
		WorldMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	}

	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	PlayerMovementComponent->BrakingDecelerationFalling = 1500.0f;
	PlayerMovementComponent->AirControl = 0.5f;
	PlayerMovementComponent->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
	
	Tags.Add(FName("Player"));
}

void AGsPlayer::BeginPlay()
{
	Super::BeginPlay();

	ApplyPlayerTuningFromDataTable();
	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();

	CurrentHP = PlayerTuning.MaxHP;
	bIsDead = false;
	bHasDashedSinceLanded = false;
	PreDashVelocity = FVector::ZeroVector;
	PreDashMovementMode = MOVE_Walking;
	PreDashCustomMovementMode = 0;
	DashStartLocation = FVector::ZeroVector;
	DashTargetLocation = FVector::ZeroVector;
	CurrentDashElapsedTime = 0.0f;
	LastSafeLocation = GetActorLocation();
	LastSafeRotation = GetActorRotation();
	bHasSafeLocation = true;
	LastFallRecoveryTime = -PlayerTuning.SafeLandingMinInterval;
	LastDashTime = -PlayerTuning.DashCooldown;
	ResetWallRunDetection();

	if (UWorld* World = GetWorld())
	{
		LastDashTime = World->GetTimeSeconds() - PlayerTuning.DashCooldown;
	}

	if (FirstPersonCameraComponent)
	{
		DefaultFirstPersonCameraRelativeTransform = FirstPersonCameraComponent->GetRelativeTransform();
		CurrentHeadCameraRotationOffset = FRotator::ZeroRotator;
		TargetWallRunCameraRoll = 0.0f;
		CurrentWallRunCameraRoll = 0.0f;
		FirstPersonCameraComponent->SetFieldOfView(PlayerTuning.DefaultCameraFOV);
	}

	if (USkeletalMeshComponent* WorldMesh = GetMesh())
	{
		WorldMesh->SetVisibility(false);
		WorldMesh->SetHiddenInGame(true);
		WorldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WorldMesh->SetGenerateOverlapEvents(false);
	}

	OnDamaged.Broadcast(GetLifePercent());
}

void AGsPlayer::ApplyPlayerTuningFromDataTable()
{
	CurrentPlayerTuning = &DefaultPlayerTuning;

	if (!PlayerTuningTable)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				3.0f,
				FColor::Red,
				FString::Printf(TEXT("'%s' 未配置玩家手感数值表 PlayerTuningTable，使用 C++ 默认手感数值。"), *GetNameSafe(this)));
		}
		return;
	}

	if (PlayerTuningRowName.IsNone())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				3.0f,
				FColor::Red,
				FString::Printf(TEXT("'%s' 玩家手感数值表行名为空，使用 C++ 默认手感数值。"), *GetNameSafe(this)));
		}
		return;
	}

	const FString ContextString = FString::Printf(TEXT("%s PlayerTuning"), *GetNameSafe(this));
	const FGsPlayerTuningRow* TuningRow = PlayerTuningTable->FindRow<FGsPlayerTuningRow>(PlayerTuningRowName, ContextString, false);
	if (!TuningRow)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				3.0f,
				FColor::Red,
				FString::Printf(
					TEXT("'%s' 玩家手感数值表 '%s' 找不到行 '%s'，使用 C++ 默认手感数值。"),
					*GetNameSafe(this),
					*GetPathNameSafe(PlayerTuningTable),
					*PlayerTuningRowName.ToString()));
		}
		return;
	}

	CurrentPlayerTuning = TuningRow;
}

void AGsPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsDead)
	{
		if (FirstPersonCameraComponent)
		{
			UpdateWallRunCameraTilt(DeltaSeconds);
			UpdateFirstPersonCameraRotation(DeltaSeconds);
		}
		return;
	}

	UpdateSlide(DeltaSeconds);
	UpdateDash(DeltaSeconds);
	UpdateWallRun(DeltaSeconds);
	UpdateWallRunDetection();

	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement();
	if (PlayerMovementComponent
		&& PlayerMovementComponent->IsFalling()
		&& bHasSafeLocation
		&& !bIsRecoveringFromFall
		&& PlayerTuning.FallResetDepth > 0.0f)
	{
		UWorld* World = GetWorld();
		const float CurrentWorldTime = World ? World->GetTimeSeconds() : 0.0f;
		if ((CurrentWorldTime - LastFallRecoveryTime) >= PlayerTuning.SafeLandingMinInterval
			&& GetActorLocation().Z <= (LastSafeLocation.Z - PlayerTuning.FallResetDepth))
		{
			RecoverFromDeepFall();
		}
	}

	if (!FirstPersonCameraComponent)
	{
		return;
	}

	float TargetFOV = PlayerTuning.DefaultCameraFOV;
	if (GetVelocity().Size2D() >= PlayerTuning.RunFOVSpeedThreshold)
	{
		TargetFOV = PlayerTuning.RunningCameraFOV;
	}
	if (IsSliding())
	{
		TargetFOV = PlayerTuning.SlideCameraFOV;
	}
	if (IsDashing())
	{
		TargetFOV = PlayerTuning.DashCameraFOV;
	}
	const float NewFOV = FMath::FInterpTo(FirstPersonCameraComponent->FieldOfView, TargetFOV, DeltaSeconds, PlayerTuning.CameraFOVInterpSpeed);
	FirstPersonCameraComponent->SetFieldOfView(NewFOV);
	UpdateWallRunCameraTilt(DeltaSeconds);
	UpdateFirstPersonCameraRotation(DeltaSeconds);
}

void AGsPlayer::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	StopSlide(true);
	if (IsDashing())
	{
		AbortDash();
	}
	if (IsWallRunning())
	{
		StopWallRun();
	}
	else
	{
		ClearDashState();
	}
	ResetWallRunDetection();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimer);
		World->GetTimerManager().ClearTimer(MeleeHitTimer);
		World->GetTimerManager().ClearTimer(DeferredDestroyTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void AGsPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(PlayerResourceData->JumpAction, ETriggerEvent::Started, this, &AGsPlayer::DoJumpStart);
		EnhancedInputComponent->BindAction(PlayerResourceData->JumpAction, ETriggerEvent::Completed, this, &AGsPlayer::DoJumpEnd);
		EnhancedInputComponent->BindAction(PlayerResourceData->MoveAction, ETriggerEvent::Triggered, this, &AGsPlayer::MoveInput);
		EnhancedInputComponent->BindAction(PlayerResourceData->MoveAction, ETriggerEvent::Completed, this, &AGsPlayer::OnMoveInputCompleted);
		EnhancedInputComponent->BindAction(PlayerResourceData->MouseLookAction, ETriggerEvent::Triggered, this, &AGsPlayer::LookInput);
		EnhancedInputComponent->BindAction(PlayerResourceData->FireAction, ETriggerEvent::Started, this, &AGsPlayer::DoStartFiring);
		EnhancedInputComponent->BindAction(PlayerResourceData->SkillAction, ETriggerEvent::Started, this, &AGsPlayer::DoSkill);
		EnhancedInputComponent->BindAction(PlayerResourceData->SlideAction, ETriggerEvent::Started, this, &AGsPlayer::DoSlide);
		EnhancedInputComponent->BindAction(PlayerResourceData->SlideAction, ETriggerEvent::Completed, this, &AGsPlayer::DoSlideEnd);
		EnhancedInputComponent->BindAction(PlayerResourceData->DashAction, ETriggerEvent::Started, this, &AGsPlayer::DoDash);
		EnhancedInputComponent->BindAction(PlayerResourceData->FalculaAction, ETriggerEvent::Started, this, &AGsPlayer::DoFalcula);
	}
	else
	{
		UE_LOG(LogUEGameJam, Error, TEXT("'%s' 找不到 Enhanced Input Component。"), *GetNameSafe(this));
	}
}

void AGsPlayer::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (IsWallRunning())
	{
		StopWallRun();
	}

	bHasDashedSinceLanded = false;
	ResetWallRunDetection();
	UpdateSafeLandingTransform();
}

void AGsPlayer::MoveInput(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AGsPlayer::LookInput(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

float AGsPlayer::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	(void)DamageEvent;
	(void)EventInstigator;
	(void)DamageCauser;

	if (bIsDead || Damage <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage = CurrentHP;
	CurrentHP = 0.0f;
	Die();

	return AppliedDamage;
}

bool AGsPlayer::IsInsideActiveRealmReveal() const
{
	if (!URealmRevealerComponent::IsAnyActive())
	{
		return false;
	}

	const FVector Center = URealmRevealerComponent::GetActiveCenter();
	const float Radius = URealmRevealerComponent::GetActiveRadius();
	return FVector::DistSquared(GetActorLocation(), Center) <= Radius * Radius;
}

void AGsPlayer::DoAim(float Yaw, float Pitch)
{
	if (bIsDead || !GetController())
	{
		return;
	}

	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AGsPlayer::UpdateFirstPersonCameraRotation(float DeltaSeconds)
{
	if (!FirstPersonCameraComponent || !FirstPersonMesh)
	{
		return;
	}

	const FName CameraAttachSocketName = FirstPersonCameraComponent->GetAttachSocketName();
	const FTransform HeadSocketWorldTransform = CameraAttachSocketName != NAME_None
		? FirstPersonMesh->GetSocketTransform(CameraAttachSocketName, RTS_World)
		: FirstPersonMesh->GetComponentTransform();
	const FRotator RawCameraWorldRotation =
		(DefaultFirstPersonCameraRelativeTransform * HeadSocketWorldTransform).GetRotation().Rotator();

	FRotator DesiredCameraWorldRotation = RawCameraWorldRotation;
	if (AController* PlayerController = GetController())
	{
		const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
		const FRotator ControlRotation = PlayerController->GetControlRotation().GetNormalized();
		const FRotator RawHeadRotationOffset = (RawCameraWorldRotation - ControlRotation).GetNormalized();
		const float BlendAlpha = FMath::Clamp(PlayerTuning.HeadCameraRotationBlendAlpha, 0.0f, 1.0f);
		const FRotator TargetHeadRotationOffset(
			RawHeadRotationOffset.Pitch * BlendAlpha,
			RawHeadRotationOffset.Yaw * BlendAlpha,
			RawHeadRotationOffset.Roll * BlendAlpha);

		CurrentHeadCameraRotationOffset = PlayerTuning.HeadCameraRotationInterpSpeed > 0.0f
			? FMath::RInterpTo(CurrentHeadCameraRotationOffset, TargetHeadRotationOffset, DeltaSeconds, PlayerTuning.HeadCameraRotationInterpSpeed).GetNormalized()
			: TargetHeadRotationOffset.GetNormalized();
		DesiredCameraWorldRotation = (ControlRotation + CurrentHeadCameraRotationOffset).GetNormalized();
	}
	else
	{
		CurrentHeadCameraRotationOffset = FRotator::ZeroRotator;
	}

	DesiredCameraWorldRotation.Roll = FRotator::NormalizeAxis(DesiredCameraWorldRotation.Roll + CurrentWallRunCameraRoll);
	FirstPersonCameraComponent->SetWorldRotation(DesiredCameraWorldRotation.GetNormalized());
}

void AGsPlayer::DoStartFiring()
{
	StartMeleeAttack();
}

void AGsPlayer::DoSlide()
{
	if (bIsDead)
	{
		return;
	}

	bIsSlideInputHeld = true;

	if (IsWallRunning())
	{
		return;
	}

	if (!IsSliding())
	{
		StartSlide();
	}
}

void AGsPlayer::DoSlideEnd()
{
	bIsSlideInputHeld = false;

	if (bIsDead || !IsSliding())
	{
		return;
	}

	StopSlide(false);
}

void AGsPlayer::DoDash()
{
	if (bIsDead || IsWallRunning())
	{
		return;
	}

	StartDash();
}

bool AGsPlayer::IsCharacterActionActive() const
{
	return CurrentAction != EUEGameJamPlayerAction::None;
}

bool AGsPlayer::IsSliding() const
{
	return CurrentAction == EUEGameJamPlayerAction::Slide;
}

bool AGsPlayer::IsDashing() const
{
	return CurrentAction == EUEGameJamPlayerAction::Dash;
}

bool AGsPlayer::IsWallRunning() const
{
	return CurrentAction == EUEGameJamPlayerAction::WallRun;
}

float AGsPlayer::GetLifePercent() const
{
	const float MaxHP = GetPlayerTuning().MaxHP;
	return MaxHP > 0.0f ? FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f) : 0.0f;
}

bool AGsPlayer::IsDead() const
{
	return bIsDead;
}

bool AGsPlayer::TryStartCharacterAction(EUEGameJamPlayerAction Action, float Duration)
{
	if (IsCharacterActionActive())
	{
		return false;
	}

	CurrentAction = Action;

	if (!GetWorld())
	{
		return true;
	}

	if (Duration > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(ActionTimer, this, &AGsPlayer::FinishCharacterAction, Duration, false);
	}
	else if (FMath::IsNearlyZero(Duration))
	{
		FinishCharacterAction();
	}

	return true;
}

void AGsPlayer::FinishCharacterAction()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimer);
	}

	CurrentAction = EUEGameJamPlayerAction::None;
}

void AGsPlayer::FinishDash()
{
	if (!IsDashing())
	{
		ClearDashState();
		return;
	}

	if (UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement())
	{
		PlayerMovementComponent->SetMovementMode(PreDashMovementMode, PreDashCustomMovementMode);

		FVector PreDashHorizontalVelocity = PreDashVelocity;
		PreDashHorizontalVelocity.Z = 0.0f;

		const float ForwardSpeed = FMath::Max(0.0f, FVector::DotProduct(PreDashHorizontalVelocity, DashDirection));
		FVector RestoredVelocity = DashDirection * ForwardSpeed;
		RestoredVelocity.Z = PreDashVelocity.Z;

		PlayerMovementComponent->Velocity = RestoredVelocity;
	}

	ClearDashState();
	FinishCharacterAction();
}

void AGsPlayer::AbortDash()
{
	if (!IsDashing())
	{
		ClearDashState();
		return;
	}

	if (UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement())
	{
		PlayerMovementComponent->SetMovementMode(PreDashMovementMode, PreDashCustomMovementMode);
		PlayerMovementComponent->StopMovementImmediately();
		PlayerMovementComponent->StopActiveMovement();
	}

	ClearDashState();
	FinishCharacterAction();
}

void AGsPlayer::UpdateSafeLandingTransform()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if ((World->GetTimeSeconds() - LastFallRecoveryTime) < GetPlayerTuning().SafeLandingMinInterval)
	{
		return;
	}

	LastSafeLocation = GetActorLocation();
	LastSafeRotation = GetActorRotation();
	bHasSafeLocation = true;
}

void AGsPlayer::RecoverFromDeepFall()
{
	if (bIsDead || bIsRecoveringFromFall || !bHasSafeLocation)
	{
		return;
	}

	bIsRecoveringFromFall = true;

	if (UWorld* World = GetWorld())
	{
		LastFallRecoveryTime = World->GetTimeSeconds();
	}

	StopSlide(true);
	if (IsDashing())
	{
		AbortDash();
	}
	if (IsWallRunning())
	{
		StopWallRun();
	}
	else
	{
		ClearDashState();
		FinishCharacterAction();
	}
	bHasDashedSinceLanded = false;
	ResetWallRunDetection();

	if (UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement())
	{
		PlayerMovementComponent->StopMovementImmediately();
		PlayerMovementComponent->StopActiveMovement();
		PlayerMovementComponent->Velocity = FVector::ZeroVector;
		PlayerMovementComponent->SetMovementMode(MOVE_Walking);
	}

	SetActorLocationAndRotation(LastSafeLocation, LastSafeRotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement())
	{
		PlayerMovementComponent->StopMovementImmediately();
		PlayerMovementComponent->Velocity = FVector::ZeroVector;
	}

	bIsRecoveringFromFall = false;
}

void AGsPlayer::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	StopSlide(true);
	if (IsDashing())
	{
		AbortDash();
	}
	if (IsWallRunning())
	{
		StopWallRun();
	}
	else
	{
		ClearDashState();
		FinishCharacterAction();
	}
	bHasDashedSinceLanded = false;
	ResetWallRunDetection();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MeleeHitTimer);
	}

	if (UCharacterMovementComponent* PlayerMovementComponent = GetCharacterMovement())
	{
		PlayerMovementComponent->StopMovementImmediately();
		PlayerMovementComponent->StopActiveMovement();
		PlayerMovementComponent->DisableMovement();
	}

	DisableInput(Cast<APlayerController>(GetController()));
	OnDamaged.Broadcast(0.0f);
	OnDeath.Broadcast();
	BP_OnDeath();

	const float DeferredDestructionTime = GetPlayerTuning().DeferredDestructionTime;
	if (DeferredDestructionTime <= 0.0f)
	{
		Destroy();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DeferredDestroyTimer, this, &AGsPlayer::OnDeferredDestroy, DeferredDestructionTime, false);
	}
}

void AGsPlayer::OnDeferredDestroy()
{
	Destroy();
}
