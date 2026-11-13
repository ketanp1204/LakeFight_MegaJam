#include "Brawler/LFBrawlerAnimInstance.h"

#include "Brawler/LFBrawler.h"
#include "Boat/LFBoat.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Water/LFWater.h"

static TAutoConsoleVariable<int32> CVarLFDebugIK(TEXT("lf.debug.ik"), 0,
	TEXT("1 = draw the paddle grip IK: sockets (green Grip_R, blue Grip_L), palm grip points (red) + error, palm normal (yellow), ")
	TEXT("thumb side (cyan); logs [ik] grip errors every 2 s."));

ULFBrawlerAnimInstance::ULFBrawlerAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleSeq(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	// Director (13 Nov): Wheelbarrow_Walk replaces the mannequin's MF_ (feminine) unarmed walk.
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkSeq(TEXT("/Game/LakeFight/Anim/Manny/Wheelbarrow_Walk.Wheelbarrow_Walk"));
	// Retargeted Mixamo clips (director, Content/LakeFight/Anim/Manny).
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RowSeq(TEXT("/Game/LakeFight/Anim/Manny/Sitting_Rowing.Sitting_Rowing"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SitSeq(TEXT("/Game/LakeFight/Anim/Manny/Sitting1.Sitting1"));
	SitAnim = SitSeq.Object;
	static ConstructorHelpers::FObjectFinder<UAnimSequence> TreadSeq(TEXT("/Game/LakeFight/Anim/Manny/Treading_Water.Treading_Water"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SwimSeq(TEXT("/Game/LakeFight/Anim/Manny/Swimming.Swimming"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ClimbSeq(TEXT("/Game/LakeFight/Anim/Manny/Swimming_To_Edge.Swimming_To_Edge"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DizzySeq(TEXT("/Game/LakeFight/Anim/Manny/Dizzy_Idle.Dizzy_Idle"));
	IdleAnim = IdleSeq.Object;
	WalkAnim = WalkSeq.Object;
	RowAnim = RowSeq.Object;
	TreadAnim = TreadSeq.Object;
	SwimAnim = SwimSeq.Object;
	ClimbAnim = ClimbSeq.Object;
	DizzyAnim = DizzySeq.Object;
}

void ULFBrawlerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (CVarLFDebugIK.GetValueOnGameThread() != 0)
	{
		DrawGripDebug(); // before the targets are replaced: the bones were solved for these targets
		DrawSitDebug();
	}
	const ALFBrawler* B = Cast<ALFBrawler>(TryGetPawnOwner());
	if (!B)
	{
		bArmIK = false;
		Clip = ELFBrawlerClip::Locomotion;
		return;
	}
	Speed = float(B->GetVelocity().Size2D());
	FLFBrawlerAnimView V;
	B->GetAnimView(V);
	Scrub = 0.f;
	switch (V.State)
	{
	case ELFBrawlerState::Driving:
		// Rowing (the boat's oars follow the hands): play rate follows the boat's speed, capped; reverse plays backward. No throttle:
		// sit. The paddle is stowed on the back while driving.
		RowRate = 0.f;
		Clip = ELFBrawlerClip::Sit; // procedural rowing on the Sitting1 base (below)
		break;
	case ELFBrawlerState::Swimming: Clip = ELFBrawlerClip::Water; break;
	case ELFBrawlerState::Climbing: Clip = ELFBrawlerClip::Climb; Scrub = V.ClimbAlpha; break;
	case ELFBrawlerState::Standing:
		if (V.SwingPhase != 0)
		{
			Clip = ELFBrawlerClip::Swing;
			Scrub = V.SwingAlpha;
		}
		else
		{
			Clip = V.bStunned ? ELFBrawlerClip::Dizzy : ELFBrawlerClip::Locomotion;
		}
		break;
	default: Clip = ELFBrawlerClip::Locomotion; break;
	}
	// Procedural swing torso: the spine twists with the paddle's arc (wind-up right, follow-through left), pitches forward
	// over the strike, and charged swings lean in. Strike / recover track the paddle exactly; charge / rest ease.
	{
		const bool bSwinging = V.State == ELFBrawlerState::Standing && V.SwingPhase != 0;
		bChargedOneHand = bSwinging && (V.bChargeHeld || (V.bChargedSwing && V.SwingPhase == 2));
		const float Ease = 1.f - FMath::Exp(-TorsoFollowRate * DeltaSeconds);
		const float Alpha = FMath::SmoothStep(0.f, 1.f, V.SwingAlpha);
		float WantYaw = 0.f, WantPitch = 0.f, WantLean = 0.f;
		bool bExact = false;
		if (bSwinging && V.SwingPhase == 1) // charging: wound up
		{
			WantYaw = SwingTwistWindupDeg;
		}
		else if (bSwinging && V.SwingPhase == 2) // strike (the paddle sweeps with the same SmoothStep)
		{
			const float Bell = FMath::Sin(PI * V.SwingAlpha);
			const float Charged = V.bChargedSwing ? 1.f : 0.f;
			WantYaw = FMath::Lerp(SwingTwistWindupDeg, SwingTwistFollowDeg, Alpha);
			WantPitch = Bell * (SwingPitchDeg * (0.5f + 0.5f * V.SwingStrength) + Charged * ChargedLeanPitchDeg);
			WantLean = Bell * Charged * ChargedLeanCm;
			bExact = true;
		}
		else if (bSwinging && V.SwingPhase == 3) // recover back to neutral
		{
			WantYaw = FMath::Lerp(SwingTwistFollowDeg, 0.f, Alpha);
			bExact = true;
		}
		TorsoYaw = bExact ? WantYaw : FMath::Lerp(TorsoYaw, WantYaw, Ease);
		TorsoPitch = bExact ? WantPitch : FMath::Lerp(TorsoPitch, WantPitch, Ease);
		PelvisForward = bExact ? WantLean : FMath::Lerp(PelvisForward, WantLean, Ease);
	}

	FTransform R, L;
	bArmIK = B->GetPaddleGripTargets(R, L);
	if (bArmIK)
	{
		const FTransform& C = GetSkelMeshComponent()->GetComponentTransform();
		RightTargetCS = R.GetRelativeTransform(C);
		LeftTargetCS = L.GetRelativeTransform(C);
		RightTargetCS.SetScale3D(FVector::OneVector);
		LeftTargetCS.SetScale3D(FVector::OneVector);
		// The shaft axis comes from the two socket positions: the imported sockets' own X axis is not along the shaft
		// (FBX empty axis conversion), and the hand alignment + slide need the true shaft direction (X = toward the blade).
		const FVector Shaft = (LeftTargetCS.GetLocation() - RightTargetCS.GetLocation()).GetSafeNormal();
		if (!Shaft.IsNearlyZero())
		{
			const FQuat Q = FRotationMatrix::MakeFromX(Shaft).ToQuat();
			RightTargetCS.SetRotation(Q);
			LeftTargetCS.SetRotation(Q);
		}
	}

	// Procedural oar rowing while driving: both hands together on the handles, torso lean with the stroke.
	bRowIK = false;
	const ALFBoat* RowBoat = V.State == ELFBrawlerState::Driving ? B->GetBoat() : nullptr;
	if (RowBoat)
	{
		float Rate = 0.f;
		if (FMath::Abs(V.DriveThrottle) >= 0.1f)
		{
			Rate = FMath::Sign(V.DriveThrottle) * FMath::Clamp(FMath::Max(RowMinStrokeRate * FMath::Abs(V.DriveThrottle),
				V.BoatSpeed / RowStrokeSpeedPerRate), 0.f, RowMaxStrokeRate);
		}
		// Director (14 Nov): forward throttle = PUSH stroke (the cycle runs backward: handles pushed out with the blades in, drawn
		// back with them lifted - the blades sweep sternward, the boat moves where he faces); reverse throttle = PULL stroke.
		// The torso lean follows the hands' fore-aft position, so it reverses with the stroke (he leans into the push).
		RowStrokePhase = FMath::Fmod(RowStrokePhase - Rate * DeltaSeconds + 1.f, 1.f);
		RowStrokeWeight = FMath::FInterpConstantTo(RowStrokeWeight, Rate != 0.f ? 1.f : 0.f, DeltaSeconds, 1.f / RowBlendTime);
		const float P = RowStrokePhase;
		float X, Z;
		if (P < RowDriveFraction) // drive: blades in, handles pulled to the ribs
		{
			X = FMath::Lerp(RowCatchX, RowFinishX, FMath::SmoothStep(0.f, 1.f, P / RowDriveFraction));
			Z = RowDriveZ;
		}
		else // recovery: handles down (blades up) and back out to the catch
		{
			const float A = (P - RowDriveFraction) / (1.f - RowDriveFraction);
			X = FMath::Lerp(RowFinishX, RowCatchX, FMath::SmoothStep(0.f, 1.f, A));
			Z = FMath::Lerp(RowDriveZ, RowRecoverZ, FMath::Sin(PI * A));
		}
		const float W = RowStrokeWeight;
		X = FMath::Lerp(RowRestX, X, W);
		Z = FMath::Lerp(RowRestZ, Z, W);
		const float LeanT = FMath::Clamp((X - RowFinishX) / FMath::Max(RowCatchX - RowFinishX, 1.f), 0.f, 1.f);
		TorsoPitch = W * FMath::Lerp(RowLeanFinishDeg, RowLeanCatchDeg, LeanT);
		TorsoYaw = 0.f;
		PelvisForward = 0.f;

		USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
		const FTransform& C = Mesh->GetComponentTransform();
		const float S = float(C.GetScale3D().X);
		const FVector Mid = (Mesh->GetBoneLocation(TEXT("upperarm_r")) + Mesh->GetBoneLocation(TEXT("upperarm_l"))) * 0.5f;
		const FVector F = B->GetActorForwardVector(), Rt = B->GetActorRightVector(), U = B->GetActorUpVector();
		const FVector LockL = RowBoat->GetOarlockWorld(true), LockR = RowBoat->GetOarlockWorld(false);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const bool bRight = Side == 0;
			const FVector Goal = Mid + (F * X + Rt * ((bRight ? 1.f : -1.f) * RowHandSpread) + U * Z) * S;
			const FVector Lock = FVector::DistSquared(Goal, LockL) < FVector::DistSquared(Goal, LockR) ? LockL : LockR;
			FTransform T(FRotationMatrix::MakeFromX((Lock - Goal).GetSafeNormal()).ToQuat(), Goal);
			T = T.GetRelativeTransform(C);
			T.SetScale3D(FVector::OneVector);
			(bRight ? RightTargetCS : LeftTargetCS) = T;
		}
		bArmIK = true;
		bRowIK = true;
	}
	else
	{
		RowStrokeWeight = 0.f;
	}
}

namespace LFGrip
{
	/** Hand frame from bone positions: F = wrist -> middle knuckle, S = pinky -> index (thumb side), Palm = the way the palm faces. */
	bool HandFrame(const FVector& H, const FVector& M, const FVector& I, const FVector& P, bool bRight, FVector& F, FVector& S, FVector& Palm)
	{
		F = (M - H).GetSafeNormal();
		S = I - P;
		S = (S - F * FVector::DotProduct(S, F)).GetSafeNormal();
		if (F.IsNearlyZero() || S.IsNearlyZero())
		{
			return false;
		}
		// Right hand palm down, fingers forward: the thumb is on the left, so F x S points down = the palm. Mirrored for the left.
		Palm = bRight ? FVector::CrossProduct(F, S) : FVector::CrossProduct(S, F);
		return true;
	}

	FVector GripPoint(const FVector& H, const FVector& M, const FVector& Palm, float Along, float Depth)
	{
		return H + (M - H) * Along + Palm * Depth;
	}

	FName Bone(const TCHAR* Base, bool bRight)
	{
		return FName(*FString::Printf(TEXT("%s_%s"), Base, bRight ? TEXT("r") : TEXT("l")));
	}
}

void ULFBrawlerAnimInstance::DrawSitDebug() const
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const UWorld* W = GetWorld();
	if (!Mesh || !W || (Clip != ELFBrawlerClip::Sit && Clip != ELFBrawlerClip::Row))
	{
		return;
	}
	// Palm -> nearest thigh surface (cm, world; < 0 = inside the thigh), from the last evaluated pose.
	const float Scale = float(Mesh->GetComponentTransform().GetScale3D().X);
	auto CSPos = [Mesh](const TCHAR* Base, bool bRight)
	{
		return Mesh->GetSocketTransform(LFGrip::Bone(Base, bRight), RTS_Component).GetLocation();
	};
	float Gap[2] = { 999.f, 999.f };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVector Palm = (CSPos(TEXT("hand"), Side == 0) + CSPos(TEXT("middle_01"), Side == 0)) * 0.5f;
		for (int32 Leg = 0; Leg < 2; ++Leg)
		{
			const FVector Closest = FMath::ClosestPointOnSegment(Palm, CSPos(TEXT("thigh"), Leg == 0), CSPos(TEXT("calf"), Leg == 0));
			Gap[Side] = FMath::Min(Gap[Side], (float(FVector::Dist(Palm, Closest)) - SitThighRadius) * Scale);
		}
	}
	const double Now = W->GetTimeSeconds();
	if (Now - LastIKLogTime >= 2.0)
	{
		LastIKLogTime = Now;
		UE_LOG(LogLF, Log, TEXT("[ik] %s %s palm -> thigh surface R %.1f cm, L %.1f cm (< 0 = inside)"), *GetNameSafe(GetOwningActor()),
			Clip == ELFBrawlerClip::Sit ? TEXT("sit") : TEXT("row"), Gap[0], Gap[1]);
	}
}

void ULFBrawlerAnimInstance::DrawGripDebug() const
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const UWorld* W = GetWorld();
	if (!Mesh || !W || !bArmIK)
	{
		return;
	}
	// Drawn from the last evaluated bones (can lag a frame in a fast swing; the exact [ik] log comes from the anim proxy).
	const FTransform& C = Mesh->GetComponentTransform();
	const float Scale = float(C.GetScale3D().X);
	auto CSPos = [Mesh](const TCHAR* Base, bool bRight)
	{
		return Mesh->GetSocketTransform(LFGrip::Bone(Base, bRight), RTS_Component).GetLocation();
	};
	float Err[2] = { -1.f, -1.f };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bRight = Side == 0;
		const FTransform& Target = bRight ? RightTargetCS : LeftTargetCS;
		const FVector T = Target.GetLocation();
		const FVector A = Target.GetUnitAxis(EAxis::X);
		const FVector H = CSPos(TEXT("hand"), bRight);
		const FVector Tw = C.TransformPosition(T);
		DrawDebugSphere(W, Tw, 2.f, 8, bRight ? FColor::Green : FColor::Blue, false, -1.f, SDPG_Foreground);
		DrawDebugLine(W, C.TransformPosition(T - A * 10.f), C.TransformPosition(T + A * 10.f), FColor::White, false, -1.f, SDPG_Foreground, 0.5f);
		FVector F, S, Palm;
		if (!LFGrip::HandFrame(H, CSPos(TEXT("middle_01"), bRight), CSPos(TEXT("index_01"), bRight), CSPos(TEXT("pinky_01"), bRight), bRight, F, S, Palm))
		{
			continue;
		}
		const FVector G = LFGrip::GripPoint(H, CSPos(TEXT("middle_01"), bRight), Palm, GripAlongHand, GripPalmDepth);
		Err[Side] = float(FVector::Dist(G, T)) * Scale;
		const FVector Gw = C.TransformPosition(G);
		DrawDebugSphere(W, Gw, 1.5f, 8, FColor::Red, false, -1.f, SDPG_Foreground);
		DrawDebugLine(W, Gw, Tw, FColor::Red, false, -1.f, SDPG_Foreground, 0.3f);
		DrawDebugLine(W, Gw, Gw + C.TransformVector(Palm).GetSafeNormal() * 6.f, FColor::Yellow, false, -1.f, SDPG_Foreground, 0.4f); // palm normal
		DrawDebugLine(W, Gw, Gw + C.TransformVector(S).GetSafeNormal() * 6.f, FColor::Cyan, false, -1.f, SDPG_Foreground, 0.4f);   // thumb side
	}
	DrawDebugString(W, Mesh->GetBoneLocation(TEXT("head")) + FVector(0.f, 0.f, 30.f),
		FString::Printf(TEXT("IK R %.1f L %.1f cm"), Err[0], Err[1]), nullptr, FColor::White, 0.f, true);
}

FAnimInstanceProxy* ULFBrawlerAnimInstance::CreateAnimInstanceProxy()
{
	return new FLFBrawlerAnimProxy(this);
}

void ULFBrawlerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FLFBrawlerAnimProxy*>(InProxy);
}

void FLFBrawlerAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const ULFBrawlerAnimInstance* I = CastChecked<ULFBrawlerAnimInstance>(InAnimInstance);
	Idle = I->IdleAnim; Walk = I->WalkAnim; Row = I->RowAnim; Sit = I->SitAnim; RowRate = I->RowRate; Tread = I->TreadAnim; Swim = I->SwimAnim;
	ClimbSeq = I->ClimbAnim; Dizzy = I->DizzyAnim;
	TorsoYaw = I->TorsoYaw;
	TorsoPitch = I->TorsoPitch;
	PelvisForward = I->PelvisForward;
	SpineSplit = I->SpineSplit;
	WalkFullSpeed = I->WalkFullSpeed;
	SwimFullSpeed = I->SwimFullSpeed;
	BlendTime = I->BlendTime;
	if (I->Clip != Clip)
	{
		PrevClip = Clip;
		PrevScrub = Scrub;
		Clip = I->Clip;
		Fade = BlendTime > 0.f ? 0.f : 1.f;
	}
	Speed = I->Speed;
	Scrub = I->Scrub;
	bArmIK = I->bArmIK;
	RightTarget = I->RightTargetCS;
	LeftTarget = I->LeftTargetCS;
	GripAlongHand = I->GripAlongHand;
	GripPalmDepth = I->GripPalmDepth;
	bRightThumbTowardBlade = I->bRightThumbTowardBlade;
	bLeftThumbTowardBlade = I->bLeftThumbTowardBlade;
	FingerCurlDeg = I->FingerCurlDeg;
	ThumbCurlDeg = I->ThumbCurlDeg;
	SitThighRadius = I->SitThighRadius;
	SitHandClearance = I->SitHandClearance;
	WalkHipDamp = I->WalkHipDamp;
	bWalkInPlace = I->bWalkInPlace;
	LeftReleaseShortfall = I->LeftReleaseShortfall;
	bRightOverhand = I->bRightOverhand;
	bLeftUnderhand = I->bLeftUnderhand;
	bChargedOneHand = I->bChargedOneHand;
	bRowIK = I->bRowIK;
	RowElbowOut = I->RowElbowOut;
	RowElbowBack = I->RowElbowBack;
	RowElbowDown = I->RowElbowDown;
	LeftRegripTime = I->LeftRegripTime;
	FreeHandOut = I->FreeHandOut;
	FreeHandForward = I->FreeHandForward;
	FreeHandUp = I->FreeHandUp;
	bDebugIK = CVarLFDebugIK.GetValueOnGameThread() != 0;
	MeshScale = I->GetSkelMeshComponent() ? float(I->GetSkelMeshComponent()->GetComponentScale().X) : 1.f;
	OwnerName = GetNameSafe(I->GetOwningActor());
}

void FLFBrawlerAnimProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	LoopTime += DeltaSeconds;
	IKLogClock += DeltaSeconds;
	RowTime += DeltaSeconds * RowRate;
	LastDelta = DeltaSeconds;
	if (!bArmIK)
	{
		LeftGripWeight = 1.f;
	}
	// The walk cycle's natural speed scales with the body: a smaller brawler must step faster to cover the same cm/s.
	const float Rate = FMath::Clamp(Speed / (WalkFullSpeed * FMath::Max(MeshScale, 0.1f)), 0.4f, 2.f);
	WalkTime += DeltaSeconds * Rate;
	if (Fade < 1.f)
	{
		Fade = FMath::Min(1.f, Fade + DeltaSeconds / FMath::Max(BlendTime, 0.01f));
	}
}

namespace
{
	void SamplePose(const UAnimSequence* Seq, float Time, bool bLoop, FAnimationPoseData& Out)
	{
		const float Len = FMath::Max(Seq->GetPlayLength(), 0.01f);
		float T = bLoop ? FMath::Fmod(Time, Len) : FMath::Clamp(Time, 0.f, Len);
		if (T < 0.f)
		{
			T += Len; // backward play (reverse rowing)
		}
		Seq->GetAnimationPose(Out, FAnimExtractContext(double(T), false));
	}

