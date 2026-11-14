#include "AI/LFCaptainAI.h"

#include "AI/LFCaptainPersonality.h"
#include "AI/LFSpectatorCamera.h"
#include "Boat/LFBoat.h"
#include "Fish/LFFishSubsystem.h"
#include "Round/LFRoundSubsystem.h"
#include "Brawler/LFBrawler.h"
#include "Water/LFWater.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarLFAIDebug(TEXT("lf.ai.debug"), 0, TEXT("1 = show each AI captain's state above its head."));
static TAutoConsoleVariable<int32> CVarLFAIFreeze(TEXT("lf.ai.freeze"), 0, TEXT("1 = AI captains stop deciding and acting (their boats coast to a stop) - for testing the player alone."));

// lf.ai.spectate: take the player out of the fight (hidden, untargetable) and watch the AI crews from an overview camera.
static FAutoConsoleCommandWithWorldAndArgs CmdLFAISpectate(TEXT("lf.ai.spectate"),
	TEXT("lf.ai.spectate [min m] [max m] - remove the player from the fight and watch the AI crews from above."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			return;
		}
		ALFBrawler* Player = Cast<ALFBrawler>(PC->GetPawn());
		if (!Player)
		{
			if (const ALFBoat* B = Cast<ALFBoat>(PC->GetPawn()))
			{
				Player = B->GetDriver();
			}
		}
		if (Player)
		{
			Player->SetSpectating();
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ALFSpectatorCamera* Cam = World->SpawnActor<ALFSpectatorCamera>(FVector(0.f, -4200.f, 2600.f), FRotator(-38.f, 90.f, 0.f), P);
		if (Cam)
		{
			// Optional: lf.ai.spectate <min m> <max m> (N2.16 hero shots want a tighter frame).
			if (Args.Num() >= 1) Cam->MinDistance = FCString::Atof(*Args[0]) * 100.f;
			if (Args.Num() >= 2) Cam->MaxDistance = FMath::Max(Cam->MinDistance, FCString::Atof(*Args[1]) * 100.f);
			PC->SetViewTargetWithBlend(Cam, 0.5f);
		}
		UE_LOG(LogLF, Display, TEXT("[ai] spectate: player removed from the fight, overview camera"));
	}));

namespace
{
	const TCHAR* CaptainStateName(ELFCaptainState S)
	{
		switch (S)
		{
		case ELFCaptainState::Chase: return TEXT("Chase");
		case ELFCaptainState::Ram: return TEXT("RAM");
		case ELFCaptainState::Fight: return TEXT("Fight");
		case ELFCaptainState::Steal: return TEXT("Steal");
		case ELFCaptainState::RecoverBoat: return TEXT("RecoverBoat");
		case ELFCaptainState::Flee: return TEXT("Flee");
		case ELFCaptainState::GoFish: return TEXT("GoFish");
		case ELFCaptainState::ReturnToDock: return TEXT("ReturnToDock");
		default: return TEXT("Idle");
		}
	}

	bool InBoat(const ALFBrawler* B)
	{
		return B && (B->GetBrawlerState() == ELFBrawlerState::Standing || B->GetBrawlerState() == ELFBrawlerState::Driving);
	}
}

ALFCaptainAI::ALFCaptainAI()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
}

void ALFCaptainAI::ApplyPersonality(const ULFCaptainPersonality* P)
{
	if (!P)
	{
		PersonalityName = TEXT("default");
		return;
	}
	PersonalityName = P->DisplayName.ToString();
	ReactionDelay = P->ReactionDelay;
	AimErrorDeg = P->AimErrorDeg;
	SwingCooldown = P->SwingCooldown;
	ChargedSwingChance = P->ChargedSwingChance;
	FightEnterRange = P->FightEnterRange;
	FightExitRange = P->FightExitRange;
	FightGiveUpTime = P->FightGiveUpTime;
	BoardRange = P->BoardRange;
	RamRange = P->RamRange;
	RamAngleDeg = P->RamAngleDeg;
	DriverTargetWeight = P->DriverTargetWeight;
	EmptySeatTargetWeight = P->EmptySeatTargetWeight;
	OwnBoatRecoverWeight = P->OwnBoatRecoverWeight;
	FleeHits = P->FleeHits;
	FleeTime = P->FleeTime;
	AggroRange = P->AggroRange;
	LootChaseValue = P->LootChaseValue;
	LootChaseRange = P->LootChaseRange;
	BankAtValue = P->BankAtValue;
	bChaseGold = P->bChaseGold;
}

