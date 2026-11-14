#include "World/LFShoreCrowd.h"

#include "Boat/LFBoat.h"
#include "Brawler/LFBrawler.h" // LFBoat.h inline SetDriver needs the complete type
#include "Fish/LFFishSubsystem.h"
#include "Water/LFWater.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
// lf.crowd.look <i> [delay s]: view spectator i from a temporary camera in front of them (evidence shots).
static FAutoConsoleCommandWithWorldAndArgs CmdLFCrowdLook(TEXT("lf.crowd.look"), TEXT("lf.crowd.look <index> [delay s] - look at a shore spectator."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 I = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
		TWeakObjectPtr<UWorld> W(World);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([W, I](float)
		{
			if (!W.IsValid()) return false;
			for (TActorIterator<ALFShoreCrowd> It(W.Get()); It; ++It)
			{
				FVector At, Facing;
				if (!It->GetFan(I, At, Facing)) return false;
				const FVector Eye = At + Facing * 600.f + FVector(0, 0, 220.f);
				ACameraActor* Cam = W->SpawnActor<ACameraActor>(Eye, (At + FVector(0, 0, 90.f) - Eye).Rotation());
				if (APlayerController* PC = W->GetFirstPlayerController()) PC->SetViewTarget(Cam);
			}
			return false;
		}), FMath::Max(Delay, 0.01f));
	}));
#endif

ALFShoreCrowd::ALFShoreCrowd()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorEnableCollision(false);
	// Constructor references so the cook includes the clips (the Mixamo cheer clips are not used anywhere else).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> M(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> B(TEXT("/Game/LakeFight/Materials/MI_LF_Char_Body.MI_LF_Char_Body"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> J(TEXT("/Game/LakeFight/Materials/MI_LF_Char_Joints.MI_LF_Char_Joints"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> I(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> V(TEXT("/Game/LakeFight/Anim/Manny/Victory.Victory"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> N(TEXT("/Game/LakeFight/Anim/Manny/Insult.Insult"));
	Manny = M.Object;
	BodyMat = B.Object;
	JointMat = J.Object;
	IdleAnim = I.Object;
	VictoryAnim = V.Object;
	InsultAnim = N.Object;
}

void ALFShoreCrowd::BeginPlay()
{
	Super::BeginPlay();
}

void ALFShoreCrowd::Setup()
{
	bSetup = true;
	const ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
	if (!Fish || !Manny) return;
	for (int32 Team = 0; Team < 4; ++Team)
	{
		FVector Bank;
		if (!Fish->GetBankLocation(ELFTeam(Team), Bank)) continue;
		const FVector Out = (Bank * FVector(1, 1, 0)).GetSafeNormal(); // centre -> dock
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Out);
		for (int32 K = 0; K < 2; ++K)
		{
			// Beside the dock (not on it), ~8-10 m behind the bank zone on land; ground found by a trace from above.
			const FVector Try = Bank + Out * (850.f + 150.f * K) + Side * (K == 0 ? 420.f : -460.f);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(LFCrowdGround), false);
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Try + FVector(0, 0, 2000.f), Try - FVector(0, 0, 500.f), ECC_WorldStatic, Q)
				|| Hit.ImpactPoint.Z < LF::Water::BaseHeight + 10.f)
			{
				continue; // water or nothing there - no spectator rather than a floating one
			}
			USkeletalMeshComponent* S = NewObject<USkeletalMeshComponent>(this);
			S->SetSkeletalMesh(Manny);
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			S->SetCanEverAffectNavigation(false);
			S->SetupAttachment(RootComponent);
			S->RegisterComponent();
			AddInstanceComponent(S);
			if (BodyMat) S->SetMaterial(0, BodyMat);
			if (JointMat) S->SetMaterial(1, JointMat);
			S->SetWorldScale3D(FVector(0.85f));
			// Manny's mesh faces +Y: yaw so it looks back over the lake (toward the centre).
			const float FaceYaw = (-Out).Rotation().Yaw - 90.f + (K == 0 ? -12.f : 12.f);
			S->SetWorldLocationAndRotation(Hit.ImpactPoint, FRotator(0.f, FaceYaw, 0.f));
			S->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			FFan F;
			F.Mesh = S;
			F.Team = Team;
			Fans.Add(F);
			Play(Fans.Last(), IdleAnim, 0.f, 0.f);
			S->SetPosition(FMath::FRandRange(0.f, 2.f)); // out of step
		}
		LastScores[Team] = Fish->GetScore(Team);
	}
	UE_LOG(LogLF, Display, TEXT("[crowd] %d spectators on the shore"), Fans.Num());
}

void ALFShoreCrowd::Play(FFan& Fan, UAnimSequence* Anim, float Now, float Hold)
{
	if (!Fan.Mesh || !Anim) return;
	Fan.Mesh->PlayAnimation(Anim, true);
	Fan.BusyUntil = Now + Hold;
}

void ALFShoreCrowd::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSetup) Setup(); // the fish subsystem's bank list is ready by the first tick
	const float Now = GetWorld()->GetTimeSeconds();
	const ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
	// Banks: the team's fans celebrate.
	for (int32 Team = 0; Fish && Team < 4; ++Team)
	{
		const int32 Score = Fish->GetScore(Team);
		if (Score > LastScores[Team])
		{
			for (FFan& F : Fans) if (F.Team == Team) Play(F, VictoryAnim, Now, 3.5f);
		}
		LastScores[Team] = Score;
	}
	// Rams: two boats closing fast within 4 m - fans within 35 m react (own team's boat hit -> insult, else cheer).
	TArray<ALFBoat*, TInlineAllocator<8>> Boats;
	for (TActorIterator<ALFBoat> It(GetWorld()); It; ++It) Boats.Add(*It);
	for (int32 A = 0; A < Boats.Num(); ++A)
	{
		for (int32 B = A + 1; B < Boats.Num(); ++B)
		{
			const FVector D = Boats[B]->GetActorLocation() - Boats[A]->GetActorLocation();
			if (D.SizeSquared2D() > 400.f * 400.f) continue;
			const float Closing = float(FVector::DotProduct(Boats[A]->GetVelocity() - Boats[B]->GetVelocity(), D.GetSafeNormal2D()));
			if (Closing < 300.f) continue;
			const FVector At = (Boats[A]->GetActorLocation() + Boats[B]->GetActorLocation()) * 0.5f;
			for (FFan& F : Fans)
			{
				if (Now < F.BusyUntil || FVector::DistSquared2D(F.Mesh->GetComponentLocation(), At) > 3500.f * 3500.f) continue;
				Play(F, FMath::RandBool() ? VictoryAnim : InsultAnim, Now, 3.f); // (the Mixamo cheer clip is a sitting pose)
			}
		}
	}
	// Back to idle when a reaction is over.
	for (FFan& F : Fans)
	{
		if (F.BusyUntil > 0.f && Now >= F.BusyUntil)
		{
			F.BusyUntil = 0.f;
			Play(F, IdleAnim, Now, 0.f);
		}
	}
}

bool ALFShoreCrowd::GetFan(int32 I, FVector& At, FVector& Facing) const
{
	if (!Fans.IsValidIndex(I) || !Fans[I].Mesh) return false;
	At = Fans[I].Mesh->GetComponentLocation();
	Facing = FRotator(0.f, Fans[I].Mesh->GetComponentRotation().Yaw + 90.f, 0.f).Vector(); // mesh faces +Y
	return true;
}