	void BlendTwo(const UAnimSequence* A, float TA, bool bLoopA, const UAnimSequence* B, float TB, bool bLoopB, float WeightA, FAnimationPoseData& Out)
	{
		FCompactPose& OutPose = Out.GetPose();
		FCompactPose PA, PB;
		FBlendedCurve CA, CB;
		UE::Anim::FStackAttributeContainer AA, AB;
		PA.SetBoneContainer(&OutPose.GetBoneContainer());
		PB.SetBoneContainer(&OutPose.GetBoneContainer());
		CA.InitFrom(Out.GetCurve());
		CB.InitFrom(Out.GetCurve());
		FAnimationPoseData DA(PA, CA, AA);
		FAnimationPoseData DB(PB, CB, AB);
		SamplePose(A, TA, bLoopA, DA);
		SamplePose(B, TB, bLoopB, DB);
		FAnimationRuntime::BlendTwoPosesTogether(DA, DB, WeightA, Out);
	}
}

void FLFBrawlerAnimProxy::EvalClip(ELFBrawlerClip C, float InScrub, FAnimationPoseData& Out) const
{
	auto Valid = [](const UAnimSequence* S) { return S != nullptr; };
	switch (C)
	{
	case ELFBrawlerClip::Row:
		if (Valid(Row)) { SamplePose(Row, RowTime, true, Out); return; }
		break;
	case ELFBrawlerClip::Sit:
		if (Valid(Sit)) { SamplePose(Sit, LoopTime, true, Out); return; }
		if (Valid(Row)) { SamplePose(Row, RowTime, true, Out); return; }
		break;
	case ELFBrawlerClip::Water:
		if (Valid(Tread) && Valid(Swim))
		{
			const float A = FMath::Clamp(Speed / SwimFullSpeed, 0.f, 1.f);
			if (A <= KINDA_SMALL_NUMBER) { SamplePose(Tread, LoopTime, true, Out); }
			else if (A >= 1.f - KINDA_SMALL_NUMBER) { SamplePose(Swim, LoopTime, true, Out); }
			else { BlendTwo(Tread, LoopTime, true, Swim, LoopTime, true, 1.f - A, Out); }
			return;
		}
		break;
	case ELFBrawlerClip::Climb:
		if (Valid(ClimbSeq)) { SamplePose(ClimbSeq, InScrub * ClimbSeq->GetPlayLength(), false, Out); return; }
		break;
	case ELFBrawlerClip::Dizzy:
		if (Valid(Dizzy)) { SamplePose(Dizzy, LoopTime, true, Out); return; }
		break;
	default:
		break;
	}
	// Locomotion (and fallback when a clip is missing): idle <-> walk by speed.
	const float WalkAlpha = Walk ? FMath::Clamp(Speed / (WalkFullSpeed * FMath::Max(MeshScale, 0.1f)), 0.f, 1.f) : 0.f;
	if (WalkAlpha <= KINDA_SMALL_NUMBER || !Walk)
	{
		SamplePose(Idle, LoopTime, true, Out);
	}
	else
	{
		BlendTwo(Idle, LoopTime, true, Walk, WalkTime, true, 1.f - WalkAlpha, Out);
	}
}