void ALFCaptainAI::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	NextDecision = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.2f, 0.6f);
	const ALFBrawler* B = Cast<ALFBrawler>(InPawn);
	ApplyPersonality(B ? B->Personality.Get() : nullptr);
	UE_LOG(LogLF, Display, TEXT("[ai] %s captains %s (%s)"), *GetName(), InPawn ? *InPawn->GetName() : TEXT("none"), *PersonalityName);
}

ALFBrawler* ALFCaptainAI::Me() const
{
	return Cast<ALFBrawler>(GetPawn());
}

ALFBrawler* ALFCaptainAI::PickTarget() const
{
	// Nearest / most tempting enemy IN A BOAT (swimmers are out of the fight). Any other team - AIs fight each other too.
	const ALFBrawler* Self = Me();
	ALFBrawler* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<ALFBrawler> It(GetWorld()); It; ++It)
	{
		ALFBrawler* B = *It;
		if (B == Self || B->Team == Self->Team || !InBoat(B))
		{
			continue;
		}
		float Score = float(FVector::Dist2D(B->GetActorLocation(), Self->GetActorLocation()));
		if (B->GetBrawlerState() == ELFBrawlerState::Driving)
		{
			Score *= DriverTargetWeight;
		}
		if (B->GetBoat() && !B->GetBoat()->HasDriver())
		{
			Score *= EmptySeatTargetWeight;
		}
		if (B == Target.Get())
		{
			Score *= 0.8f; // stickiness: don't flip targets every decision
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = B;
		}
	}
	return Best;
}

ALFBoat* ALFCaptainAI::PickCrewlessBoat() const
{
	// A boat whose crew is all overboard (nobody standing or driving in it) and isn't ours: ram it.
	const ALFBrawler* Self = Me();
	ALFBoat* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<ALFBoat> It(GetWorld()); It; ++It)
	{
		ALFBoat* B = *It;
		if (B == Self->GetBoat() || B == Self->GetHomeBoat())
		{
			continue;
		}
		bool bCrewed = false;
		for (TActorIterator<ALFBrawler> BI(GetWorld()); BI; ++BI)
		{
			bCrewed |= InBoat(*BI) && BI->GetBoat() == B;
		}
		const float D = float(FVector::Dist2D(B->GetActorLocation(), Self->GetActorLocation()));
		if (!bCrewed && D < BestD && D < RamRange * 2.f)
		{
			BestD = D;
			Best = B;
		}
	}
	return Best;
}

ALFBoat* ALFCaptainAI::PickRecoverBoat() const
{
	const ALFBrawler* Self = Me();
	ALFBoat* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<ALFBoat> It(GetWorld()); It; ++It)
	{
		ALFBoat* B = *It;
		if (B->HasDriver() || B->IsCapsized())
		{
			continue; // nearest EMPTY, upright boat
		}
		float Score = float(FVector::Dist2D(B->GetActorLocation(), Self->GetActorLocation()));
		if (B == Self->GetHomeBoat())
		{
			Score *= OwnBoatRecoverWeight; // < 1 prefers our own, > 1 prefers stealing (Thief)
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = B;
		}
	}
	return Best ? Best : Self->GetHomeBoat();
}

