#include "World/LFAmbientLife.h"

#include "Boat/LFBoat.h"
#include "Brawler/LFBrawler.h" // LFBoat.h inline SetDriver needs the complete type
#include "Water/LFWater.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"

#if !UE_BUILD_SHIPPING
// lf.ambient.look <i>: view critter i from a temporary camera (evidence shots). Floaters first, then gulls, then dragonflies.
static FAutoConsoleCommandWithWorldAndArgs CmdLFAmbientLook(TEXT("lf.ambient.look"), TEXT("lf.ambient.look <index> - look at an ambient critter."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 I = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
		if (Delay > 0.f)
		{
			TWeakObjectPtr<UWorld> W(World);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([W, I](float)
			{
				if (W.IsValid()) IConsoleManager::Get().ProcessUserConsoleInput(*FString::Printf(TEXT("lf.ambient.look %d"), I), *GLog, W.Get());
				return false;
			}), Delay);
			return;
		}
		for (TActorIterator<ALFAmbientLife> It(World); It; ++It)
		{
			FVector At;
			float Size;
			if (!It->GetCritter(I, At, Size)) return;
			const FVector Eye = At + FVector(-Size * 3.f, -Size * 2.f, Size * 1.5f);
			ACameraActor* Cam = World->SpawnActor<ACameraActor>(Eye, (At - Eye).Rotation());
			if (APlayerController* PC = World->GetFirstPlayerController()) PC->SetViewTarget(Cam);
			UE_LOG(LogLF, Display, TEXT("[ambient] look %d at %s"), I, *At.ToCompactString());
		}
	}));
#endif

namespace
{
	// All under /Game/LakeFight/Meshes (always cooked - DefaultGame.ini).
	UStaticMesh* AmbientMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/LakeFight/Meshes/%s.%s"), Name, Name));
	}
	constexpr float LakeMinR = 1800.f; // keep clear of the centre fish spot
	constexpr float LakeMaxR = 5600.f;
}

// ---------------------------------------------------------------- subsystem

bool ULFAmbientLifeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void ULFAmbientLifeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.GetMapName().Contains(TEXT("L_Lake01")) || FParse::Param(FCommandLine::Get(), TEXT("LFNoAmbient")))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InWorld.SpawnActor<ALFAmbientLife>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

// ---------------------------------------------------------------- actor

ALFAmbientLife::ALFAmbientLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorEnableCollision(false);
}

UStaticMeshComponent* ALFAmbientLife::AddPart(UStaticMesh* Mesh, float Scale)
{
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(Scale > 1.2f); // small critters: no shadow cost
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	AddInstanceComponent(C);
	C->SetWorldScale3D(FVector(Scale));
	return C;
}

bool ALFAmbientLife::OpenWater(const FVector& At) const
{
	// Same rule as the fish spots: nothing static between 1.5 m above the surface and just under it.
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LFAmbientWater), false);
	return !GetWorld()->LineTraceSingleByChannel(Hit, FVector(At.X, At.Y, LF::Water::BaseHeight + 150.f),
		FVector(At.X, At.Y, LF::Water::BaseHeight - 30.f), ECC_WorldStatic, Q);
}

bool ALFAmbientLife::PickWaterPoint(FRandomStream& Rng, float MinR, float MaxR, FVector& Out) const
{
	for (int32 Try = 0; Try < 40; ++Try)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = Rng.FRandRange(MinR, MaxR);
		Out = FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, LF::Water::BaseHeight);
		// Clear of the shore by ~2.5 m all round.
		if (OpenWater(Out) && OpenWater(Out + FVector(250, 0, 0)) && OpenWater(Out - FVector(250, 0, 0))
			&& OpenWater(Out + FVector(0, 250, 0)) && OpenWater(Out - FVector(0, 250, 0)))
		{
			return true;
		}
	}
	return false;
}

