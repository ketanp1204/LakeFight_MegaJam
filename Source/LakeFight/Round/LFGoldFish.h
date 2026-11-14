#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LFGoldFish.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class ALFBoat;

/** One bonus spot at halftime; spent gold spots do not respawn. Same actor serves spilled gold pickups. */
UCLASS()
class LAKEFIGHT_API ALFGoldFish : public AActor
{
	GENERATED_BODY()
public:
	ALFGoldFish();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void MakePickup();
	bool IsSpot() const { return !bPickup; }

	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float CatchInterval = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float CatchMaxSpeed = 150.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float CatchRadius = 200.f;

private:
	UPROPERTY(VisibleAnywhere, Category="LF|Fish")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> GoldMesh;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Sparkles;
	FVector Centre = FVector::ZeroVector;
	TWeakObjectPtr<ALFBoat> Catcher;
	float Progress = 0.f;
	float Born = 0.f;
	int32 Remaining = 1;
	bool bPickup = false;
};