void ALFCaptainAI::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ALFBrawler* Self = Me();
	if (!Self)
	{
		return;
	}
	const ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
	if (CVarLFAIFreeze.GetValueOnGameThread() != 0 || (Round && Round->IsInMenu())) // N1: hold still behind the main menu
	{
		ALFBoat* B = Self->GetBoat();
		if (B && B->GetDriver() == Self)
		{
			B->SetDriveInput(0.f, 0.f);
		}
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextDecision)
	{
		NextDecision = Now + DecisionInterval;
		Decide();
	}
	if (ApplyAt >= 0.0 && Now >= ApplyAt)
	{
		if (PendingState != State)
		{
			UE_LOG(LogLF, Display, TEXT("[ai] %s (%s): %s -> %s%s%s"), *Self->GetName(), *PersonalityName, CaptainStateName(State), CaptainStateName(PendingState),
				PendingTarget.IsValid() ? TEXT(" target ") : (GoalBoat.IsValid() ? TEXT(" boat ") : TEXT("")),
				PendingTarget.IsValid() ? *PendingTarget->GetName() : (GoalBoat.IsValid() ? *GoalBoat->GetName() : TEXT("")));
		}
		State = PendingState;
		Target = PendingTarget;
		ApplyAt = -1.0;
	}
	Execute(DeltaSeconds);

	const FString Who = Target.IsValid() ? Target->GetName() : (GoalBoat.IsValid() ? GoalBoat->GetName() : FString());
	Self->DebugState = FString::Printf(TEXT("%s: %s%s%s"), *PersonalityName, CaptainStateName(State), Who.IsEmpty() ? TEXT("") : TEXT(" > "), *Who);
	if (CVarLFAIDebug.GetValueOnGameThread() > 0)
	{
		DrawDebugString(GetWorld(), Self->GetActorLocation() + FVector(0.f, 0.f, 110.f), Self->DebugState, nullptr, FColor::Yellow, 0.f, true, 1.3f);
	}
}

