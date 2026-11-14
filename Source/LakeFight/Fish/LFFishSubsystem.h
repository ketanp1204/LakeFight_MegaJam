#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LFFishSubsystem.generated.h"

class ALFBoat;
class AStaticMeshActor;
class UStaticMesh;
class USoundWave;

/** T04 stage 1: player fishing, shared pickups, driver-team banking. No AI decisions. */
UCLASS()
class LAKEFIGHT_API ULFFishSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULFFishSubsystem();
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	int32 Spill(ALFBoat* Boat, int32 Amount); // T04 hook: return pickups actually spawned
	int32 GetScore(int32 Team) const { return Scores[FMath::Clamp(Team, 0, 3)]; } // T04 hook

	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float CatchInterval = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float CatchMaxSpeed = 220.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float CatchRadius = 400.f;
	/** TargetPoints have no area: use a 3 m disc around each marker. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float ZoneRadius = 300.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float DriftSpeed = 50.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float PickupLifetime = 10.f;
	/** 0 = use every discovered FishZone; positive values limit active spots. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish", meta=(ClampMin="0"))
	int32 MaxSpots = 0; // T04 hook

private:
	struct FSpot
	{
		FVector Centre = FVector::ZeroVector;
		FVector Goal = FVector::ZeroVector;
		TWeakObjectPtr<AStaticMeshActor> Mesh;
		TWeakObjectPtr<ALFBoat> Catcher;
		int32 Remaining = 0;
		float Progress = 0.f;
		float RespawnAt = 0.f;
	};
	struct FPickup
	{
		TWeakObjectPtr<ALFBoat> SourceBoat; // T04 hook
		TWeakObjectPtr<AStaticMeshActor> Mesh;
		float Born = 0.f;
	};
	struct FBank
	{
		TWeakObjectPtr<AActor> Zone;
		int32 Team = 0;
	};
	struct FFlight
	{
		TWeakObjectPtr<AStaticMeshActor> Mesh;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Born = 0.f;
	};
	TArray<FSpot> Spots;
	TArray<FPickup> Pickups;
	TArray<FBank> Banks;
	TArray<FFlight> Flights;
	int32 Scores[4] = { 0, 0, 0, 0 };
	int32 Caught = 0;
	int32 Spilled = 0;
	int32 Scooped = 0;
	int32 Banked = 0;
	float NextReport = 10.f;
	bool bStarted = false;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SpotMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> FishMesh;
	UPROPERTY()
	TObjectPtr<USoundWave> CatchSound;
	UPROPERTY()
	TObjectPtr<USoundWave> BankSound;

	AStaticMeshActor* MakeMesh(UStaticMesh* Mesh, const FVector& At, float Scale);
	FVector PickPoint(const FVector& Centre) const;
	bool IsOpenWater(const FVector& At) const;
	void Respawn(FSpot& Spot);
	void PlaySound(USoundWave* Sound);
};
