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
	Sit,        // Sitting1 (driving, idle: |throttle| < 0.1 and speed < 50 cm/s)
	Water,      // Treading_Water <-> Swimming by speed
	Climb,      // Swimming_To_Edge, scrubbed over the climb
	Dizzy,      // Dizzy_Idle (stunned)
	Swing       // paddle swing: idle base + procedural torso twist/pitch/lean, arms follow the paddle arc (IK)
};

/**
 * Native brawler AnimInstance (T02 step 5): retargeted Mixamo clips per state with a short crossfade, plus Two Bone IK on BOTH
 * arms onto the paddle's Grip_R / Grip_L sockets while standing (T03.1: the palm, not the wrist, lands on the socket; the hand turns
 * so its knuckle line runs along the shaft; fingers + thumb curl around it; `lf.debug.ik 1` draws it). Written in C++
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

	// --- Procedural oar rowing (T03.1, director: no usable rowing clip). Sitting1 legs; both hands together on the oar handles
	// (IK), the torso leans with the stroke; the boat's oars follow the hands. The rower faces the stern (classic pull). ---

	/** Strokes per second = boat speed / RowStrokeSpeedPerRate, at least RowMinStrokeRate x |throttle|, at most RowMaxStrokeRate.
	 *  No throttle: hands rest on the handles (blades out). Reverse throttle rows backward. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row", meta=(ClampMin="1"))
	float RowStrokeSpeedPerRate = 500.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row", meta=(ClampMin="0"))
	float RowMinStrokeRate = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row", meta=(ClampMin="0.05"))
	float RowMaxStrokeRate = 0.9f;

	/** Fraction of the cycle spent on the drive (blades in, pulling); the rest is the recovery. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row", meta=(ClampMin="0.1", ClampMax="0.9"))
	float RowDriveFraction = 0.4f;

	/** Hand path relative to the shoulder midpoint (mesh units, the rower's own axes): forward at the catch / the finish / at rest,
	 *  half the spread between the hands, height on the drive / the recovery (lower = blades up) / at rest. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowCatchX = 50.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowFinishX = 12.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowRestX = 28.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowHandSpread = 12.f;

	/** The handles meet near the centreline (70 cm inboard of oarlocks 67 cm out): the left hand rows this much higher than the right
	 *  (cm, mesh units; split +/- half) so the hands pass over each other instead of through. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowHandStagger = 14.f;

	// --- Seated feet (T03.2): two-bone IK plants both feet on the boat's floor boards (knees bend more on the low thwart) ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Sit")
	bool bSitFootIK = true;

	// --- Head while rowing (T03.2): layered blend on neck_01 / neck_02 / head ---
	/** Time for the sitting-idle look-around to fade out (rowing) / back in (idle boat), s. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Head", meta=(ClampMin="0.01"))
	float HeadLookFadeTime = 0.3f;

	/** Head yaw toward the steering direction at full steer (deg). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Head")
	float HeadSteerBiasDeg = 12.f;

	/** Look-ahead pitch (deg, - = down a little toward the water ahead). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Head")
	float HeadLookPitchDeg = -5.f;

	/** Share of the head turn on neck_01 / neck_02 / head (sums to 1). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Head")
	FVector HeadSplit = FVector(0.3f, 0.3f, 0.4f);

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowDriveZ = -14.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowRecoverZ = -24.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowRestZ = -26.f;

	/** Torso lean (deg, + = toward the stern he faces) at the catch / the finish. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowLeanCatchDeg = 22.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowLeanFinishDeg = -15.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row", meta=(ClampMin="0.01"))
	float RowBlendTime = 0.3f;

	/** Elbow direction while rowing (mesh units from each shoulder): out to the side, back, down - elbows flare outward. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowElbowOut = 40.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowElbowBack = 15.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Row")
	float RowElbowDown = 20.f;

	/** How much of the walk clip's pelvis sway is removed (0 = clip as is, 1 = pelvis keeps the idle orientation). Was 0.6 for the
	 *  mannequin's MF_ walk; Wheelbarrow_Walk needs none. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="0", ClampMax="1"))
	float WalkHipDamp = 0.f;

	/** Strip the walk clip's horizontal hip travel (Mixamo clips that aren't "in place"). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim")
	bool bWalkInPlace = true;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> TreadAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> SwimAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> ClimbAnim;
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim") TObjectPtr<UAnimSequence> DizzyAnim;

	/** Ground speed (cm/s) at which the walk cycle is fully blended in. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="1"))
	float WalkFullSpeed = 180.f;

	/** Swim speed (cm/s) at which Swimming fully replaces Treading_Water. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="1"))
	float SwimFullSpeed = 100.f;

	// --- Procedural swing torso (T03.1; replaces the Great_Sword_Slash clip, which fought the paddle IK) ---

	/** Spine yaw at the wind-up (+ = turned right, toward the paddle behind the right shoulder) and at the follow-through. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Swing")
	float SwingTwistWindupDeg = 35.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Swing")
	float SwingTwistFollowDeg = -45.f;

	/** Forward pitch at the middle of the strike (full at strength 1, half at strength 0). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Swing")
	float SwingPitchDeg = 10.f;

	/** Charged swings: pelvis shifts forward (cm, world) and the spine leans a little more into the strike. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Swing")
	float ChargedLeanCm = 6.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Swing")
	float ChargedLeanPitchDeg = 6.f;

	/** How the twist/pitch splits over spine_01 / spine_02 / spine_03 (sums to 1). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Swing")
	FVector SpineSplit = FVector(0.2f, 0.3f, 0.5f);

	/** Ease toward the wind-up while charging and back to neutral at rest (1/s). The strike and recover follow the paddle exactly. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Swing", meta=(ClampMin="0"))
	float TorsoFollowRate = 14.f;

	/** Crossfade between clips (s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim", meta=(ClampMin="0"))
	float BlendTime = 0.2f;

	// --- Paddle grip (T03.1 item 2): hands on the Grip_R / Grip_L sockets ---

	/** Grip point inside the hand, as a fraction of hand bone -> middle_01 (the knuckle line = 1). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip", meta=(ClampMin="0", ClampMax="1.5"))
	float GripAlongHand = 0.9f;

	/** Grip point depth off the knuckle line toward the palm (cm, mesh space = unscaled Manny): roughly the shaft radius. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	float GripPalmDepth = 3.f;

	/** Thumb (index) side of each hand points toward the blade. False = toward the T-grip. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	bool bRightThumbTowardBlade = true;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	bool bLeftThumbTowardBlade = true;

	/** Palm grip (director): right hand on top of the shaft, palm down; left hand under it, palm up. False = least twist from the clip. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	bool bRightOverhand = true;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	bool bLeftUnderhand = true;

	/** Extra finger curl toward the palm per joint (deg; _01, _02, _03), added to the playing clip. Thumb separately. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	FVector FingerCurlDeg = FVector(45.f, 55.f, 35.f);

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	FVector ThumbCurlDeg = FVector(0.f, 25.f, 20.f);

	/** The left hand lets go when even the nearest point of the shaft is this far beyond its reach (fraction of the arm). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip", meta=(ClampMin="0"))
	float LeftReleaseShortfall = 0.02f;

	/** Ease time for the left hand letting go / re-gripping (s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip", meta=(ClampMin="0.01"))
	float LeftRegripTime = 0.12f;

	/** Where the free left hand rests while it has let go (cm, mesh space, from the left hip joint): out to the left, forward, up. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	float FreeHandOut = 12.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	float FreeHandForward = 10.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Grip")
	float FreeHandUp = 0.f;

	// --- Seated hands (T03.1): Sitting1 / Sitting_Rowing put the hands inside the thighs; push them out to the thigh surface ---

	/** Thigh radius around the thigh -> calf bone line (cm, mesh space = unscaled Manny). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Sit", meta=(ClampMin="0"))
	float SitThighRadius = 9.f;

	/** Extra gap between the palm and the thigh surface (cm, mesh space). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Anim|Sit", meta=(ClampMin="0"))
	float SitHandClearance = 2.f;

	// Game thread -> proxy.
	ELFBrawlerClip Clip = ELFBrawlerClip::Locomotion;
	float Speed = 0.f;
	float Scrub = 0.f; // 0..1 for Climb; swing phase alpha for Swing (log only)
	bool bChargedOneHand = false; // charged punch: left hand off the paddle (charge hold past the threshold + the charged strike)
	float TorsoYaw = 0.f;    // deg, + = right
	float TorsoPitch = 0.f;  // deg, + = forward
	float PelvisForward = 0.f; // cm, world
	float RowRate = 0.f;
	float RowStrokePhase = 0.f; // game thread
	float RowStrokeWeight = 0.f;
	bool bRowIK = false;        // arms on the oar handles (rowing), not the paddle
	bool bArmIK = false;
	float HeadLookWeight = 0.f; // 0 = clip's look-around, 1 = look ahead (rowing)
	float HeadYaw = 0.f;        // deg, + = right (steering bias)
	bool bFootIK = false;       // seated: plant the feet on the floor plane below
	FVector FloorPointCS = FVector::ZeroVector;
	FVector FloorNormalCS = FVector::UpVector;
	uint8 LastSwingPhase = 0;   // swing blend-in: torso values at the start of the strike
	float StrikeStartYaw = 0.f;
	float StrikeStartPitch = 0.f;
	float StrikeStartLean = 0.f;
	FTransform RightTargetCS = FTransform::Identity; // Grip_R in mesh component space (X = shaft toward the blade)
	FTransform LeftTargetCS = FTransform::Identity;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	void DrawGripDebug() const;
	void DrawSitDebug() const;
	mutable double LastIKLogTime = -100.0;
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
	void LogGrip(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref);
	void ApplySwingTorso(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref) const;
	void ApplyHeadLook(FCompactPose& Pose) const;
	void PlantFeet(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref);

	float HeadLookWeight = 0.f;
	float HeadYaw = 0.f;
	float HeadLookPitchDeg = -5.f;
	FVector HeadSplit = FVector(0.3f, 0.3f, 0.4f);
	bool bFootIK = false;
	FVector FloorPointCS = FVector::ZeroVector;
	FVector FloorNormalCS = FVector::UpVector;
	float FootLogClock = 0.f;

	TObjectPtr<UAnimSequence> Idle, Walk, Row, Sit, Tread, Swim, ClimbSeq, Dizzy;
	float TorsoYaw = 0.f;
	float TorsoPitch = 0.f;
	float PelvisForward = 0.f;
	FVector SpineSplit = FVector(0.2f, 0.3f, 0.5f);
	float RowRate = 0.f;
	float RowTime = 0.f;
	float WalkHipDamp = 0.f;
	bool bWalkInPlace = true;
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
	FTransform RightTarget = FTransform::Identity;
	FTransform LeftTarget = FTransform::Identity;
	float GripAlongHand = 0.9f;
	float GripPalmDepth = 3.f;
	bool bRightThumbTowardBlade = true;
	bool bLeftThumbTowardBlade = true;
	FVector FingerCurlDeg = FVector::ZeroVector;
	FVector ThumbCurlDeg = FVector::ZeroVector;
	float SitThighRadius = 9.f;
	float SitHandClearance = 2.f;
	bool bDebugIK = false; // lf.debug.ik
	float MeshScale = 1.f;
	float IKLogClock = 0.f;
	float LastDelta = 0.f;
	float LeftGripWeight = 1.f; // eased 0 (let go) .. 1 (on the paddle)
	float LeftReleaseShortfall = 0.02f;
	float LeftRegripTime = 0.12f;
	bool bRightOverhand = true;
	bool bLeftUnderhand = true;
	bool bChargedOneHand = false;
	bool bRowIK = false;
	float RowElbowOut = 40.f;
	float RowElbowBack = 15.f;
	float RowElbowDown = 20.f;
	float FreeHandOut = 12.f;
	float FreeHandForward = 10.f;
	float FreeHandUp = 0.f;
	FString OwnerName;

	float LoopTime = 0.f; // shared phase for looping clips (each wraps by its own length)
	float WalkTime = 0.f;
};
