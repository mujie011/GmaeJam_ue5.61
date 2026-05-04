// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/Character/GsPlayerTuning.h"
#include "GsPlayer.generated.h"

class UBoxComponent;
class UCameraComponent;
class UInputComponent;
class USkeletalMeshComponent;
class UGsPlayerResourceDataAsset;
class AGsGrapplePoint;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUEGameJamPlayerDamagedDelegate, float, LifePercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUEGameJamPlayerDeathDelegate);

UENUM(BlueprintType)
enum class EUEGameJamPlayerAction : uint8
{
	None,
	MeleeAttack,
	Skill,
	Dash,
	Slide,
	WallRun
};

/**
 *  纯玩家侧近战角色
 */
UCLASS()
class UEGAMEJAM_API AGsPlayer : public ACharacter
{
	GENERATED_BODY()

	/** 第一人称手臂网格，仅自己可见 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** 第一人称相机 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCameraComponent;

	/** 近战造成伤害时使用的盒形检测范围，可在蓝图中调整位置和大小 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> MeleeDamageCollision;

protected:

	/** 玩家资源引用配置，用于集中填写输入、近战和技能资源 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resources", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGsPlayerResourceDataAsset> PlayerResourceData;

	/** 玩家手感数值表，策划和程序在表中调整纯数值参数，避免直接修改角色蓝图 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tuning", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataTable> PlayerTuningTable;

	/** 玩家手感数值表行名，默认读取 Default 行 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tuning", meta = (AllowPrivateAccess = "true"))
	FName PlayerTuningRowName = FName("Default");

	/** 未配置 DataTable 时使用的 C++ 默认玩家手感数值 */
	FGsPlayerTuningRow DefaultPlayerTuning;

	/** 当前运行时使用的玩家手感数值行，通常指向 DataTable 中的行 */
	const FGsPlayerTuningRow* CurrentPlayerTuning = &DefaultPlayerTuning;

	/** 当前生命值 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 0.0f;

	/** 当前角色动作，用于阻止互斥动作同时触发 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Action", meta = (AllowPrivateAccess = "true"))
	EUEGameJamPlayerAction CurrentAction = EUEGameJamPlayerAction::None;

	/** 是否已经死亡 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

	/** 当前动作结束计时器 */
	FTimerHandle ActionTimer;

	/** 近战命中计时器 */
	FTimerHandle MeleeHitTimer;

	/** 死亡后销毁计时器 */
	FTimerHandle DeferredDestroyTimer;

	/** 起跳后延迟开启墙跑检测的计时器 */
	FTimerHandle WallRunDetectionDelayTimer;

	/** 滑铲前的胶囊体半高 */
	float OriginalSlideCapsuleHalfHeight = 0.0f;

	/** 滑铲前的最大地面速度 */
	float OriginalSlideMaxWalkSpeed = 0.0f;

	/** 最近一次本地空间移动输入，用于确定滑铲方向 */
	FVector2D CachedMoveInput = FVector2D::ZeroVector;

	/** 进入滑铲时锁定的方向 */
	FVector SlideDirection = FVector::ForwardVector;

	/** 当前滑铲沿锁定方向的速度 */
	float CurrentSlideSpeed = 0.0f;

	/** 当前是否按住滑铲输入 */
	bool bIsSlideInputHeld = false;

	/** 是否已经请求停止滑铲，但因为头顶空间不足正在等待可以站立 */
	bool bIsWaitingToStopSlideWhenCanStand = false;

	/** 进入冲刺时锁定的方向 */
	FVector DashDirection = FVector::ForwardVector;

	/** 最近一次成功冲刺发生的时间 */
	float LastDashTime = 0.0f;

	/** 进入冲刺前缓存的完整速度，用于冲刺结束时提取前向惯性和竖直速度 */
	FVector PreDashVelocity = FVector::ZeroVector;

	/** 进入冲刺前缓存的移动模式，用于冲刺结束后恢复移动组件 */
	EMovementMode PreDashMovementMode = MOVE_Walking;

	/** 进入冲刺前缓存的自定义移动模式 */
	uint8 PreDashCustomMovementMode = 0;

	/** 冲刺开始时的位置 */
	FVector DashStartLocation = FVector::ZeroVector;

	/** 冲刺目标位置 */
	FVector DashTargetLocation = FVector::ZeroVector;