namespace
{
	struct FGripParams
	{
		float Along = 0.9f;
		float Depth = 3.f;
		bool bThumbTowardBlade = true;
		float PalmVertical = 0.f;          // -1 = palm down (overhand), +1 = palm up (underhand), 0 = least twist from the clip
		bool bHasPole = false;             // bend the elbow toward Pole (component space) instead of the clip's elbow
		FVector Pole = FVector::ZeroVector;
		const FVector* SlideToward = nullptr; // out of reach: slide this hand along the shaft toward that point (the other grip)
		float MinHandGap = 12.f;
		float Weight = 1.f;             // IK blend (the left hand lets go when no point of the shaft is in reach)
		float* OutShortfall = nullptr;  // how far the wrist target stays beyond reach, as a fraction of the arm
		FVector Curl = FVector::ZeroVector;
		FVector ThumbCurl = FVector::ZeroVector;
	};

	FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, const FReferenceSkeleton& Ref, FName Name)
	{
		const int32 R = Ref.FindBoneIndex(Name);
		return R == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(R));
	}

	void SetCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Idx, const FTransform& T)
	{
		TArray<FBoneTransform> One;
		One.Add(FBoneTransform(Idx, T));
		CS.LocalBlendCSBoneTransforms(One, 1.f);
	}

	/** Curl each finger joint toward the palm (component space, parent first so children follow). */
	void CurlFingers(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref, bool bRight,
		const FVector& Palm, const FGripParams& G)
	{
		static const TCHAR* Fingers[] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
		for (int32 F = 0; F < 5; ++F)
		{
			const FVector& Curl = F == 0 ? G.ThumbCurl : G.Curl;
			FCompactPoseBoneIndex Idx[3] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
			bool bOk = true;
			for (int32 J = 0; J < 3; ++J)
			{
				Idx[J] = FindBone(Bones, Ref, LFGrip::Bone(*FString::Printf(TEXT("%s_0%d"), Fingers[F], J + 1), bRight));
				bOk &= Idx[J] != INDEX_NONE;
			}
			if (!bOk)
			{
				continue;
			}
			for (int32 J = 0; J < 3; ++J)
			{
				const float Deg = float(Curl[J]);
				if (FMath::IsNearlyZero(Deg))
				{
					continue;
				}
				FTransform T = CS.GetComponentSpaceTransform(Idx[J]);
				const FVector Dir = J < 2 ? CS.GetComponentSpaceTransform(Idx[J + 1]).GetLocation() - T.GetLocation()
					: T.GetLocation() - CS.GetComponentSpaceTransform(Idx[J - 1]).GetLocation();
				// Rotating Dir about (Dir x Palm) by a positive angle swings it toward the palm.
				const FVector Axis = FVector::CrossProduct(Dir.GetSafeNormal(), Palm).GetSafeNormal();
				if (Axis.IsNearlyZero())
				{
					continue;
				}
				T.SetRotation(FQuat(Axis, FMath::DegreesToRadians(Deg)) * T.GetRotation());
				SetCS(CS, Idx[J], T);
			}
		}
	}

	/**
	 * One arm onto its paddle socket: turn the hand so its knuckle line runs along the shaft (thumb side toward the blade or the
	 * T-grip, least twist from the clip), Two Bone IK the wrist so the palm grip point lands on the socket, then curl the fingers.
	 */
	void SolveHand(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref, bool bRight,
		const FTransform& Target, const FGripParams& G)
	{
		const FCompactPoseBoneIndex U = FindBone(Bones, Ref, LFGrip::Bone(TEXT("upperarm"), bRight));
		const FCompactPoseBoneIndex L = FindBone(Bones, Ref, LFGrip::Bone(TEXT("lowerarm"), bRight));
		const FCompactPoseBoneIndex H = FindBone(Bones, Ref, LFGrip::Bone(TEXT("hand"), bRight));
		if (U == INDEX_NONE || L == INDEX_NONE || H == INDEX_NONE)
		{
			return;
		}
		FTransform UT = CS.GetComponentSpaceTransform(U);
		FTransform LT = CS.GetComponentSpaceTransform(L);
		FTransform HT = CS.GetComponentSpaceTransform(H);

		// Hand alignment (from the clip's hand pose) and the wrist -> grip point offset after it.
		FQuat Align = FQuat::Identity;
		FVector GripOffset = FVector::ZeroVector;
		FVector Palm = FVector::ZeroVector;
		const FCompactPoseBoneIndex M = FindBone(Bones, Ref, LFGrip::Bone(TEXT("middle_01"), bRight));
		const FCompactPoseBoneIndex I = FindBone(Bones, Ref, LFGrip::Bone(TEXT("index_01"), bRight));
		const FCompactPoseBoneIndex P = FindBone(Bones, Ref, LFGrip::Bone(TEXT("pinky_01"), bRight));
		FVector F, S;
		if (M != INDEX_NONE && I != INDEX_NONE && P != INDEX_NONE)
		{
			const FVector Mp = CS.GetComponentSpaceTransform(M).GetLocation();
			if (LFGrip::HandFrame(HT.GetLocation(), Mp, CS.GetComponentSpaceTransform(I).GetLocation(),
				CS.GetComponentSpaceTransform(P).GetLocation(), bRight, F, S, Palm))
			{
				const FVector A = Target.GetUnitAxis(EAxis::X);
				const FVector Sd = G.bThumbTowardBlade ? A : -A;
				FVector Fd = F - A * FVector::DotProduct(F, A);
				// Palm grip (director): right palm down on top of the shaft, left palm up under it. Fingers follow from the palm
				// and the thumb side (Palm = F x S for the right hand, S x F for the left).
				FVector PalmD = FVector::UpVector * G.PalmVertical;
				PalmD -= A * FVector::DotProduct(PalmD, A);
				if (G.PalmVertical != 0.f && PalmD.Normalize())
				{
					Fd = bRight ? FVector::CrossProduct(Sd, PalmD) : FVector::CrossProduct(PalmD, Sd);
				}
				if (Fd.Normalize())
				{
					Align = FRotationMatrix::MakeFromXY(Fd, Sd).ToQuat() * FRotationMatrix::MakeFromXY(F, S).ToQuat().Inverse();
				}
				GripOffset = Align.RotateVector(LFGrip::GripPoint(HT.GetLocation(), Mp, Palm, G.Along, G.Depth) - HT.GetLocation());
				Palm = Align.RotateVector(Palm);
			}
		}
		FVector Effector = Target.GetLocation() - GripOffset;

		// Out of reach (e.g. the wind-up swings the blade back over the right shoulder): slide the hand up the shaft toward
		// the other grip to the farthest point the arm reaches, keeping a hand's width between the hands.
		if (G.SlideToward)
		{
			const float ArmLen = float(FVector::Dist(UT.GetLocation(), LT.GetLocation()) + FVector::Dist(LT.GetLocation(), HT.GetLocation())) * 0.98f;
			const FVector Anchor = *G.SlideToward;
			const FVector Along = Target.GetLocation() - Anchor;
			const float TOrig = float(Along.Size());
			if (TOrig > G.MinHandGap && FVector::Dist(UT.GetLocation(), Effector) > ArmLen)
			{
				const FVector Dir = Along / TOrig;
				// |P0 + t Dir| = ArmLen, P0 = anchor-side wrist point relative to the shoulder.
				const FVector P0 = Anchor - GripOffset - UT.GetLocation();
				const float B = float(FVector::DotProduct(P0, Dir));
				const float Disc = B * B - float(P0.SizeSquared()) + ArmLen * ArmLen;
				// No point of the shaft is in reach (the slash clip's wind-up turns the left shoulder away): go for the
				// closest point of the shaft instead of the far socket.
				const float T = Disc >= 0.f ? -B + FMath::Sqrt(Disc) : -B;
				Effector = Anchor + Dir * FMath::Clamp(T, G.MinHandGap, TOrig) - GripOffset;
			}
			if (G.OutShortfall)
			{
				*G.OutShortfall = FMath::Max(0.f, float(FVector::Dist(UT.GetLocation(), Effector)) - ArmLen) / FMath::Max(ArmLen, 1.f);
			}
		}
		if (G.Weight <= KINDA_SMALL_NUMBER)
		{
			return; // let go: the arm keeps the clip pose
		}
		const FQuat HandRot = Align * HT.GetRotation();

		// Pole = the current elbow position (keeps the animated bend plane), unless an explicit pole is given (rowing).
		FVector OutJoint, OutEnd;
		AnimationCore::SolveTwoBoneIK(UT.GetLocation(), LT.GetLocation(), HT.GetLocation(), G.bHasPole ? G.Pole : LT.GetLocation(), Effector,
			OutJoint, OutEnd, /*bAllowStretching*/ false, 1.f, 1.f);

		// Upper arm: rotate so the elbow lands on OutJoint; children inherit that rotation.
		const FQuat DU = FQuat::FindBetweenNormals((LT.GetLocation() - UT.GetLocation()).GetSafeNormal(), (OutJoint - UT.GetLocation()).GetSafeNormal());
		const FVector L1 = UT.GetLocation() + DU.RotateVector(LT.GetLocation() - UT.GetLocation());
		const FVector H1 = UT.GetLocation() + DU.RotateVector(HT.GetLocation() - UT.GetLocation());
		UT.SetRotation(DU * UT.GetRotation());
		// Forearm: rotate so the hand lands on OutEnd.
		const FQuat DL = FQuat::FindBetweenNormals((H1 - L1).GetSafeNormal(), (OutEnd - OutJoint).GetSafeNormal());
		LT.SetLocation(OutJoint);
		LT.SetRotation(DL * DU * LT.GetRotation());
		HT.SetLocation(OutEnd);
		HT.SetRotation(HandRot);

		TArray<FBoneTransform> Out;
		Out.Add(FBoneTransform(U, UT));
		Out.Add(FBoneTransform(L, LT));
		Out.Add(FBoneTransform(H, HT));
		CS.LocalBlendCSBoneTransforms(Out, G.Weight);

		if (!Palm.IsNearlyZero())
		{
			FGripParams Curl = G;
			Curl.Curl *= G.Weight;
			Curl.ThumbCurl *= G.Weight;
			CurlFingers(CS, Bones, Ref, bRight, Palm, Curl);
		}
	}
}

