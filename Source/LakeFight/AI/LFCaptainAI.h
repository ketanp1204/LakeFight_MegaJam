#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "LFCaptainAI.generated.h"

class ALFBrawler;
class ALFBoat;
class ULFCaptainPersonality;

UENUM(BlueprintType)
enum class ELFCaptainState : uint8
{
	Idle,
	Chase,
	Ram,
	Fight,
	Steal,
	RecoverBoat,
	Flee,
	GoFish,       // T04 stage 3: drive to a fish spot and hold station on it (slow, close) while the catches come in
	ReturnToDock  // T04 stage 3: carry the catch into our team's bank zone
};

/**
 * T03 AI captain. Possesses an ALFBrawler and plays with the player's rules: it only uses the brawler/boat actions
 * (StartSwimTo, TryGrabHull, TryDriveBoat, Swing, AddMovementInput, ALFBoat::SetDriveInput) - no teleports, no extra forces.
 * Decisions every DecisionInterval (+ a reaction delay); steering/aim every tick. Behaviour weights come from the brawler's
 * ULFCaptainPersonality (Bully / Thief / Hothead) when set; otherwise the defaults below.
 */
UCLASS()
class LAKEFIGHT_API ALFCaptainAI : public AAIController
{
	GENERATED_BODY()

public:
	ALFCaptainAI();

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0.1"))
	float DecisionInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category="LF|AI")
	FVector2D ReactionDelay = FVector2D(0.15f, 0.35f);

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float AimErrorDeg = 12.f;

	/** Swing when the target is within this horizontal distance (cm). */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float SwingRange = 165.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float SwingCooldown = 1.1f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0", ClampMax="1"))
	float ChargedSwingChance = 0.2f;

	/** Fight hysteresis: enter within FightEnterRange, leave beyond FightExitRange or after FightGiveUpTime out of reach. */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float FightEnterRange = 190.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float FightExitRange = 350.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float FightGiveUpTime = 2.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float BoardRange = 120.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float RamAngleDeg = 20.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float RamRange = 1500.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float LookaheadTime = 0.8f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float AvoidBaseLength = 500.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float AvoidSpeedTime = 1.5f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="1"))
	int32 FleeHits = 2;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float FleeWindow = 3.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float FleeTime = 4.f;

	UPROPERTY(EditAnywhere, Category="LF|AI")
	float DriverTargetWeight = 1.f;

	UPROPERTY(EditAnywhere, Category="LF|AI")
	float EmptySeatTargetWeight = 1.f;

	UPROPERTY(EditAnywhere, Category="LF|AI")
	float OwnBoatRecoverWeight = 0.6f;

	// --- T04 stage 3: fishing economy (personality overrides) ---
	/** Go after an enemy boat within this range (cm) even while fishing. */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fish", meta=(ClampMin="0"))
	float AggroRange = 1500.f;
	/** Go after an enemy boat carrying at least this many points of fish (within LootChaseRange). */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fish", meta=(ClampMin="1"))
	int32 LootChaseValue = 4;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fish", meta=(ClampMin="0"))
	float LootChaseRange = 4000.f;
	/** Head home to bank once our cargo is worth this many points (or when the clock says so). */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fish", meta=(ClampMin="1"))
	int32 BankAtValue = 5;
	/** Drop everything for the gold spot when it appears. */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fish")
	bool bChaseGold = true;
	/** Station keeping: stop within this distance (cm) of the spot (catch radius is 400 cm, max 220 cm/s). */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fish", meta=(ClampMin="0"))
	float SpotHoldRadius = 150.f;
	/** A chase (not for loot) that hasn't come to blows after this long (s) is dropped for ChaseCooldown (s): back to fishing. */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fish", meta=(ClampMin="0"))
	float MaxChaseTime = 9.f;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fish", meta=(ClampMin="0"))
	float ChaseCooldown = 12.f;

	// --- N3 20: reasons to fight ---
	/** Target pick: distance is divided by (1 + this * fish points aboard the target's boat) - loaded boats draw fire. */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float CargoTargetWeight = 0.2f;
	/** A boat carrying at least LootNearValue points within AggroRange * LootNearRangeScale is chased like loot. */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="1"))
	int32 LootNearValue = 2;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float LootNearRangeScale = 2.f;
	/** Rubber band: when one team leads by at least LeaderMinLead points, every other crew targets it within LeaderFocusRange. */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	int32 LeaderMinLead = 4;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float LeaderFocusRange = 3500.f;
	/** Leader distance multiplier in the target pick (< 1 = preferred). */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0.05"))
	float LeaderTargetWeight = 0.4f;
	/** Defend the spot: while fishing, ram an enemy boat that comes within DefendRadius of the spot; stop when it is beyond
	 *  DefendExitRadius or after DefendMaxTime, then don't defend again for DefendCooldown (hysteresis, no flip-flop). */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float DefendRadius = 800.f;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float DefendExitRadius = 1400.f;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float DefendMaxTime = 7.f;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float DefendCooldown = 4.f;
	/** Thief (OwnBoatRecoverWeight > 1): hunt enemy boats carrying fish within ThiefHuntRange that are within ThiefDockRange
	 *  of their own bank (on the way in to bank); when such a boat is crewless within ThiefDiveRange, dive in and take it. */
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float ThiefHuntRange = 5000.f;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float ThiefDockRange = 3500.f;
	UPROPERTY(EditAnywhere, Category="LF|AI|Fight", meta=(ClampMin="0"))
	float ThiefDiveRange = 600.f;

	UFUNCTION(BlueprintPure, Category="LF|AI")
	ELFCaptainState GetCaptainState() const { return State; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	ALFBrawler* Me() const;
	ALFBrawler* PickTarget() const;
	int32 LeadingTeam() const;                // N3 20: team ahead by >= LeaderMinLead (not ours), else -1
	ALFBrawler* PickSpotIntruder() const;     // N3 20: enemy boat crew within DefendRadius of our fishing spot
	ALFBrawler* PickThiefMark() const;        // N3 20: loaded enemy boat near its own dock
	ALFBoat* PickDiveBoat() const;            // N3 20: loaded crewless enemy boat within ThiefDiveRange
	bool IsThief() const { return OwnBoatRecoverWeight > 1.f; }
	/** -LFLegacyAI: switch the N3 20 behaviour off (A/B test rounds against the pre-N3 AI). */
	const bool bLegacyAI = FParse::Param(FCommandLine::Get(), TEXT("LFLegacyAI"));
	ALFBoat* PickCrewlessBoat() const;
	ALFBoat* PickRecoverBoat() const;
	void ApplyPersonality(const ULFCaptainPersonality* P);
	void Decide();
	void Execute(float DeltaSeconds);
	void DriveToward(const FVector& Point, float Throttle);
	/** T04 stage 3: arrive at Point - speed falls with distance (station keeping inside HoldRadius, reverse to brake). */
	void DriveTo(const FVector& Point, float HoldRadius, float MinApproachSpeed, bool bAvoidShore);
	float AvoidShore(float Steer) const;
	void FaceAndSwing(ALFBrawler* Target);
	void WalkToward(const FVector& Point);

	ELFCaptainState State = ELFCaptainState::Idle;
	ELFCaptainState PendingState = ELFCaptainState::Idle;
	TWeakObjectPtr<ALFBrawler> Target;
	TWeakObjectPtr<ALFBrawler> PendingTarget;
	TWeakObjectPtr<ALFBoat> GoalBoat;      // RecoverBoat goal, or the crew-less boat we're ramming
	FVector GoalPoint = FVector::ZeroVector; // GoFish spot / ReturnToDock bank
	double ChaseSince = -1.0;              // stage 3: start of the current Chase/Ram run
	double NoChaseUntil = -1.0;
	double NextDecision = 0.0;
	double ApplyAt = -1.0;
	double LastSwing = -100.0;
	double FleeUntil = -1.0;
	double LastInReach = -100.0;           // fight hysteresis
	double DefendSince = -1.0;             // N3 20: defending the spot since (-1 = not)
	double NoDefendUntil = -1.0;
	TWeakObjectPtr<ALFBrawler> DefendTarget;
	FString PersonalityName;
};
