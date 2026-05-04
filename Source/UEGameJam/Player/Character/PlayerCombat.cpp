// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Character/GsPlayer.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Character/GsPlayerResourceDataAsset.h"
#include "Player/Skill/GsSkillBall.h"
#include "TimerManager.h"

void AGsPlayer::DoSkill()
{
	BP_OnSkillInput();
	StartSkillCast();
}

bool AGsPlayer::StartMeleeAttack()
{
	if (bIsDead)
	{
		return false;
	}

	const FGsPlayerTuningRow& PlayerTuning = GetPlayerTuning();
	float ActionDuration = PlayerTuning.MeleeFallbackDuration;

	if (FirstPersonMesh)
	{
		if (UAnimInstance* AnimInstance = FirstPersonMesh->GetAnimInstance())
		{
			const float MontageDuration = AnimInstance->Montage_Play(PlayerResourceData->MeleeAttackMontage);
			if (MontageDuration > 0.0f)
			{
				ActionDuration = MontageDuration;
			}
		}
	}

	if (!TryStartCharacterAction(EUEGameJamPlayerAction::MeleeAttack, ActionDuration))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	World->GetTimerManager().ClearTimer(MeleeHitTimer);

	if (PlayerTuning.MeleeHitDelay <= 0.0f)
	{
		PerformMeleeHit();
	}
	else
	{
		World->GetTimerManager().SetTimer(MeleeHitTimer, this, &AGsPlayer::PerformMeleeHit, PlayerTuning.MeleeHitDelay, false);
	}

	return true;
}

FVector AGsPlayer::GetSkillAimTarget(const FVector& ViewLocation, const FVector& ViewDirection) const
{
	const FVector TraceEnd = ViewLocation + (ViewDirection * GetPlayerTuning().SkillAimTraceDistance);

	UWorld* World = GetWorld();
	if (!World)
	{
		return TraceEnd;
	}

	FHitResult OutHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerSkillAim), false, this);
	QueryParams.AddIgnoredActor(this);

	World->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

bool AGsPlayer::StartSkillCast()
{
	if (bIsDead)
	{
		return false;
	}

	// 上一次技能（飞行小球或落地大球）尚未结束 → 禁止再丢
	if (AGsSkillBall::IsAnySkillActive())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const bool bShouldBypassSkillAction = IsSliding() || IsWallRunning();
	const bool bStartedSkillAction = !bShouldBypassSkillAction;
	if (bStartedSkillAction && !TryStartCharacterAction(EUEGameJamPlayerAction::Skill, GetPlayerTuning().SkillActionDuration))
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	GetController()->GetPlayerViewPoint(ViewLocation, ViewRotation);
	
	const FVector ViewDirection = ViewRotation.Vector();
	const FVector AimTarget = GetSkillAimTarget(ViewLocation, ViewDirection);
	const FVector SpawnLocation = GetActorLocation();
	const FRotator SpawnRotation = UKismetMathLibrary::FindLookAtRotation(SpawnLocation, AimTarget);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AGsSkillBall* SpawnedSkillBall = World->SpawnActor<AGsSkillBall>(PlayerResourceData->SkillProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (!SpawnedSkillBall)
	{
		if (bStartedSkillAction)
		{
			FinishCharacterAction();
		}
		return false;
	}

	SpawnedSkillBall->InitializeSkillBall(AimTarget);

	return true;
}

void AGsPlayer::PerformMeleeHit()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(MeleeHitTimer);

	const float MeleeDamage = GetPlayerTuning().MeleeDamage;
	if (MeleeDamage <= 0.0f)
	{
		return;
	}

	UBoxComponent* AttackDamageCollision = GetMeleeDamageCollision();
	if (!AttackDamageCollision)
	{
		return;
	}

	AttackDamageCollision->UpdateOverlaps();

	TArray<AActor*> OverlappingActors;
	AttackDamageCollision->GetOverlappingActors(OverlappingActors);
	if (OverlappingActors.IsEmpty())
	{
		return;
	}

	TSet<AActor*> DamagedActors;
	for (AActor* HitActor : OverlappingActors)
	{
		if (!IsValid(HitActor) || HitActor == this)
		{
			continue;
		}

		if (DamagedActors.Contains(HitActor))
		{
			continue;
		}

		DamagedActors.Add(HitActor);
		UGameplayStatics::ApplyDamage(HitActor, MeleeDamage, GetController(), this, UDamageType::StaticClass());
	}
}