void ALFAmbientLife::BeginPlay()
{
	Super::BeginPlay();
	FRandomStream Rng(1511);
	UStaticMesh* Duck = AmbientMesh(TEXT("SM_LF_Duck"));
	UStaticMesh* Swan = AmbientMesh(TEXT("SM_LF_Swan"));
	// The swan FBX has a separate head/neck object; only use the swan if the import kept it (taller than the bare body).
	if (Swan && Swan->GetBounds().BoxExtent.Z * 2.f < 30.f) Swan = nullptr;
	UStaticMesh* JunkMeshes[] = { AmbientMesh(TEXT("SM_LF_Barrel")), AmbientMesh(TEXT("SM_LF_LifeRing")), AmbientMesh(TEXT("SM_LF_Bottle_Message")) };
	const float JunkScale[] = { 1.f, 1.f, 1.75f };

	auto AddFloater = [&](UStaticMesh* Mesh, float Scale, bool bBird, float Wander)
	{
		FVector At;
		if (!Mesh || !PickWaterPoint(Rng, LakeMinR, LakeMaxR, At)) return;
		FFloater F;
		F.Mesh = AddPart(Mesh, Scale);
		F.Home = At;
		F.Pos = At;
		F.Yaw = Rng.FRandRange(0.f, 360.f);
		F.Phase = Rng.FRandRange(0.f, 10.f);
		F.WanderRadius = Wander;
		F.bBird = bBird;
		Floaters.Add(F);
	};
	for (int32 I = 0; I < Ducks; ++I) AddFloater(Duck, 1.75f, true, 350.f);
	for (int32 I = 0; I < Swans; ++I) AddFloater(Swan, 1.f, true, 500.f);
	for (int32 I = 0; I < Junk; ++I) AddFloater(JunkMeshes[I % 3], JunkScale[I % 3], false, 150.f);

	// Gulls: body + two wings on their shoulder pivots (B11_ATTACHMENTS.json, Blender -Y = Unreal +Y after the FBX flip),
	// plus the glide-pose mesh swapped in while gliding.
	UStaticMesh* BirdBody = AmbientMesh(TEXT("SM_LF_Bird_Body"));
	UStaticMesh* WingL = AmbientMesh(TEXT("SM_LF_Bird_Wing_L"));
	UStaticMesh* WingR = AmbientMesh(TEXT("SM_LF_Bird_Wing_R"));
	UStaticMesh* Glide = AmbientMesh(TEXT("SM_LF_Bird_Glide"));
	for (int32 I = 0; BirdBody && WingL && WingR && Glide && I < Gulls; ++I)
	{
		FGull G;
		G.Body = AddPart(BirdBody, GullScale);
		G.WingL = AddPart(WingL, GullScale);
		G.WingR = AddPart(WingR, GullScale);
		G.Glide = AddPart(Glide, GullScale);
		G.WingL->AttachToComponent(G.Body, FAttachmentTransformRules::KeepRelativeTransform);
		G.WingR->AttachToComponent(G.Body, FAttachmentTransformRules::KeepRelativeTransform);
		G.WingL->SetRelativeLocation(FVector(0.f, 4.3f, 0.6f));
		G.WingR->SetRelativeLocation(FVector(0.f, -4.3f, 0.6f));
		G.WingL->SetRelativeScale3D(FVector(1.f));
		G.WingR->SetRelativeScale3D(FVector(1.f));
		G.Angle = 2.f * PI * I / FMath::Max(Gulls, 1) + Rng.FRandRange(-0.3f, 0.3f);
		G.Radius = Rng.FRandRange(2200.f, 3600.f);
		G.Height = GullHeight + Rng.FRandRange(-500.f, 700.f);
		G.Speed = Rng.FRandRange(0.16f, 0.24f);
		G.Phase = Rng.FRandRange(0.f, 10.f);
		Flock.Add(G);
	}

	// Dragonflies around reed clumps (the L01 reed patches are SM_LF_Reeds actors).
	TArray<FVector> Reeds;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		const UStaticMesh* M = It->GetStaticMeshComponent() ? It->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
		if (M && M->GetName() == TEXT("SM_LF_Reeds")) Reeds.Add(It->GetActorLocation());
	}
	UStaticMesh* FlyBody = AmbientMesh(TEXT("SM_LF_Dragonfly"));
	UStaticMesh* FlyWings = AmbientMesh(TEXT("SM_LF_Dragonfly_Wings"));
	for (int32 I = 0; FlyBody && FlyWings && Reeds.Num() && I < Dragonflies; ++I)
	{
		FFly F;
		F.Body = AddPart(FlyBody, 1.5f);
		F.Wings = AddPart(FlyWings, 1.5f);
		F.Wings->AttachToComponent(F.Body, FAttachmentTransformRules::KeepRelativeTransform);
		F.Wings->SetRelativeLocation(FVector(2.55f, 0.f, 0.f));
		F.Wings->SetRelativeScale3D(FVector(1.f));
		F.Anchor = Reeds[Rng.RandRange(0, Reeds.Num() - 1)] * FVector(1, 1, 0) + FVector(0, 0, LF::Water::BaseHeight + 90.f);
		F.Pos = F.Anchor;
		F.Target = F.Anchor;
		F.Phase = Rng.FRandRange(0.f, 10.f);
		Flies.Add(F);
	}
	UE_LOG(LogLF, Display, TEXT("[ambient] %d floaters, %d gulls, %d dragonflies (%d reed clumps)%s"), Floaters.Num(), Flock.Num(), Flies.Num(), Reeds.Num(),
		Swan ? TEXT("") : TEXT(" - swan mesh without head/neck, swans skipped"));
}

