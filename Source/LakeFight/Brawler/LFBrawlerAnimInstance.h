#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "LFBrawlerAnimInstance.generated.h"

class UAnimSequence;

/** Which clip the brawler plays (picked on the game thread from the brawler's state). */
enum class ELFBrawlerClip : uint8
{
	Locomotion, // idle <-> walk by speed
	Row,        // Sitting_Rowing (driving), play rate = throttle (reverse plays backward)
	Sit,        // Sitting (driving, idle: |throttle| < 0.1 and speed < 50 cm/s)
	Water,      // Treading_Water <-> Swimming by speed
	Climb,      // Swimming_To_Edge, scrubbed over the climb
	Dizzy,      // Dizzy_Idle (stunned)
	Slash       // Great_Sword_Slash, scrubbed by the swing phase (paddle IK keeps both hands on the paddle)
};

/**
 * Native brawler AnimInstance (T02 step 5): retargeted Mixamo clips per state with a short crossfade, plus Two Bone IK on BOTH
 * arms onto the paddle's grip points while standing (right hand at the T-grip, left ~55 cm down the shaft). Written in C++
 * because the AnimGraph can't be authored through MCP. The ragdoll is physics (not this class).
 */
UCLASS()
class LAKEFIGHT_API ULFBrawlerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	ULFBrawlerAnimInstance();

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> WalkAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> RowAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> SitAnim;

	/** Rowing play rate at full throttle (scales linearly with throttle; negative = backward). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") float RowRateAtFullThrottle = 1.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> TreadAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> SwimAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> ClimbAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> DizzyAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> SlashAnim;

	/** Ground speed (cm/s) at which the walk cycle is fully blended in. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="1"))
	float WalkFullSpeed = 180.f;

	/** Swim speed (cm/s) at which Swimming fully replaces Treading_Water. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="1"))
	float SwimFullSpeed = 100.f;

	/** Slash clip fractions: wind-up hold while charging, then the strike plays over the swing, then the rest over recover. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="0", ClampMax="1"))
	float SlashWindupFrac = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="0", ClampMax="1"))
	float SlashStrikeEndFrac = 0.7f;

	/** Crossfade between clips (s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="0"))
	float BlendTime = 0.2f;

	// Game thread -> proxy.
	ELFBrawlerClip Clip = ELFBrawlerClip::Locomotion;
	float Speed = 0.f;
	float Scrub = 0.f; // 0..1 for Climb / Slash
	float RowRate = 0.f;
	bool bArmIK = false;
	FVector RightTargetCS = FVector::ZeroVector;
	FVector LeftTargetCS = FVector::ZeroVector;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};

struct FLFBrawlerAnimProxy : public FAnimInstanceProxy
{
	FLFBrawlerAnimProxy() = default;
	explicit FLFBrawlerAnimProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	void EvalClip(ELFBrawlerClip C, float InScrub, FAnimationPoseData& Out) const;

	TObjectPtr<UAnimSequence> Idle, Walk, Row, Sit, Tread, Swim, ClimbSeq, Dizzy, Slash;
	float RowRate = 0.f;
	float RowTime = 0.f;
	float WalkFullSpeed = 180.f;
	float SwimFullSpeed = 100.f;
	float BlendTime = 0.2f;

	ELFBrawlerClip Clip = ELFBrawlerClip::Locomotion;
	ELFBrawlerClip PrevClip = ELFBrawlerClip::Locomotion;
	float Fade = 1.f; // 0 -> 1 from PrevClip to Clip
	float Speed = 0.f;
	float Scrub = 0.f;
	float PrevScrub = 0.f;
	bool bArmIK = false;
	FVector RightTarget = FVector::ZeroVector;
	FVector LeftTarget = FVector::ZeroVector;

	float LoopTime = 0.f; // shared phase for looping clips (each wraps by its own length)
	float WalkTime = 0.f;
};
