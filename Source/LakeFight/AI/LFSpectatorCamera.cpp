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
	// L01 (director: "the camera is way too high up"): on the 150 x 116 m lake the crews start ~100 m apart, so framing all of
	// them put the camera ~180 m out. Follow the action instead - the closest pair of brawlers - and cap the distance.
	FVector Centre = Sum / Points.Num();
	float Spread = 0.f;
	if (Points.Num() >= 2)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Points.Num(); ++I)
		{
			for (int32 J = I + 1; J < Points.Num(); ++J)
			{
				const float D = float(FVector::Dist2D(Points[I], Points[J]));
				if (D < Best)
				{
					Best = D;
					Centre = (Points[I] + Points[J]) * 0.5f;
					Spread = D * 0.5f;
				}
			}
		}
	}
	const float WantDist = FMath::Clamp(Spread * 2.f * SpreadScale, MinDistance, FMath::Max(MaxDistance, MinDistance));
	const float RealDt = FMath::Min(GetWorld()->DeltaRealTimeSeconds, 0.1f);
	const float A = bInit ? 1.f - FMath::Exp(-FollowSpeed * RealDt) : 1.f;
	bInit = true;
	Focus = FMath::Lerp(Focus, FVector(Centre.X, Centre.Y, 0.f), A);
	Dist = FMath::Lerp(Dist, WantDist, A);
	const FVector Dir = FVector(0.f, -1.f, 0.6f).GetSafeNormal(); // from the south, ~31 deg down
	const FVector Loc = Focus + Dir * Dist;
	SetActorLocationAndRotation(Loc, (Focus - Loc).Rotation());
}
