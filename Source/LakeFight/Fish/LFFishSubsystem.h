#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LFFishSubsystem.generated.h"

class ALFBoat;
enum class ELFTeam : uint8;
class AStaticMeshActor;
class UStaticMesh;
class USoundWave;

/** T04: fish spots any driven boat can work (player and AI alike), shared pickups, driver-team banking. */
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

	// ---- AI query API (N1 #14, for T04 stage 3 goals). Read-only; AIs still fish/bank by driving like the player.
	/** Best fish spot for Boat: the gold spot if bPreferGold and it is active, else the nearest live spot that no other
	 *  boat currently holds. False when nothing is available (all spots respawning or taken). */
	bool FindFishSpot(const ALFBoat* Boat, FVector& OutLocation, bool bPreferGold = true) const;
	/** Centre of the team's bank zone (deposit by driving the hull centre into it). */
	bool GetBankLocation(ELFTeam Team, FVector& OutLocation) const;
	/** True while Boat is the boat currently reeling from a spot (it must stay slow and close to keep the catch). */
	bool IsFishing(const ALFBoat* Boat) const;
	/** Fish aboard (ordinary + gold count) and their bank value in points. */
	static int32 GetCargoCount(const ALFBoat* Boat);
	static int32 GetCargoValue(const ALFBoat* Boat);
	/** Live floating pickups (spilled fish anyone can scoop) within Radius of From; nearest first in OutLocation. */
	bool FindPickup(const FVector& From, float Radius, FVector& OutLocation) const;
	int32 GetActiveSpotCount() const;

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
	/** 0 = use every discovered FishZone; positive values limit active spots. (Legacy placement only.) */
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish", meta=(ClampMin="0"))
	int32 MaxSpots = 0; // T04 hook

	/** N3 19: fair placement (director: Yellow won most rounds from a zone near its dock). Two live spots: one always on a
	 *  ring round the lake centre (the gold spot's own centre stays clear), one "lane" spot that rotates through the four
	 *  docks in shuffled rounds of four, each time exactly LaneDistFromBank from that team's bank on its centre line. The
	 *  FishZone_* markers are then only checked for presence. False = the old marker zones. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	bool bFairSpots = true;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float CentreRingMin = 0.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float CentreRingMax = 500.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	float LaneDistFromBank = 4300.f;
	/** A fair spot nobody has fished for this long (s) moves on (the lane spot to the next dock in the rotation). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish", meta=(ClampMin="1"))
	float IdleSpotLifetime = 25.f;

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
		int32 Slot = -1;     // N3 19: -1 marker zone, 0 centre ring, 1 rotating lane
		int32 LaneTeam = -1; // N3 19: the dock this lane spawn serves
		float LastActive = 0.f; // N3 19: spawn or last catch time
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
	TArray<int32> LaneBag;        // N3 19: shuffled teams still due a lane spot this rotation
	int32 LaneSpawns[4] = { 0, 0, 0, 0 };
	int32 RingSpawns = 0;
	int32 ZoneMarkers = 0;
	bool PlaceFairSpot(FSpot& Spot);
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
