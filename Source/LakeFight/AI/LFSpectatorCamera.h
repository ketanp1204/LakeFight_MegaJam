#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "LFSpectatorCamera.generated.h"

/** lf.ai.spectate camera: follows the centroid of all brawlers still in play, pulling back as they spread out. */
UCLASS()
class LAKEFIGHT_API ALFSpectatorCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	ALFSpectatorCamera();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="LF|Spectate")
	float MinDistance = 1600.f;

	UPROPERTY(EditAnywhere, Category="LF|Spectate")
	float SpreadScale = 1.3f;

	UPROPERTY(EditAnywhere, Category="LF|Spectate")
	float FollowSpeed = 1.5f;

private:
	FVector Focus = FVector::ZeroVector;
	float Dist = 3000.f;
	bool bInit = false;
};
