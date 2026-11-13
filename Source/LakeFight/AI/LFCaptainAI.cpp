#include "AI/LFCaptainAI.h"

#include "Boat/LFBoat.h"
#include "Brawler/LFBrawler.h"
#include "Water/LFWater.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarLFAIDebug(TEXT("lf.ai.debug"), 0, TEXT("1 = show each AI captain's state above its head."));

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
		default: return TEXT("Idle");
		}
	}

	bool InPlay(const ALFBrawler* B)
	{
		return B && B->GetBrawlerState() != ELFBrawlerState::Ragdoll && B->GetBrawlerState() != ELFBrawlerState::None;
	}
}

ALFCaptainAI::ALFCaptainAI()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
}

void ALFCaptainAI::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	NextDecision = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.2f, 0.6f);
	UE_LOG(LogLF, Display, TEXT("[ai] %s captains %s"), *GetName(), InPawn ? *InPawn->GetName() : TEXT("none"));
}

ALFBrawler* ALFCaptainAI::Me() const
{
	return Cast<ALFBrawler>(GetPawn());
}

ALFBrawler* ALFCaptainAI::PickTarget() const
{
	const ALFBrawler* Self = Me();
	ALFBrawler* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (TActorIterator<ALFBrawler> It(GetWorld()); It; ++It)
	{
		ALFBrawler* B = *It;
		if (B == Self || B->Team == Self->Team || !InPlay(B))
		{
			continue;
		}
		float D = float(FVector::Dist2D(B->GetActorLocation(), Self->GetActorLocation()));
		if (B->GetBrawlerState() == ELFBrawlerState::Swimming)
		{
			D *= 1.5f; // swimmers are less interesting than enemies in boats
		}
		if (D < BestD)
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
		if (B->HasDriver())
		{
			continue; // nearest EMPTY boat
		}
		float Score = float(FVector::Dist2D(B->GetActorLocation(), Self->GetActorLocation()));
		if (B == Self->GetHomeBoat())
		{
			Score *= 0.6f; // prefer our own
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
			UE_LOG(LogLF, Display, TEXT("[ai] %s: %s -> %s%s%s"), *Self->GetName(), CaptainStateName(State), CaptainStateName(PendingState),
				PendingTarget.IsValid() ? TEXT(" target ") : TEXT(""), PendingTarget.IsValid() ? *PendingTarget->GetName() : TEXT(""));
		}
		State = PendingState;
		Target = PendingTarget;
		ApplyAt = -1.0;
	}
	Execute(DeltaSeconds);

	Self->DebugState = FString::Printf(TEXT("%s%s"), CaptainStateName(State), Target.IsValid() ? *FString::Printf(TEXT(" > %s"), *Target->GetName()) : TEXT(""));
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
		const bool bTargetInBoat = T && (T->GetBrawlerState() == ELFBrawlerState::Standing || T->GetBrawlerState() == ELFBrawlerState::Driving);
		const float TDist = T ? float(FVector::Dist2D(T->GetActorLocation(), Self->GetActorLocation())) : TNumericLimits<float>::Max();
		FVector2D Unused;
		const bool bHullsClose = T && MyBoat && MyBoat->GunwaleDistance(T->GetActorLocation(), Unused) < BoardRange;

		if (Now < FleeUntil && T)
		{
			Want = ELFCaptainState::Flee;
		}
		else if (T && (TDist < SwingRange * 1.15f || (bTargetInBoat && bHullsClose)))
		{
			Want = ELFCaptainState::Fight; // board/fight: stand and paddle
		}
		else if (S == ELFBrawlerState::Standing && MyBoat && MyBoat != Self->GetHomeBoat() && !MyBoat->HasDriver())
		{
			Want = ELFCaptainState::Steal; // in someone else's empty boat: take the seat
		}
		else if (T)
		{
			Want = ELFCaptainState::Chase;
			if (S == ELFBrawlerState::Driving && MyBoat && T->GetBoat() && T->GetBoat() != MyBoat)
			{
				const FVector To = (T->GetBoat()->GetActorLocation() - MyBoat->GetActorLocation()) * FVector(1, 1, 0);
				const float Ang = FMath::Abs(FMath::FindDeltaAngleDegrees(MyBoat->GetActorRotation().Yaw, To.Rotation().Yaw));
				const float Speed = MyBoat->GetForwardSpeed();
				if (Ang < RamAngleDeg && To.Size() < RamRange && Speed > 150.f)
				{
					Want = ELFCaptainState::Ram;
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

	if (S == ELFBrawlerState::Driving && MyBoat)
	{
		switch (State)
		{
		case ELFCaptainState::Chase:
			if (T)
			{
				const AActor* Goal = T->GetBoat() ? static_cast<const AActor*>(T->GetBoat()) : static_cast<const AActor*>(T);
				DriveToward(Goal->GetActorLocation() + Goal->GetVelocity() * LookaheadTime, 1.f);
			}
			break;
		case ELFCaptainState::Ram:
			if (T)
			{
				const AActor* Goal = T->GetBoat() ? static_cast<const AActor*>(T->GetBoat()) : static_cast<const AActor*>(T);
				DriveToward(Goal->GetActorLocation(), 1.f);
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
		if (State == ELFCaptainState::Chase || State == ELFCaptainState::Steal || State == ELFCaptainState::Flee || State == ELFCaptainState::Ram)
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
	// Turn toward the freer side, harder the closer the wall.
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
	const float Strength = T->IsNearGunwale() ? FMath::FRandRange(0.8f, 1.f) : -1.f; // charged if they're near the gunwale
	Self->Swing(Strength);
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