void ALFCaptainAI::Decide()
{
	ALFBrawler* Self = Me();
	const double Now = GetWorld()->GetTimeSeconds();
	const ELFBrawlerState S = Self->GetBrawlerState();
	ELFCaptainState Want = ELFCaptainState::Idle;
	ALFBrawler* T = PickTarget();

	if (Self->RecentHits(FleeWindow) >= FleeHits && FleeUntil < Now)
	{
		FleeUntil = Now + FleeTime;
	}

	if (S == ELFBrawlerState::Swimming)
	{
		Want = ELFCaptainState::RecoverBoat;
		GoalBoat = PickRecoverBoat();
		T = nullptr;
	}
	else if (S == ELFBrawlerState::Standing || S == ELFBrawlerState::Driving)
	{
		ALFBoat* MyBoat = Self->GetBoat();
		const float TDist = T ? float(FVector::Dist2D(T->GetActorLocation(), Self->GetActorLocation())) : TNumericLimits<float>::Max();
		FVector2D Unused;
		const bool bHullsClose = T && MyBoat && BoardRange > 0.f && MyBoat->GunwaleDistance(T->GetActorLocation(), Unused) < BoardRange;
		if (TDist <= SwingRange)
		{
			LastInReach = Now;
		}
		// Fight hysteresis: enter close, stay until clearly apart or out of reach for a while.
		const bool bStayFighting = State == ELFCaptainState::Fight && T && T == Target.Get()
			&& TDist < FightExitRange && (Now - LastInReach) < FightGiveUpTime;

		if (Now < FleeUntil && T)
		{
			Want = ELFCaptainState::Flee;
		}
		else if (T && (TDist < FightEnterRange || bHullsClose || bStayFighting))
		{
			Want = ELFCaptainState::Fight;
			if (State != ELFCaptainState::Fight)
			{
				LastInReach = Now;
			}
		}
		else if (S == ELFBrawlerState::Standing && MyBoat && MyBoat != Self->GetHomeBoat() && !MyBoat->HasDriver())
		{
			Want = ELFCaptainState::Steal; // in someone else's empty boat: take the seat
		}
		else
		{
			// T04 stage 3: fishing economy first - bank a good catch, otherwise fish - unless an enemy is close (AggroRange) or
			// worth robbing (cargo >= LootChaseValue within LootChaseRange). Fights still start whenever boats meet (above).
			ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
			const ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
			const FVector From = MyBoat ? MyBoat->GetActorLocation() : Self->GetActorLocation();
			const int32 Value = ULFFishSubsystem::GetCargoValue(MyBoat);
			FVector BankAt = FVector::ZeroVector, Spot = FVector::ZeroVector;
			const bool bHaveBank = Fish && Fish->GetBankLocation(Self->Team, BankAt);
			const float TimeHome = bHaveBank ? float(FVector::Dist2D(From, BankAt)) / 500.f + 8.f : 0.f;
			const bool bClock = Round && Value > 0 && Round->GetRemaining() < TimeHome + 10.f;
			const int32 TLoot = T && T->GetBoat() ? ULFFishSubsystem::GetCargoValue(T->GetBoat()) : 0;
			const bool bLoot = TLoot >= LootChaseValue && TDist < LootChaseRange;
			// Give up a fruitless chase for a while (loot is always worth chasing).
			const bool bChasing = State == ELFCaptainState::Chase || State == ELFCaptainState::Ram;
			if (!bChasing) ChaseSince = -1.0;
			else if (ChaseSince < 0.0) ChaseSince = Now;
			if (bChasing && !bLoot && ChaseSince >= 0.0 && Now - ChaseSince > MaxChaseTime)
			{
				NoChaseUntil = Now + ChaseCooldown;
				ChaseSince = -1.0;
			}
			const bool bGoAfter = T && (bLoot || (TDist < AggroRange && Now >= NoChaseUntil));
			if (MyBoat && bHaveBank && Value > 0 && (Value >= BankAtValue || bClock) && !(bGoAfter && TDist < AggroRange * 0.5f))
			{
				Want = ELFCaptainState::ReturnToDock;
				GoalPoint = BankAt;
				T = nullptr;
			}
			else if (!bGoAfter && MyBoat && Fish && Fish->FindFishSpot(MyBoat, Spot, bChaseGold))
			{
				Want = ELFCaptainState::GoFish;
				GoalPoint = Spot;
				T = nullptr;
			}
		}
		if (Want == ELFCaptainState::Idle && (S == ELFBrawlerState::Standing || S == ELFBrawlerState::Driving) && !(Now < FleeUntil && T))
		{
			// Chase / ram an enemy boat - or ram a boat whose crew is all overboard.
			ALFBoat* Crewless = T ? nullptr : PickCrewlessBoat();
			const AActor* Goal = T ? static_cast<const AActor*>(T->GetBoat()) : static_cast<const AActor*>(Crewless);
			GoalBoat = Crewless;
			if (Goal)
			{
				Want = ELFCaptainState::Chase;
				if (S == ELFBrawlerState::Driving && MyBoat && Goal != MyBoat)
				{
					const FVector To = (Goal->GetActorLocation() - MyBoat->GetActorLocation()) * FVector(1, 1, 0);
					const float Ang = FMath::Abs(FMath::FindDeltaAngleDegrees(MyBoat->GetActorRotation().Yaw, To.Rotation().Yaw));
					if (Ang < RamAngleDeg && To.Size() < RamRange && MyBoat->GetForwardSpeed() > 150.f)
					{
						Want = ELFCaptainState::Ram;
					}
				}
			}
		}
	}

	if (Want != PendingState || T != PendingTarget.Get() || ApplyAt < 0.0)
	{
		const bool bChange = Want != State || T != Target.Get();
		PendingState = Want;
		PendingTarget = T;
		ApplyAt = bChange ? Now + FMath::FRandRange(ReactionDelay.X, ReactionDelay.Y) : Now;
	}
}

