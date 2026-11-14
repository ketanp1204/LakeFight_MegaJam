#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LFAmbientLife.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * N1 item 9 (stretch): ambient life on and over the lake, all original B11 meshes, all NoCollision, one cheap tick.
 * - Floaters (ducks x1.75, swans, barrel, life ring, bottle x1.75) bob and tilt on LF::Water; birds paddle round a home
 *   point and hurry away from boats, junk drifts and spins.
 * - A gull flock circles high over the lake, flapping (B11 wing parts on their shoulder pivots) then gliding (B11 glide mesh).
 * - Dragonflies dart around a few reed clumps.
 */
UCLASS()
class LAKEFIGHT_API ALFAmbientLife : public AActor
{
	GENERATED_BODY()
public:
	ALFAmbientLife();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, Category="LF|Ambient") int32 Ducks = 6;
	UPROPERTY(EditDefaultsOnly, Category="LF|Ambient") int32 Swans = 2;
	UPROPERTY(EditDefaultsOnly, Category="LF|Ambient") int32 Junk = 3;          // barrel, life ring, bottle
	UPROPERTY(EditDefaultsOnly, Category="LF|Ambient") int32 Gulls = 5;
	UPROPERTY(EditDefaultsOnly, Category="LF|Ambient") int32 Dragonflies = 6;
	UPROPERTY(EditDefaultsOnly, Category="LF|Ambient") float GullHeight = 3800.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Ambient") float GullScale = 2.2f;

	/** Dev (lf.ambient.look): world location of critter I (floaters, then gulls, then dragonflies); false if out of range. */
	bool GetCritter(int32 I, FVector& Out, float& Size) const;

private:
	struct FFloater
	{
		TObjectPtr<UStaticMeshComponent> Mesh;
		FVector Home = FVector::ZeroVector;
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		float Yaw = 0.f;
		float Phase = 0.f;
		float WanderRadius = 300.f;
		bool bBird = true;
	};
	struct FGull
	{
		TObjectPtr<UStaticMeshComponent> Body;
		TObjectPtr<UStaticMeshComponent> WingL;
		TObjectPtr<UStaticMeshComponent> WingR;
		TObjectPtr<UStaticMeshComponent> Glide;
		float Angle = 0.f;
		float Radius = 3000.f;
		float Height = 0.f;
		float Speed = 0.3f; // rad/s round the circle
		float Phase = 0.f;
		bool bGliding = false;
	};
	struct FFly
	{
		TObjectPtr<UStaticMeshComponent> Body;
		TObjectPtr<UStaticMeshComponent> Wings;
		FVector Anchor = FVector::ZeroVector;
		FVector Pos = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
		float NextDart = 0.f;
		float Phase = 0.f;
	};
	TArray<FFloater> Floaters;
	TArray<FGull> Flock;
	TArray<FFly> Flies;

	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, float Scale);
	bool OpenWater(const FVector& At) const;
	bool PickWaterPoint(FRandomStream& Rng, float MinR, float MaxR, FVector& Out) const;
	void TickFloaters(float Dt, float Now);
	void TickFlock(float Dt, float Now);
	void TickFlies(float Dt, float Now);
};

/** Spawns one ALFAmbientLife per game world (no level edit needed). Off with -LFNoAmbient. */
UCLASS()
class LAKEFIGHT_API ULFAmbientLifeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