namespace
{
	/**
	 * Seated clips rest the hands ON the thighs, but on Manny they sink in. Keep each palm SitThighRadius + clearance away from
	 * both thigh bone lines (thigh -> calf): two-bone IK the wrist out along the push, hand rotation kept.
	 */
	void PushHandsOffThighs(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref, float Radius)
	{
		FVector ThighA[2], ThighB[2];
		for (int32 Leg = 0; Leg < 2; ++Leg)
		{
			const FCompactPoseBoneIndex T = FindBone(Bones, Ref, LFGrip::Bone(TEXT("thigh"), Leg == 0));
			const FCompactPoseBoneIndex C = FindBone(Bones, Ref, LFGrip::Bone(TEXT("calf"), Leg == 0));
			if (T == INDEX_NONE || C == INDEX_NONE)
			{
				return;
			}
			ThighA[Leg] = CS.GetComponentSpaceTransform(T).GetLocation();
			ThighB[Leg] = CS.GetComponentSpaceTransform(C).GetLocation();
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const bool bRight = Side == 0;
			const FCompactPoseBoneIndex U = FindBone(Bones, Ref, LFGrip::Bone(TEXT("upperarm"), bRight));
			const FCompactPoseBoneIndex L = FindBone(Bones, Ref, LFGrip::Bone(TEXT("lowerarm"), bRight));
			const FCompactPoseBoneIndex H = FindBone(Bones, Ref, LFGrip::Bone(TEXT("hand"), bRight));
			const FCompactPoseBoneIndex M = FindBone(Bones, Ref, LFGrip::Bone(TEXT("middle_01"), bRight));
			if (U == INDEX_NONE || L == INDEX_NONE || H == INDEX_NONE || M == INDEX_NONE)
			{
				continue;
			}
			FTransform UT = CS.GetComponentSpaceTransform(U);
			FTransform LT = CS.GetComponentSpaceTransform(L);
			FTransform HT = CS.GetComponentSpaceTransform(H);
			const FVector Palm = (HT.GetLocation() + CS.GetComponentSpaceTransform(M).GetLocation()) * 0.5f;
			FVector Push = FVector::ZeroVector;
			for (int32 Leg = 0; Leg < 2; ++Leg)
			{
				const FVector P = Palm + Push;
				const FVector Closest = FMath::ClosestPointOnSegment(P, ThighA[Leg], ThighB[Leg]);
				const FVector Out = P - Closest;
				const float D = float(Out.Size());
				if (D < Radius)
				{
					// Out of the thigh along the closest-point normal (straight up if the palm sits on the bone line).
					const FVector N = D > KINDA_SMALL_NUMBER ? Out / D : FVector::UpVector;
					Push += N * (Radius - D);
				}
			}
			if (Push.IsNearlyZero(0.1))
			{
				continue;
			}
			FVector OutJoint, OutEnd;
			AnimationCore::SolveTwoBoneIK(UT.GetLocation(), LT.GetLocation(), HT.GetLocation(), LT.GetLocation(), HT.GetLocation() + Push,
				OutJoint, OutEnd, /*bAllowStretching*/ false, 1.f, 1.f);
			const FQuat HandRot = HT.GetRotation();
			const FQuat DU = FQuat::FindBetweenNormals((LT.GetLocation() - UT.GetLocation()).GetSafeNormal(), (OutJoint - UT.GetLocation()).GetSafeNormal());
			const FVector L1 = UT.GetLocation() + DU.RotateVector(LT.GetLocation() - UT.GetLocation());
			const FVector H1 = UT.GetLocation() + DU.RotateVector(HT.GetLocation() - UT.GetLocation());
			UT.SetRotation(DU * UT.GetRotation());
			const FQuat DL = FQuat::FindBetweenNormals((H1 - L1).GetSafeNormal(), (OutEnd - OutJoint).GetSafeNormal());
			LT.SetLocation(OutJoint);
			LT.SetRotation(DL * DU * LT.GetRotation());
			HT.SetLocation(OutEnd);
			HT.SetRotation(HandRot);
			TArray<FBoneTransform> Tr;
			Tr.Add(FBoneTransform(U, UT));
			Tr.Add(FBoneTransform(L, LT));
			Tr.Add(FBoneTransform(H, HT));
			CS.LocalBlendCSBoneTransforms(Tr, 1.f);
		}
	}
}

