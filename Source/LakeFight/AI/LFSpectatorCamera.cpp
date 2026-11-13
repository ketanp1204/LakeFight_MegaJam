#include "AI/LFSpectatorCamera.h"

#include "Brawler/LFBrawler.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ALFSpectatorCamera::ALFSpectatorCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ALFSpectatorCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FVector Sum = FVector::ZeroVector;
	TArray<FVector> Points;
	for (TActorIterator<ALFBrawler> It(GetWorld()); It; ++It)
	{
		if (It->GetBrawlerState() != ELFBrawlerState::None)
		{
			Points.Add(It->GetActorLocation());
			Sum += It->GetActorLocation();
		}
	}
	if (Points.Num() == 0)
	{
		return;
	}
	const FVector Centre = Sum / Points.Num();
	float Spread = 0.f;
	for (const FVector& P : Points)
	{
		Spread = FMath::Max(Spread, float(FVector::Dist2D(P, Centre)));
	}
	const float WantDist = FMath::Max(MinDistance, Spread * 2.f * SpreadScale);
	const float RealDt = FMath::Min(GetWorld()->DeltaRealTimeSeconds, 0.1f);
	const float A = bInit ? 1.f - FMath::Exp(-FollowSpeed * RealDt) : 1.f;
	bInit = true;
	Focus = FMath::Lerp(Focus, FVector(Centre.X, Centre.Y, 0.f), A);
	Dist = FMath::Lerp(Dist, WantDist, A);
	const FVector Dir = FVector(0.f, -1.f, 0.75f).GetSafeNormal(); // from the south, ~37 deg down
	const FVector Loc = Focus + Dir * Dist;
	SetActorLocationAndRotation(Loc, (Focus - Loc).Rotation());
}