void ALFCaptainAI::Execute(float DeltaSeconds)
{
	ALFBrawler* Self = Me();
	const ELFBrawlerState S = Self->GetBrawlerState();
	ALFBrawler* T = Target.Get();
	ALFBoat* MyBoat = Self->GetBoat();
	const double Now = GetWorld()->GetTimeSeconds();

	if (S == ELFBrawlerState::Swimming)
	{
		ALFBoat* Goal = GoalBoat.Get();
		if (State == ELFCaptainState::RecoverBoat && Goal)
		{
			FVector2D Unused;
			if (Goal->GunwaleDistance(Self->GetActorLocation(), Unused) <= Self->GrabRange * 0.9f)
			{
				Self->TryGrabHull(Goal);
			}
			else
			{
				Self->StartSwimTo(Goal->GetActorLocation());
			}
		}
		return;
	}

	// What we're driving at: the target's boat (or the target), or a crew-less boat.
	const AActor* DriveGoal = T ? (T->GetBoat() ? static_cast<const AActor*>(T->GetBoat()) : static_cast<const AActor*>(T))
		: static_cast<const AActor*>(GoalBoat.Get());

	if (S == ELFBrawlerState::Driving && MyBoat)
	{
		switch (State)
		{
		case ELFCaptainState::Chase:
			if (DriveGoal)
			{
				DriveToward(DriveGoal->GetActorLocation() + DriveGoal->GetVelocity() * LookaheadTime, 1.f);
			}
			break;
		case ELFCaptainState::Ram:
			if (DriveGoal)
			{
				DriveToward(DriveGoal->GetActorLocation(), 1.f);
			}
			break;
		case ELFCaptainState::Flee:
			if (T)
			{
				const FVector Away = (MyBoat->GetActorLocation() - T->GetActorLocation()).GetSafeNormal2D();
				DriveToward(MyBoat->GetActorLocation() + Away * 2000.f, 1.f);
			}
			break;
		case ELFCaptainState::Fight:
			MyBoat->SetDriveInput(0.f, 0.f);
			Self->LeaveDriving(); // stand up to fight
			break;
		case ELFCaptainState::GoFish:
			DriveTo(GoalPoint, SpotHoldRadius, 60.f, true);
			break;
		case ELFCaptainState::ReturnToDock:
			// Drive right into the bank (the hull centre must enter it); no shore avoidance near the dock.
			DriveTo(GoalPoint, 0.f, 180.f, FVector::Dist2D(GoalPoint, MyBoat->GetActorLocation()) > 1500.f);
			break;
		default:
			MyBoat->SetDriveInput(0.f, 0.f);
			break;
		}
		return;
	}

	if (S == ELFBrawlerState::Standing && MyBoat)
	{
		if (State == ELFCaptainState::Fight && T)
		{
			const float D = float(FVector::Dist2D(T->GetActorLocation(), Self->GetActorLocation()));
			if (D <= SwingRange)
			{
				if (Self->CanSwing() && Now - LastSwing >= SwingCooldown)
				{
					FaceAndSwing(T);
				}
			}
			else
			{
				WalkToward(T->GetActorLocation()); // edge of our boat closest to them (can't walk off)
			}
			return;
		}
		if (State == ELFCaptainState::Chase || State == ELFCaptainState::Steal || State == ELFCaptainState::Flee || State == ELFCaptainState::Ram
			|| State == ELFCaptainState::GoFish || State == ELFCaptainState::ReturnToDock)
		{
			// Take the seat (drive/steal) - interact AT the seat, like the player.
			if (!MyBoat->HasDriver())
			{
				const FVector Seat = MyBoat->GetDriverSeatWorld();
				if (FVector::Dist2D(Seat, Self->GetActorLocation()) <= Self->DriveReach * 0.8f)
				{
					Self->TryDriveBoat();
				}
				else
				{
					WalkToward(Seat);
				}
			}
		}
	}
}

void ALFCaptainAI::DriveToward(const FVector& Point, float Throttle)
{
	ALFBoat* MyBoat = Me()->GetBoat();
	if (!MyBoat)
	{
		return;
	}
	const FVector To = (Point - MyBoat->GetActorLocation()) * FVector(1, 1, 0);
	const float Ang = FMath::FindDeltaAngleDegrees(MyBoat->GetActorRotation().Yaw, To.Rotation().Yaw); // + = target to the right
	float Steer = FMath::Clamp(Ang / 35.f, -1.f, 1.f);
	Steer = AvoidShore(Steer);
	const float Thr = FMath::Abs(Ang) > 100.f ? Throttle * 0.35f : Throttle;
	MyBoat->SetDriveInput(Thr, Steer);
}