namespace
{
	/** Two-bone IK one arm's wrist toward Goal (component space), hand rotation kept, blended by Weight. */
	void MoveWristTo(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref, bool bRight,
		const FVector& Goal, float Weight)
	{
		const FCompactPoseBoneIndex U = FindBone(Bones, Ref, LFGrip::Bone(TEXT("upperarm"), bRight));
		const FCompactPoseBoneIndex L = FindBone(Bones, Ref, LFGrip::Bone(TEXT("lowerarm"), bRight));
		const FCompactPoseBoneIndex H = FindBone(Bones, Ref, LFGrip::Bone(TEXT("hand"), bRight));
		if (U == INDEX_NONE || L == INDEX_NONE || H == INDEX_NONE || Weight <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		FTransform UT = CS.GetComponentSpaceTransform(U);
		FTransform LT = CS.GetComponentSpaceTransform(L);
		FTransform HT = CS.GetComponentSpaceTransform(H);
		FVector OutJoint, OutEnd;
		AnimationCore::SolveTwoBoneIK(UT.GetLocation(), LT.GetLocation(), HT.GetLocation(), LT.GetLocation(), Goal,
			OutJoint, OutEnd, /*bAllowStretching*/ false, 1.f, 1.f);
		const FQuat HandRot = HT.GetRotation();
		const FQuat DU = FQuat::FindBetweenNormals((LT.GetLocation() - UT.GetLocation()).GetSafeNormal(), (OutJoint - UT.GetLocation()).GetSafeNormal());
		const FVector L1 = UT.GetLocation() + DU.RotateVector(LT.GetLocation() - UT.GetLocation());
		const FVector H1 = UT.GetLocation() + DU.RotateVector(HT.GetLocation() - UT.GetLocation());
		UT.SetRotation(DU * UT.GetRotation());
		const FQuat DL = FQuat::FindBetweenNormals((H1 - L1).GetSafeNormal(), (OutEnd - OutJoint).GetSafeNormal());
		LT.SetLocation(OutJoint);
		LT.SetRotation(DL * DU * LT.GetRotation());
		HT.SetLocation(OutEnd);
		HT.SetRotation(HandRot);
		TArray<FBoneTransform> Tr;
		Tr.Add(FBoneTransform(U, UT));
		Tr.Add(FBoneTransform(L, LT));
		Tr.Add(FBoneTransform(H, HT));
		CS.LocalBlendCSBoneTransforms(Tr, Weight);
	}
}

void FLFBrawlerAnimProxy::ApplySwingTorso(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref) const
{
	const FCompactPoseBoneIndex Pelvis = FindBone(Bones, Ref, TEXT("pelvis"));
	const FCompactPoseBoneIndex ThighR = FindBone(Bones, Ref, TEXT("thigh_r"));
	const FCompactPoseBoneIndex ThighL = FindBone(Bones, Ref, TEXT("thigh_l"));
	if (Pelvis == INDEX_NONE || ThighR == INDEX_NONE || ThighL == INDEX_NONE)
	{
		return;
	}
	// Body axes in mesh space: up = +Z, right from the hips, forward = right x up.
	const FVector Up = FVector::UpVector;
	FVector Right = CS.GetComponentSpaceTransform(ThighR).GetLocation() - CS.GetComponentSpaceTransform(ThighL).GetLocation();
	Right = (Right - Up * FVector::DotProduct(Right, Up)).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		return;
	}
	const FVector Forward = FVector::CrossProduct(Right, Up);
	if (!FMath::IsNearlyZero(PelvisForward))
	{
		FTransform P = CS.GetComponentSpaceTransform(Pelvis);
		P.AddToTranslation(Forward * (PelvisForward / FMath::Max(MeshScale, 0.01f)));
		SetCS(CS, Pelvis, P);
	}
	// + yaw about Up turns Forward toward Right; + pitch about Right tips Up toward Forward.
	static const TCHAR* Spine[] = { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") };
	for (int32 I = 0; I < 3; ++I)
	{
		const FCompactPoseBoneIndex Idx = FindBone(Bones, Ref, Spine[I]);
		if (Idx == INDEX_NONE)
		{
			continue;
		}
		const float W = float(SpineSplit[I]);
		const FQuat Q = FQuat(Up, FMath::DegreesToRadians(TorsoYaw * W)) * FQuat(Right, FMath::DegreesToRadians(TorsoPitch * W));
		FTransform T = CS.GetComponentSpaceTransform(Idx);
		T.SetRotation(Q * T.GetRotation());
		SetCS(CS, Idx, T);
	}
}

void FLFBrawlerAnimProxy::LogGrip(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref)
{
	// Measured on the solved pose itself (exact; a game-thread check would mix frames during a fast swing).
	const bool bSwing = Clip == ELFBrawlerClip::Swing;
	if (IKLogClock < (bSwing ? 0.1f : 2.f))
	{
		return;
	}
	IKLogClock = 0.f;
	float Err[2] = { -1.f, -1.f };
	float Off[2] = { -1.f, -1.f };
	float Across[2] = { 0.f, 0.f }; // wrists across the body: -1 = at the left shoulder line, 0 = centre, +1 = at the right shoulder line
	{
		const FCompactPoseBoneIndex ShR = FindBone(Bones, Ref, TEXT("upperarm_r"));
		const FCompactPoseBoneIndex ShL = FindBone(Bones, Ref, TEXT("upperarm_l"));
		const FCompactPoseBoneIndex HR = FindBone(Bones, Ref, TEXT("hand_r"));
		const FCompactPoseBoneIndex HL = FindBone(Bones, Ref, TEXT("hand_l"));
		if (ShR != INDEX_NONE && ShL != INDEX_NONE && HR != INDEX_NONE && HL != INDEX_NONE)
		{
			const FVector R = CS.GetComponentSpaceTransform(ShR).GetLocation();
			const FVector L = CS.GetComponentSpaceTransform(ShL).GetLocation();
			FVector Dir = R - L;
			Dir.Z = 0.f;
			const float Half = float(Dir.Size()) * 0.5f;
			if (Dir.Normalize() && Half > 1.f)
			{
				Across[0] = float(FVector::DotProduct(CS.GetComponentSpaceTransform(HR).GetLocation() - (R + L) * 0.5f, Dir)) / Half;
				Across[1] = float(FVector::DotProduct(CS.GetComponentSpaceTransform(HL).GetLocation() - (R + L) * 0.5f, Dir)) / Half;
			}
		}
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bRight = Side == 0;
		auto Pos = [&](const TCHAR* Base)
		{
			const FCompactPoseBoneIndex I = FindBone(Bones, Ref, LFGrip::Bone(Base, bRight));
			return I == INDEX_NONE ? FVector::ZeroVector : CS.GetComponentSpaceTransform(I).GetLocation();
		};
		const FVector H = Pos(TEXT("hand"));
		const FVector M = Pos(TEXT("middle_01"));
		FVector F, S, Palm;
		if (!LFGrip::HandFrame(H, M, Pos(TEXT("index_01")), Pos(TEXT("pinky_01")), bRight, F, S, Palm))
		{
			continue;
		}
		const FTransform& T = bRight ? RightTarget : LeftTarget;
		const FVector G = LFGrip::GripPoint(H, M, Palm, GripAlongHand, GripPalmDepth);
		Err[Side] = float(FVector::Dist(G, T.GetLocation())) * MeshScale;
		Off[Side] = float(FMath::PointDistToLine(G, T.GetUnitAxis(EAxis::X), T.GetLocation())) * MeshScale;
	}
	UE_LOG(LogLF, Log, TEXT("[ik] %s %s grip: palm -> socket R %.1f L %.1f cm, off the shaft R %.1f L %.1f cm, left hand grip %.2f, wrists across the body R %.2f L %.2f (-1 = left shoulder line, +1 = right)"), *OwnerName,
		bSwing ? *FString::Printf(TEXT("swing %.2f"), Scrub) : TEXT("hold"), Err[0], Err[1], Off[0], Off[1], LeftGripWeight, Across[0], Across[1]);
}

bool FLFBrawlerAnimProxy::Evaluate(FPoseContext& Output)
{
	if (!Idle)
	{
		return false;
	}
	FAnimationPoseData OutData(Output);
	if (Fade < 1.f && PrevClip != Clip)
	{
		FCompactPose PA, PB;
		FBlendedCurve CA, CB;
		UE::Anim::FStackAttributeContainer AA, AB;
		PA.SetBoneContainer(&Output.Pose.GetBoneContainer());
		PB.SetBoneContainer(&Output.Pose.GetBoneContainer());
		CA.InitFrom(Output.Curve);
		CB.InitFrom(Output.Curve);
		FAnimationPoseData DA(PA, CA, AA);
		FAnimationPoseData DB(PB, CB, AB);
		EvalClip(PrevClip, PrevScrub, DA);
		EvalClip(Clip, Scrub, DB);
		FAnimationRuntime::BlendTwoPosesTogether(DA, DB, 1.f - Fade, OutData);
	}
	else
	{
		EvalClip(Clip, Scrub, OutData);
	}

	// Walk: (1) keep it IN PLACE - Wheelbarrow_Walk is a Mixamo clip with forward travel baked into the hips, which made the body
	// run ahead of the capsule, snap back at the loop and swing in a circle when turning (director, playtest); the root and the
	// pelvis keep the idle pose's horizontal position, only the vertical bob stays. (2) Optionally damp the pelvis sway.
	if (Clip == ELFBrawlerClip::Locomotion && Walk && Idle && (bWalkInPlace || WalkHipDamp > 0.f))
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
		const FCompactPoseBoneIndex Pelvis = FindBone(Bones, Ref, TEXT("pelvis"));
		if (Pelvis != INDEX_NONE)
		{
			FCompactPose IdlePose;
			FBlendedCurve IdleCurve;
			UE::Anim::FStackAttributeContainer IdleAttr;
			IdlePose.SetBoneContainer(&Bones);
			IdleCurve.InitFrom(Output.Curve);
			FAnimationPoseData IdleData(IdlePose, IdleCurve, IdleAttr);
			SamplePose(Idle, LoopTime, true, IdleData);
			if (bWalkInPlace)
			{
				const FCompactPoseBoneIndex Root = FindBone(Bones, Ref, TEXT("root"));
				for (const FCompactPoseBoneIndex B : { Root, Pelvis })
				{
					if (B != INDEX_NONE)
					{
						const FVector T = Output.Pose[B].GetTranslation();
						const FVector I = IdlePose[B].GetTranslation();
						Output.Pose[B].SetTranslation(FVector(I.X, I.Y, T.Z));
					}
				}
			}
			const float W = WalkHipDamp * FMath::Clamp(Speed / (WalkFullSpeed * FMath::Max(MeshScale, 0.1f)), 0.f, 1.f);
			const FQuat Old = Output.Pose[Pelvis].GetRotation();
			const FQuat New = FQuat::Slerp(Old, IdlePose[Pelvis].GetRotation(), W).GetNormalized();
			Output.Pose[Pelvis].SetRotation(New);
			const FQuat Comp = New.Inverse() * Old; // child local' = Comp * child local keeps the child's component-space rotation
			for (const TCHAR* Child : { TEXT("thigh_l"), TEXT("thigh_r"), TEXT("spine_01") })
			{
				const FCompactPoseBoneIndex C = FindBone(Bones, Ref, Child);
				if (C != INDEX_NONE)
				{
					Output.Pose[C].SetRotation((Comp * Output.Pose[C].GetRotation()).GetNormalized());
					Output.Pose[C].SetTranslation(Comp.RotateVector(Output.Pose[C].GetTranslation()));
				}
			}
		}
	}

