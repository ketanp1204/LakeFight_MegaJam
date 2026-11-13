#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "LFCaptainAI.generated.h"

class ALFBrawler;
class ALFBoat;

UENUM(BlueprintType)
enum class ELFCaptainState : uint8
{
	Idle,
	Chase,
	Ram,
	Fight,
	Steal,
	RecoverBoat,
	Flee
};

/**
 * T03 AI captain. Possesses an ALFBrawler and plays with the player's rules: it only uses the brawler/boat actions
 * (StartSwimTo, TryGrabHull, TryDriveBoat, Swing, AddMovementInput, ALFBoat::SetDriveInput) - no teleports, no extra forces.
 * Decisions are re-evaluated every DecisionInterval (with a small reaction delay); steering/aim run every tick.
 */
UCLASS()
class LAKEFIGHT_API ALFCaptainAI : public AAIController
{
	GENERATED_BODY()

public:
	ALFCaptainAI();

	/** Re-evaluate the state this often (s). */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0.1"))
	float DecisionInterval = 0.5f;

	/** A new decision takes effect after a random delay in this range (s) - beatable. */
	UPROPERTY(EditAnywhere, Category="LF|AI")
	FVector2D ReactionDelay = FVector2D(0.15f, 0.35f);

	/** Random yaw error (deg) applied when aiming a paddle swing. */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float AimErrorDeg = 12.f;

	/** Swing when the target is within this horizontal distance (cm). */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float SwingRange = 165.f;

	/** Minimum time between swings (s). */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float SwingCooldown = 1.1f;

	/** Leave the seat to fight when our hull is within this distance (cm, gunwale to target) of an enemy standing in a boat. */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float BoardRange = 120.f;

	/** Ram when lined up within this angle (deg) and closer than RamRange (cm). */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float RamAngleDeg = 20.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float RamRange = 1500.f;

	/** Chase lookahead on the target's velocity (s). */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float LookaheadTime = 0.8f;

	/** Shore avoidance trace length (cm) + speed x this (s). */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float AvoidBaseLength = 500.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float AvoidSpeedTime = 1.5f;

	/** Flee for FleeTime (s) after FleeHits paddle hits within FleeWindow (s). */
	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="1"))
	int32 FleeHits = 2;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float FleeWindow = 3.f;

	UPROPERTY(EditAnywhere, Category="LF|AI", meta=(ClampMin="0"))
	float FleeTime = 4.f;

	UFUNCTION(BlueprintPure, Category="LF|AI")
	ELFCaptainState GetCaptainState() const { return State; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	ALFBrawler* Me() const;
	ALFBrawler* PickTarget() const;
	ALFBoat* PickRecoverBoat() const;
	void Decide();
	void Execute(float DeltaSeconds);
	void DriveToward(const FVector& Point, float Throttle);
	float AvoidShore(float Steer) const;
	void FaceAndSwing(ALFBrawler* Target);
	void WalkToward(const FVector& Point);

	ELFCaptainState State = ELFCaptainState::Idle;
	ELFCaptainState PendingState = ELFCaptainState::Idle;
	TWeakObjectPtr<ALFBrawler> Target;
	TWeakObjectPtr<ALFBrawler> PendingTarget;
	TWeakObjectPtr<ALFBoat> GoalBoat;
	double NextDecision = 0.0;
	double ApplyAt = -1.0;
	double LastSwing = -100.0;
	double FleeUntil = -1.0;
};
