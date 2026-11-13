#include "Brawler/LFBrawlerAnimInstance.h"

#include "Brawler/LFBrawler.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ULFBrawlerAnimInstance::ULFBrawlerAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleSeq(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkSeq(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
	// Retargeted Mixamo clips (director, Content/LakeFight/Anim/Manny).
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RowSeq(TEXT("/Game/LakeFight/Anim/Manny/Sitting_Rowing.Sitting_Rowing"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> TreadSeq(TEXT("/Game/LakeFight/Anim/Manny/Treading_Water.Treading_Water"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SwimSeq(TEXT("/Game/LakeFight/Anim/Manny/Swimming.Swimming"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ClimbSeq(TEXT("/Game/LakeFight/Anim/Manny/Swimming_To_Edge.Swimming_To_Edge"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DizzySeq(TEXT("/Game/LakeFight/Anim/Manny/Dizzy_Idle.Dizzy_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SlashSeq(TEXT("/Game/LakeFight/Anim/Manny/Great_Sword_Slash.Great_Sword_Slash"));
	IdleAnim = IdleSeq.Object;
	WalkAnim = WalkSeq.Object;
	RowAnim = RowSeq.Object;
	TreadAnim = TreadSeq.Object;
	SwimAnim = SwimSeq.Object;
	ClimbAnim = ClimbSeq.Object;
	DizzyAnim = DizzySeq.Object;
	SlashAnim = SlashSeq.Object;
}

void ULFBrawlerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
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
	case ELFBrawlerState::Driving:  Clip = ELFBrawlerClip::Row; break;
	case ELFBrawlerState::Swimming: Clip = ELFBrawlerClip::Water; break;
	case ELFBrawlerState::Climbing: Clip = ELFBrawlerClip::Climb; Scrub = V.ClimbAlpha; break;
	case ELFBrawlerState::Standing:
		if (V.SwingPhase != 0)
		{
			Clip = ELFBrawlerClip::Slash;
			// Charging: hold the wind-up. Swinging: play the strike. Recover: play the rest.
			Scrub = V.SwingPhase == 1 ? SlashWindupFrac
				: (V.SwingPhase == 2 ? FMath::Lerp(SlashWindupFrac, SlashStrikeEndFrac, V.SwingAlpha)
				: FMath::Lerp(SlashStrikeEndFrac, 1.f, V.SwingAlpha));
		}
		else
		{
			Clip = V.bStunned ? ELFBrawlerClip::Dizzy : ELFBrawlerClip::Locomotion;
		}
		break;
	default: Clip = ELFBrawlerClip::Locomotion; break;
	}
	FVector R, L;
	bArmIK = B->GetPaddleGripTargets(R, L);
	if (bArmIK)
	{
		const FTransform& C = GetSkelMeshComponent()->GetComponentTransform();
		RightTargetCS = C.InverseTransformPosition(R);
		LeftTargetCS = C.InverseTransformPosition(L);
	}
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
	Idle = I->IdleAnim; Walk = I->WalkAnim; Row = I->RowAnim; Tread = I->TreadAnim; Swim = I->SwimAnim;
	ClimbSeq = I->ClimbAnim; Dizzy = I->DizzyAnim; Slash = I->SlashAnim;
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
}

void FLFBrawlerAnimProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	LoopTime += DeltaSeconds;
	const float Rate = FMath::Clamp(Speed / WalkFullSpeed, 0.4f, 1.5f);
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
		const float T = bLoop ? FMath::Fmod(Time, Len) : FMath::Clamp(Time, 0.f, Len);
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
		if (Valid(Row)) { SamplePose(Row, LoopTime, true, Out); return; }
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
	case ELFBrawlerClip::Slash:
		if (Valid(Slash)) { SamplePose(Slash, InScrub * Slash->GetPlayLength(), false, Out); return; }
		break;
	default:
		break;
	}
	// Locomotion (and fallback when a clip is missing): idle <-> walk by speed.
	const float WalkAlpha = Walk ? FMath::Clamp(Speed / WalkFullSpeed, 0.f, 1.f) : 0.f;
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
	void SolveArm(FCSPose<FCompactPose>& CS, const FBoneContainer& Bones, const FReferenceSkeleton& Ref,
		FName UpperName, FName LowerName, FName HandName, const FVector& Effector, TArray<FBoneTransform>& Out)
	{
		const int32 UpperRef = Ref.FindBoneIndex(UpperName);
		const int32 LowerRef = Ref.FindBoneIndex(LowerName);
		const int32 HandRef = Ref.FindBoneIndex(HandName);
		if (UpperRef == INDEX_NONE || LowerRef == INDEX_NONE || HandRef == INDEX_NONE)
		{
			return;
		}
		const FCompactPoseBoneIndex U = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(UpperRef));
		const FCompactPoseBoneIndex L = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(LowerRef));
		const FCompactPoseBoneIndex H = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandRef));
		if (U == INDEX_NONE || L == INDEX_NONE || H == INDEX_NONE)
		{
			return;
		}
		FTransform UT = CS.GetComponentSpaceTransform(U);
		FTransform LT = CS.GetComponentSpaceTransform(L);
		FTransform HT = CS.GetComponentSpaceTransform(H);

		// Pole = the current elbow position (keeps the animated bend plane).
		FVector OutJoint, OutEnd;
		AnimationCore::SolveTwoBoneIK(UT.GetLocation(), LT.GetLocation(), HT.GetLocation(), LT.GetLocation(), Effector,
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
		HT.SetRotation(DL * DU * HT.GetRotation());

		Out.Add(FBoneTransform(U, UT));
		Out.Add(FBoneTransform(L, LT));
		Out.Add(FBoneTransform(H, HT));
	}
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

	// Arms follow the paddle.
	if (bArmIK)
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
		FCSPose<FCompactPose> CS;
		CS.InitPose(Output.Pose);
		TArray<FBoneTransform> Tr;
		SolveArm(CS, Bones, Ref, TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), RightTarget, Tr);
		if (Tr.Num())
		{
			CS.LocalBlendCSBoneTransforms(Tr, 1.f);
		}
		TArray<FBoneTransform> TrL;
		SolveArm(CS, Bones, Ref, TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), LeftTarget, TrL);
		if (TrL.Num())
		{
			CS.LocalBlendCSBoneTransforms(TrL, 1.f);
		}
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
	}
	return true;
}
