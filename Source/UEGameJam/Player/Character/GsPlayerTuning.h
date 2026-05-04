// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GsPlayerTuning.generated.h"

/**
 * 玩家手感数值配置行。
 */
USTRUCT(BlueprintType)
struct UEGAMEJAM_API FGsPlayerTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 冲刺速度，用于计算冲刺可到达的总位移距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta = (ClampMin = 0, Units = "cm/s"))
	float DashSpeed = 2000.0f;

	/** 冲刺持续时间，数值越大前冲位移段持续越久 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta = (ClampMin = 0, Units = "s"))
	float DashDuration = 0.3f;

	/** 两次冲刺之间的冷却时间，数值越大连续冲刺间隔越久 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta = (ClampMin = 0, Units = "s"))
	float DashCooldown = 0.75f;

	/** 滑铲时使用的水平移动速度，数值越大向前滑得越快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta = (ClampMin = 0, Units = "cm/s"))
	float SlideSpeed = 1200.0f;

	/** 滑铲时胶囊体的半高，用于让角色保持低姿态 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta = (ClampMin = 0, Units = "cm"))
	float SlideCapsuleHalfHeight = 48.0f;

	/** 滑铲速度低于这个值时会尝试结束滑铲 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta = (ClampMin = 0, Units = "cm/s"))
	float SlideStopSpeed = 400.0f;

	/** 滑铲时每秒降低的速度，数值越大滑铲减速越快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta = (ClampMin = 0))
	float SlideDeceleration = 500.0f;

	/** 下坡滑铲时每秒增加的速度，数值越大下坡加速越快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta = (ClampMin = 0))
	float SlideSlopeAcceleration = 900.0f;

	/** 滑铲可达到的最大水平速度，数值越大下坡时最高速度越高 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta = (ClampMin = 0, Units = "cm/s"))
	float SlideMaxSpeed = 1800.0f;

	/** 近战命中造成的伤害值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Melee", meta = (ClampMin = 0))
	float MeleeDamage = 100.0f;

	/** 没有成功播放攻击蒙太奇时，近战动作锁定的备用时长 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Melee", meta = (ClampMin = 0, Units = "s"))
	float MeleeFallbackDuration = 0.35f;

	/** 近战命中判定延迟，用于把 Box Sweep 对齐到挥砍时机 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Melee", meta = (ClampMin = 0, Units = "s"))
	float MeleeHitDelay = 0.08f;

	/** 旧版技能发射前推距离，当前极简发射逻辑不再使用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta = (ClampMin = 0, Units = "cm"))
	float SkillSpawnForwardOffset = 100.0f;

	/** 技能瞄准检测的最远距离，数值越大越容易命中远处准星中心位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta = (ClampMin = 0, Units = "cm"))
	float SkillAimTraceDistance = 10000.0f;

	/** 技能释放占用动作状态的时长，数值越大越久不能触发其他互斥动作 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill", meta = (ClampMin = 0, Units = "s"))
	float SkillActionDuration = 0.15f;

	/** 玩家默认视野角，静止或低速移动时相机会平滑回到这个 FOV */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta = (ClampMin = 1, ClampMax = 170, Units = "deg"))
	float DefaultCameraFOV = 100.0f;

	/** 玩家跑起来时过渡到的视野角，用于增强速度感 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta = (ClampMin = 1, ClampMax = 170, Units = "deg"))
	float RunningCameraFOV = 120.0f;

	/** 冲刺期间目标视野角，用于增强爆发速度感 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta = (ClampMin = 1, ClampMax = 170, Units = "deg"))
	float DashCameraFOV = 130.0f;

	/** 滑铲期间目标视野角，用于增强贴地高速移动的速度感 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta = (ClampMin = 1, ClampMax = 170, Units = "deg"))
	float SlideCameraFOV = 130.0f;

	/** 水平移动速度达到这个值时视为跑起来，单位为厘米每秒 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta = (ClampMin = 0, Units = "cm/s"))
	float RunFOVSpeedThreshold = 450.0f;

	/** 相机 FOV 向目标值过渡的速度，数值越大变化越快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta = (ClampMin = 0))
	float CameraFOVInterpSpeed = 8.0f;

	/** 头部原始旋转偏移的保留比例，数值越大保留的方向轻晃越明显 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta = (ClampMin = 0, ClampMax = 1))
	float HeadCameraRotationBlendAlpha = 0.25f;

	/** 头部原始旋转偏移平滑过渡的速度，数值越大轻晃跟随越快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta = (ClampMin = 0))
	float HeadCameraRotationInterpSpeed = 12.0f;

	/** 起跳后延迟多久才开始检测墙跑触发，单位为秒 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = 0, Units = "s"))
	float WallRunCheckDelay = 0.2f;

	/** 左右两侧墙跑检测的射线距离，数值越大越容易探测到侧边墙面 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = 0, Units = "cm"))
	float WallRunSideTraceDistance = 80.0f;

	/** 相机朝向与墙面法线允许的最大点积绝对值，越小越要求沿墙观察 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = 0, ClampMax = 1))
	float WallRunMaxCameraWallNormalDot = 0.6f;

	/** 角色前进方向与相机朝向至少需要多接近才允许触发墙跑 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = -1, ClampMax = 1))
	float WallRunMinForwardCameraDot = 0.8f;

	/** 墙跑时沿墙横向移动的固定速度，数值越大沿墙跑得越快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = 0, Units = "cm/s"))
	float WallRunSpeed = 900.0f;

	/** 墙跑跳出时水平发射力度，数值越大斜向离墙跳得越远 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = 0, Units = "cm/s"))
	float WallRunJumpHorizontalStrength = 850.0f;

	/** 墙跑跳出时向上的发射力度，数值越大离墙后跳得越高 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = 0, Units = "cm/s"))
	float WallRunJumpVerticalStrength = 650.0f;

	/** 墙跑时第一人称视角倾斜的角度，右墙为负左墙为正 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = 0, ClampMax = 89, Units = "deg"))
	float WallRunCameraTiltAngle = 15.0f;

	/** 墙跑视角倾斜切换的速度，数值越大进入和回正越快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Run", meta = (ClampMin = 0))
	float WallRunCameraTiltInterpSpeed = 8.0f;

	/** 相对最近一次安全落地点，向下掉落超过这个高度后会回传，单位为厘米 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fall Recovery", meta = (ClampMin = 0, Units = "cm"))
	float FallResetDepth = 2000.0f;

	/** 深坑回传后至少间隔这么久才允许再次刷新安全点或再次触发回传 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fall Recovery", meta = (ClampMin = 0, Units = "s"))
	float SafeLandingMinInterval = 0.2f;

	/** 角色最大生命值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta = (ClampMin = 0))
	float MaxHP = 500.0f;

	/** 死亡后延时销毁的时间，留 0 表示立即销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta = (ClampMin = 0, Units = "s"))
	float DeferredDestructionTime = 5.0f;
};