void ALFCaptainAI::DriveTo(const FVector& Point, float HoldRadius, float MinApproachSpeed, bool bAvoidShore)
{
	ALFBoat* MyBoat = Me()->GetBoat();
	if (!MyBoat)
	{
		return;
	}
	const FVector To = (Point - MyBoat->GetActorLocation()) * FVector(1, 1, 0);
	const float Dist = float(To.Size());
	const float Ang = FMath::FindDeltaAngleDegrees(MyBoat->GetActorRotation().Yaw, To.Rotation().Yaw);
	const float Fwd = MyBoat->GetForwardSpeed();
	// Wanted forward speed: fast far away, ~0.5 cm/s per cm of remaining distance near, zero inside the hold radius (fishing
	// needs < 220 cm/s within 400 cm). Throttle chases that speed - negative throttle brakes.
	float Want = Dist <= HoldRadius ? 0.f : FMath::Clamp((Dist - HoldRadius) * 0.5f, MinApproachSpeed, 700.f);
	if (FMath::Abs(Ang) > 60.f)
	{
		Want = FMath::Min(Want, 120.f); // turn first (the low-speed pivot does the work)
	}
	const float Throttle = FMath::Clamp((Want - Fwd) / 150.f, -1.f, 1.f);
	float Steer = Dist <= HoldRadius ? 0.f : FMath::Clamp(Ang / 35.f, -1.f, 1.f);
	if (bAvoidShore)
	{
		Steer = AvoidShore(Steer);
	}
	MyBoat->SetDriveInput(Throttle, Steer);
}

float ALFCaptainAI::AvoidShore(float Steer) const
{
	// No navmesh on water: three traces ahead (centre, +-35 deg) against the static world.
	const ALFBoat* MyBoat = Me()->GetBoat();
	const FVector From = MyBoat->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const float Speed = float(MyBoat->GetVelocity().Size2D());
	const float Len = AvoidBaseLength + Speed * AvoidSpeedTime;
	const float Yaw = MyBoat->GetActorRotation().Yaw;
	FCollisionObjectQueryParams Static(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LFAIAvoid), false, MyBoat);
	auto Hit = [&](float Offset, float& OutFrac)
	{
		FHitResult H;
		const FVector Dir = FRotator(0.f, Yaw + Offset, 0.f).Vector();
		const bool b = GetWorld()->LineTraceSingleByObjectType(H, From, From + Dir * Len, Static, Params);
		OutFrac = b ? H.Time : 1.f;
		return b;
	};
	float FC, FL, FR;
	const bool bC = Hit(0.f, FC);
	const bool bL = Hit(-35.f, FL);
	const bool bR = Hit(35.f, FR);
	if (!bC && !bL && !bR)
	{
		return Steer;
	}
	const float Urgency = 1.f - FMath::Min3(FC, FL, FR);
	const float Away = FL < FR ? 1.f : -1.f;
	return FMath::Clamp(Steer * (1.f - Urgency) + Away * FMath::Max(Urgency, bC ? 0.7f : 0.4f), -1.f, 1.f);
}

void ALFCaptainAI::FaceAndSwing(ALFBrawler* T)
{
	ALFBrawler* Self = Me();
	const FVector To = (T->GetActorLocation() - Self->GetActorLocation()) * FVector(1, 1, 0);
	const float Yaw = To.Rotation().Yaw + FMath::FRandRange(-AimErrorDeg, AimErrorDeg);
	SetControlRotation(FRotator(0.f, Yaw, 0.f));
	const bool bCharged = T->IsNearGunwale() || FMath::FRand() < ChargedSwingChance;
	Self->Swing(bCharged ? FMath::FRandRange(0.8f, 1.f) : -1.f);
	LastSwing = GetWorld()->GetTimeSeconds();
}

void ALFCaptainAI::WalkToward(const FVector& Point)
{
	ALFBrawler* Self = Me();
	const FVector Dir = (Point - Self->GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		Self->AddMovementInput(Dir, 1.f);
	}
}