void ALFAmbientLife::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	TickFloaters(Dt, Now);
	TickFlock(Dt, Now);
	TickFlies(Dt, Now);
}

void ALFAmbientLife::TickFloaters(float Dt, float Now)
{
	TArray<FVector, TInlineAllocator<8>> Boats;
	for (TActorIterator<ALFBoat> It(GetWorld()); It; ++It) Boats.Add(It->GetActorLocation());
	for (FFloater& F : Floaters)
	{
		if (!F.Mesh) continue;
		const float T = Now + F.Phase;
		FVector Want;
		if (F.bBird)
		{
			// Paddle a slow loop round home; a boat within 6 m sends it off at a hurry.
			Want = (F.Home + FVector(FMath::Cos(T * 0.12f), FMath::Sin(T * 0.09f), 0.f) * F.WanderRadius - F.Pos).GetClampedToMaxSize(40.f);
			for (const FVector& B : Boats)
			{
				const FVector Away = (F.Pos - B) * FVector(1, 1, 0);
				const float D = float(Away.Size());
				if (D < 600.f) Want += Away.GetSafeNormal() * 220.f * (1.f - D / 600.f);
			}
		}
		else
		{
			Want = (F.Home + FVector(FMath::Sin(T * 0.05f), FMath::Cos(T * 0.04f), 0.f) * F.WanderRadius - F.Pos) * 0.1f; // lazy drift
		}
		F.Vel = FMath::Lerp(F.Vel, Want, 1.f - FMath::Exp(-1.5f * Dt));
		FVector Next = F.Pos + F.Vel * Dt;
		if (OpenWater(Next)) F.Pos = Next; else F.Vel = -F.Vel * 0.5f; // bounced off the shore / an island
		if (F.bBird && F.Vel.SizeSquared2D() > 25.f)
		{
			F.Yaw = FMath::FixedTurn(F.Yaw, F.Vel.Rotation().Yaw, 90.f * Dt);
		}
		else if (!F.bBird)
		{
			F.Yaw += 6.f * Dt; // slow spin
		}
		// Ride the waves: height at the centre, tilt from the slope across ~40 cm.
		const float H = LF::Water::GetHeightAt(F.Pos, Now);
		const float Hx = LF::Water::GetHeightAt(F.Pos + FVector(40.f, 0.f, 0.f), Now);
		const float Hy = LF::Water::GetHeightAt(F.Pos + FVector(0.f, 40.f, 0.f), Now);
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Hx - H, 40.f));
		const float Roll = FMath::RadiansToDegrees(FMath::Atan2(Hy - H, 40.f));
		const float Bob = F.bBird ? 0.f : 3.f * FMath::Sin(T * 1.7f);
		F.Mesh->SetWorldLocationAndRotation(FVector(F.Pos.X, F.Pos.Y, H + Bob), FRotator(-Pitch, F.Yaw, Roll));
	}
}