	// Swing torso (before the arm IK, so the arms reach the paddle from the twisted chest).
	if (!FMath::IsNearlyZero(TorsoYaw, 0.05f) || !FMath::IsNearlyZero(TorsoPitch, 0.05f) || !FMath::IsNearlyZero(PelvisForward, 0.05f))
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		FCSPose<FCompactPose> CS;
		CS.InitPose(Output.Pose);
		ApplySwingTorso(CS, Bones, Bones.GetReferenceSkeleton());
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	}

	// Seated: hands rest on the thighs, not inside them.
	if (Clip == ELFBrawlerClip::Sit || Clip == ELFBrawlerClip::Row)
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		FCSPose<FCompactPose> CS;
		CS.InitPose(Output.Pose);
		PushHandsOffThighs(CS, Bones, Bones.GetReferenceSkeleton(), SitThighRadius + SitHandClearance);
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	}

	// Arms follow the paddle.
	if (bArmIK)
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
		FCSPose<FCompactPose> CS;
		CS.InitPose(Output.Pose);
		FGripParams G;
		G.Along = GripAlongHand;
		G.Depth = GripPalmDepth;
		G.Curl = FingerCurlDeg;
		G.ThumbCurl = ThumbCurlDeg;
		if (bRowIK)
		{
			// Oar handles: overhand, thumbs inboard (away from the oarlock), each hand on its own handle (no slide / release).
			G.bThumbTowardBlade = false;
			G.PalmVertical = -1.f;
			// Elbows flare out to the sides (director: they converged behind the back following Sitting1's tucked elbows).
			const FCompactPoseBoneIndex ShR = FindBone(Bones, Ref, TEXT("upperarm_r"));
			const FCompactPoseBoneIndex ShL = FindBone(Bones, Ref, TEXT("upperarm_l"));
			FVector PoleR = FVector::ZeroVector, PoleL = FVector::ZeroVector;
			bool bPoles = false;
			if (ShR != INDEX_NONE && ShL != INDEX_NONE)
			{
				const FVector PR = CS.GetComponentSpaceTransform(ShR).GetLocation();
				const FVector PL = CS.GetComponentSpaceTransform(ShL).GetLocation();
				FVector Rt = PR - PL;
				Rt.Z = 0.f;
				if (Rt.Normalize())
				{
					const FVector Fw = FVector::CrossProduct(Rt, FVector::UpVector);
					const FVector Off = -Fw * RowElbowBack - FVector::UpVector * RowElbowDown;
					PoleR = PR + Rt * RowElbowOut + Off;
					PoleL = PL - Rt * RowElbowOut + Off;
					bPoles = true;
				}
			}
			G.bHasPole = bPoles;
			G.Pole = PoleR;
			SolveHand(CS, Bones, Ref, true, RightTarget, G);
			G.Pole = PoleL;
			SolveHand(CS, Bones, Ref, false, LeftTarget, G);
			LeftGripWeight = 1.f;
			if (bDebugIK)
			{
				LogGrip(CS, Bones, Ref);
			}
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
			return true;
		}
		G.bThumbTowardBlade = bRightThumbTowardBlade;
		G.PalmVertical = bRightOverhand ? -1.f : 0.f;
		SolveHand(CS, Bones, Ref, true, RightTarget, G);
		G.bThumbTowardBlade = bLeftThumbTowardBlade;
		G.PalmVertical = bLeftUnderhand ? 1.f : 0.f;
		const FVector RightGrip = RightTarget.GetLocation();
		G.SlideToward = &RightGrip;
		float Shortfall = 0.f;
		G.Weight = LeftGripWeight;
		G.OutShortfall = &Shortfall;
		SolveHand(CS, Bones, Ref, false, LeftTarget, G);
		// Charged punch (director): past the charge threshold the left hand lets go, the right holds the paddle in the charge
		// pose and fires; the left re-grips in the recover. Light punches keep both hands on (the left only lets go if no point
		// of the shaft is in its reach).
		const float WantW = (bChargedOneHand || Shortfall > LeftReleaseShortfall) ? 0.f : 1.f;
		// The free left hand rests beside the left hip, a little forward and out (a balance arm) - never wherever the clip and
		// the torso twist happen to put it (that left it across the chest in the charge hold).
		if (LeftGripWeight < 1.f)
		{
			const FCompactPoseBoneIndex ThighL = FindBone(Bones, Ref, TEXT("thigh_l"));
			const FCompactPoseBoneIndex ShR = FindBone(Bones, Ref, TEXT("upperarm_r"));
			const FCompactPoseBoneIndex ShL = FindBone(Bones, Ref, TEXT("upperarm_l"));
			if (ThighL != INDEX_NONE && ShR != INDEX_NONE && ShL != INDEX_NONE)
			{
				FVector BodyRight = CS.GetComponentSpaceTransform(ShR).GetLocation() - CS.GetComponentSpaceTransform(ShL).GetLocation();
				BodyRight.Z = 0.f;
				if (BodyRight.Normalize())
				{
					const FVector Fwd = FVector::CrossProduct(BodyRight, FVector::UpVector);
					const FVector Rest = CS.GetComponentSpaceTransform(ThighL).GetLocation()
						- BodyRight * FreeHandOut + Fwd * FreeHandForward + FVector::UpVector * FreeHandUp;
					MoveWristTo(CS, Bones, Ref, false, Rest, 1.f - LeftGripWeight);
				}
			}
		}
		LeftGripWeight = FMath::FInterpConstantTo(LeftGripWeight, WantW, LastDelta, 1.f / FMath::Max(LeftRegripTime, 0.01f));
		if (bDebugIK)
		{
			LogGrip(CS, Bones, Ref);
		}
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	}
	return true;
}
