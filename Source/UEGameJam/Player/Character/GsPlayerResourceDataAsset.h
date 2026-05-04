// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GsPlayerResourceDataAsset.generated.h"

class AGsSkillBall;
class UAnimMontage;
class UInputAction;

/**
 * 玩家资源引用配置。
 */
UCLASS(BlueprintType)
class UEGAMEJAM_API UGsPlayerResourceDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 跳跃输入动作 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	/** 移动输入动作 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	/** 鼠标视角输入动作 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> MouseLookAction;

	/** 近战攻击输入动作，沿用 FireAction 名称以兼容输入资源 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> FireAction;

	/** 技能输入动作，用于释放技能球 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> SkillAction;

	/** 滑铲输入动作 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> SlideAction;

	/** 冲刺输入动作 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> DashAction;

	/** 钩爪输入动作 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> FalculaAction;

	/** 近战攻击时播放的动画蒙太奇 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee")
	TObjectPtr<UAnimMontage> MeleeAttackMontage;

	/** 技能释放时生成的技能球类，可在蓝图中替换具体表现 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	TSubclassOf<AGsSkillBall> SkillProjectileClass;

	/** 旧版技能发射 Socket 配置，当前极简发射逻辑不再使用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FName SkillSpawnSocketName = NAME_None;
};