	/** 当前冲刺已推进的时间 */
	float CurrentDashElapsedTime = 0.0f;

	/** 自上次落地以来是否已经完成过一次空中冲刺 */
	bool bHasDashedSinceLanded = false;

	/** 最近一次安全落地点位置 */
	FVector LastSafeLocation = FVector::ZeroVector;

	/** 最近一次安全落地点朝向 */
	FRotator LastSafeRotation = FRotator::ZeroRotator;

	/** 是否已经记录了可回传的安全落地点 */
	bool bHasSafeLocation = false;

	/** 是否正在执行深坑回传，避免重复进入 */
	bool bIsRecoveringFromFall = false;

	/** 最近一次深坑回传发生的时间 */
	float LastFallRecoveryTime = -1.0f;

	/** 是否已进入起跳后的墙跑检测阶段 */
	bool bCanCheckWallRun = false;

	/** 本次腾空是否已经成功触发过墙跑提示 */
	bool bHasTriggeredWallRunThisJump = false;

	/** 墙跑开始时锁定的沿墙移动方向 */
	FVector WallRunDirection = FVector::ZeroVector;

	/** 当前墙跑依附的墙面法线 */
	FVector WallRunSurfaceNormal = FVector::ZeroVector;

	/** 进入墙跑前缓存的重力缩放 */
	float PreWallRunGravityScale = 1.0f;

	/** 进入墙跑前缓存的空中控制强度 */
	float PreWallRunAirControl = 0.0f;

	/** 进入墙跑前缓存的移动模式 */
	EMovementMode PreWallRunMovementMode = MOVE_Falling;

	/** 进入墙跑前缓存的自定义移动模式 */
	uint8 PreWallRunCustomMovementMode = 0;

	/** 第一人称相机默认的相对变换，用于还原头部 Socket 的原始跟随朝向 */
	FTransform DefaultFirstPersonCameraRelativeTransform = FTransform::Identity;

	/** 当前平滑后的头部旋转偏移 */
	FRotator CurrentHeadCameraRotationOffset = FRotator::ZeroRotator;

	/** 当前墙跑视角目标 Roll，右墙为负左墙为正，非墙跑为 0 */
	float TargetWallRunCameraRoll = 0.0f;

	/** 当前墙跑视角已经平滑到的 Roll 值 */
	float CurrentWallRunCameraRoll = 0.0f;

public:

	/** 生命值变化委托，参数为当前生命百分比 */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FUEGameJamPlayerDamagedDelegate OnDamaged;

	/** 玩家死亡委托 */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FUEGameJamPlayerDeathDelegate OnDeath;

public:

	AGsPlayer();

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

	/** 从 DataTable 应用玩家手感数值，未配置时使用 C++ 默认数值 */
	void ApplyPlayerTuningFromDataTable();

	/** 获取当前玩家手感数值行 */
	const FGsPlayerTuningRow& GetPlayerTuning() const { return CurrentPlayerTuning ? *CurrentPlayerTuning : DefaultPlayerTuning; }

	/** 输入系统回调：处理移动输入 */
	void MoveInput(const FInputActionValue& Value);

	/** 输入系统回调：处理视角输入 */
	void LookInput(const FInputActionValue& Value);

public:

	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category="Player Character|Realm")
	bool IsInsideActiveRealmReveal() const;

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoStartFiring();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSkill();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSlide();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSlideEnd();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoDash();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoFalcula();

	UFUNCTION(BlueprintPure, Category="Action")
	bool IsCharacterActionActive() const;

	UFUNCTION(BlueprintPure, Category="Action")
	bool IsSliding() const;

	UFUNCTION(BlueprintPure, Category="Action")
	bool IsDashing() const;

	UFUNCTION(BlueprintPure, Category="Action")
	bool IsWallRunning() const;

	UFUNCTION(BlueprintPure, Category="Health")
	float GetLifePercent() const;

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const;

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
	UBoxComponent* GetMeleeDamageCollision() const { return MeleeDamageCollision; }