void ALFAmbientLife::TickFlock(float Dt, float Now)
{
	for (FGull& G : Flock)
	{
		if (!G.Body) continue;
		G.Angle += G.Speed * Dt;
		// 3 s flapping, 4 s gliding, each bird on its own clock.
		const float Cycle = FMath::Fmod(Now + G.Phase, 7.f);
		G.bGliding = Cycle >= 3.f;
		const FVector P(FMath::Cos(G.Angle) * G.Radius, FMath::Sin(G.Angle) * G.Radius, G.Height + 150.f * FMath::Sin((Now + G.Phase) * 0.4f));
		const float Yaw = FMath::RadiansToDegrees(G.Angle) + 90.f; // tangent to the circle (counter-clockwise)
		const FRotator Rot(G.bGliding ? -2.f : 3.f, Yaw, -14.f); // banked into the turn
		G.Body->SetWorldLocationAndRotation(P, Rot);
		G.Glide->SetWorldLocationAndRotation(P, Rot);
		G.Body->SetVisibility(!G.bGliding, true);
		G.Glide->SetVisibility(G.bGliding);
		if (!G.bGliding)
		{
			// Flap ~2.5 Hz between the authored up (+-42) and down (-+32) angles about the local X axis.
			const float S = FMath::Sin((Now + G.Phase) * 2.f * PI * 2.5f);
			const float A = S > 0.f ? 42.f * S : 32.f * S;
			G.WingL->SetRelativeRotation(FRotator(0.f, 0.f, A));
			G.WingR->SetRelativeRotation(FRotator(0.f, 0.f, -A));
		}
	}
}

void ALFAmbientLife::TickFlies(float Dt, float Now)
{
	for (FFly& F : Flies)
	{
		if (!F.Body) continue;
		if (Now >= F.NextDart)
		{
			// Dart to a new point within 2.5 m of the reeds, hover 0.4-1.6 s.
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			F.Target = F.Anchor + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(40.f, 250.f) + FVector(0, 0, FMath::FRandRange(-40.f, 60.f));
			F.NextDart = Now + FMath::FRandRange(0.4f, 1.6f);
		}
		const FVector To = F.Target - F.Pos;
		F.Pos += To * (1.f - FMath::Exp(-6.f * Dt));
		const FVector Hover(0.f, 0.f, 4.f * FMath::Sin((Now + F.Phase) * 9.f));
		const float Yaw = To.SizeSquared2D() > 4.f ? To.Rotation().Yaw : F.Body->GetComponentRotation().Yaw;
		F.Body->SetWorldLocationAndRotation(F.Pos + Hover, FRotator(0.f, Yaw, 0.f));
		F.Wings->SetRelativeRotation(FRotator(0.f, 0.f, 18.f * FMath::Sin((Now + F.Phase) * 2.f * PI * 11.f))); // buzz
	}
}

bool ALFAmbientLife::GetCritter(int32 I, FVector& Out, float& Size) const
{
	if (Floaters.IsValidIndex(I) && Floaters[I].Mesh) { Out = Floaters[I].Mesh->GetComponentLocation(); Size = 120.f; return true; }
	I -= Floaters.Num();
	if (Flock.IsValidIndex(I) && Flock[I].Body) { Out = Flock[I].Body->GetComponentLocation(); Size = 250.f; return true; }
	I -= Flock.Num();
	if (Flies.IsValidIndex(I) && Flies[I].Body) { Out = Flies[I].Body->GetComponentLocation(); Size = 25.f; return true; }
	return false;
}