protected:

	/** 清空移动输入缓存，避免停下后还能沿旧方向滑铲 */
	void OnMoveInputCompleted(const FInputActionValue& Value);

	/** 开始一个角色动作，如果当前已有动作则返回 false */
	bool TryStartCharacterAction(EUEGameJamPlayerAction Action, float Duration);

	/** 结束当前角色动作 */
	void FinishCharacterAction();

	/** 正常结束冲刺并恢复移动状态，只保留进入冲刺前的前向惯性和竖直速度 */
	void FinishDash();

	/** 强制中断冲刺并恢复移动组件，不恢复进入冲刺前速度 */
	void AbortDash();

	/** 尝试开始滑铲 */
	bool StartSlide();

	/** 尝试开始冲刺 */
	bool StartDash();

	/** 根据最近一次移动输入计算滑铲方向 */
	bool TryGetSlideInputDirection(FVector& OutSlideDirection) const;

	/** 停止滑铲；如果站起空间不足且未强制恢复则返回 false */
	bool StopSlide(bool bForceRestore);

	/** 判断滑铲后的胶囊体是否可以安全恢复到站立高度 */
	bool CanRestoreSlideCapsule() const;

	/** 每帧更新滑铲速度与结束条件 */
	void UpdateSlide(float DeltaSeconds);

	/** 每帧推进冲刺位移并处理碰撞与结束条件 */
	void UpdateDash(float DeltaSeconds);

	/** 起跳后开启墙跑检测延迟 */
	void StartWallRunDetectionDelay();

	/** 延迟结束后正式允许墙跑检测 */
	void EnableWallRunDetection();

	/** 重置本次腾空的墙跑检测状态 */
	void ResetWallRunDetection();

	/** 每帧检测是否满足墙跑触发条件 */
	void UpdateWallRunDetection();

	/** 从角色左右两侧寻找可用于墙跑的墙面 */
	bool TryFindWallRunSurface(FHitResult& OutWallHit, FVector& OutWallNormal) const;

	/** 沿已锁定的墙面法线方向确认墙跑依附墙面仍然存在 */
	bool TryFindWallRunSurfaceAlongNormal(const FVector& ExpectedWallNormal, FHitResult& OutWallHit, FVector& OutWallNormal) const;

	/** 判断当前状态是否满足墙跑触发条件 */
	bool CanTriggerWallRun(const FVector& WallNormal) const;

	/** 开始一次沿墙横向跑动 */
	bool StartWallRun(const FVector& WallNormal);

	/** 每帧维持墙跑移动与退出条件 */
	void UpdateWallRun(float DeltaSeconds);

	/** 结束当前墙跑并恢复普通空中状态 */
	void StopWallRun();

	/** 尝试从墙跑状态跳出并重新开启墙跑检测延迟 */
	bool TryWallRunJump();

	/** 每帧平滑更新墙跑时的相机倾斜 */
	void UpdateWallRunCameraTilt(float DeltaSeconds);

	/** 每帧更新第一人称相机朝向，合成控制器瞄准、头部轻晃与墙跑倾斜 */
	void UpdateFirstPersonCameraRotation(float DeltaSeconds);

	/** 设置墙跑相机倾斜的目标 Roll */
	void SetWallRunCameraTiltTarget(float InTargetRoll);

	/** 清理冲刺运行时状态缓存 */
	void ClearDashState();

	/** 更新最近一次安全落地点 */
	void UpdateSafeLandingTransform();

	/** 触发深坑回传 */
	void RecoverFromDeepFall();

	/** 开始一次近战攻击 */
	bool StartMeleeAttack();

	/** 获取技能发射使用的真实玩家视角位置与朝向 */
	bool GetSkillViewPoint(FVector& OutViewLocation, FRotator& OutViewRotation) const;

	/** 获取技能沿屏幕中心瞄准时的目标点 */
	FVector GetSkillAimTarget(const FVector& ViewLocation, const FVector& ViewDirection) const;

	/** 释放一次技能球 */
	bool StartSkillCast();

	/** 查找当前靠近、准星对准且视线无遮挡的钩爪点 */
	AGsGrapplePoint* FindReachableGrapplePoint() const;

	/** 读取近战伤害盒当前重叠对象并对命中目标造成伤害 */
	void PerformMeleeHit();

	/** 角色死亡时的统一处理 */
	void Die();

	/** 死亡后延时销毁回调 */
	void OnDeferredDestroy();

	/** 蓝图技能输入回调 */
	UFUNCTION(BlueprintImplementableEvent, Category="Player Character", meta = (DisplayName = "On Skill Input"))
	void BP_OnSkillInput();

	/** 蓝图死亡回调 */
	UFUNCTION(BlueprintImplementableEvent, Category="Player Character", meta = (DisplayName = "On Death"))
	void BP_OnDeath();
};
