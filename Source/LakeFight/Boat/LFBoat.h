#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LFBoat.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class ULFBuoyancyComponent;
class UInputMappingContext;
class UInputAction;
class UPhysicalMaterial;
struct FInputActionValue;

/**
 * Physics-simulated rowboat pawn. Box hull = physics body (root, fitted to SM_LF_Rowboat, origin at the waterline);
 * VisualMesh is cosmetic only (no collision).
 * Handling: spooled thrust at the stern below the waterline (bow lift + squat), steering as a LATERAL rudder force
 * at the stern whose authority scales with speed through the water (stern swings out, boat pivots near the bow,
 * yaw carries on after release), outward lean in turns. Boat-vs-boat hits add a clamped shove + roll for comedy.
 */
UCLASS()
class LAKEFIGHT_API ALFBoat : public APawn
{
	GENERATED_BODY()

public:
	ALFBoat();

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Boat")
	TObjectPtr<UBoxComponent> Hull;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Boat")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Boat")
	TObjectPtr<ULFBuoyancyComponent> Buoyancy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Camera")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Camera")
	TObjectPtr<UCameraComponent> Camera;

	// --- Hull ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Hull", meta=(ClampMin="10"))
	float HullMassKg = 250.f;

	/** Centre of mass offset (cm, local). Lower = more stable against rolling. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hull")
	FVector CenterOfMassOffset = FVector(0.f, 0.f, -10.f);

	/** PM_LF_Hull. If unset, a runtime material is built from HullFriction / HullRestitution. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hull")
	TObjectPtr<UPhysicalMaterial> HullPhysMaterial;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hull", meta=(ClampMin="0"))
	float HullFriction = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hull", meta=(ClampMin="0", ClampMax="1"))
	float HullRestitution = 0.35f;

	// --- Handling: thrust ---
	/** Forward thrust as acceleration (cm/s^2). Top speed ~= ThrustAccel / Buoyancy.ForwardDrag. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float ThrustAccel = 330.f;

	/** Reverse thrust as a fraction of forward thrust. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0", ClampMax="1"))
	float ReverseThrustScale = 0.4f;

	/** Seconds for thrust to spool up to the requested throttle. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0.01"))
	float ThrottleRiseTime = 0.6f;

	/** Seconds for thrust to fall back to zero / a lower throttle. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0.01"))
	float ThrottleFallTime = 0.4f;

	/** Where thrust is applied (local cm). Behind and below the centre of mass, so the bow lifts under power. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling")
	FVector ThrustPoint = FVector(-140.f, 0.f, -20.f);

	/** Thrust/rudder only work while their point is no more than this far above the water (cm). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float ThrustMaxAirGap = 10.f;

	/** Extra nose-up pitch accel (deg/s^2) while spooled thrust is high and speed is still low: the stern squats. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float SquatAccelDeg = 60.f;

	// --- Handling: rudder ---
	/** Rudder point (local cm): behind the centre, below the waterline. Steering pushes it sideways. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling")
	FVector RudderPoint = FVector(-130.f, 0.f, -20.f);

	/** Lateral rudder force at full steer and full authority, as acceleration (cm/s^2). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float RudderForceAccel = 85.f;

	/** Forward speed through the water (cm/s) at which the rudder reaches full authority. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="1"))
	float RudderFullSpeed = 400.f;

	/** Rudder authority floor when (nearly) stopped, so you can still nudge the bow round. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0", ClampMax="1"))
	float MinTurnAuthority = 0.15f;

	/** Authority multiplier when moving backwards (steering also inverts naturally). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0", ClampMax="1"))
	float ReverseRudderScale = 0.5f;

	/** Outward roll accel when turning at full authority (deg/s^2). 0 = no lean. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float LeanAccelDeg = 120.f;

	// --- Camera ---
	/** Camera arm pitch (deg). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera")
	float CameraPitch = -16.f;

	/** How fast the camera follows the boat's yaw (1/s). Low = boat visibly swings under the camera in turns. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0.1"))
	float CameraYawLagSpeed = 2.0f;

	/** How fast the camera follows pitch (1/s). Higher than yaw. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0.1"))
	float CameraPitchLagSpeed = 8.0f;

	/** Fraction of the boat's pitch the camera follows (0 = horizon locked). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0", ClampMax="1"))
	float CameraPitchFollow = 0.25f;

	// --- Hits ---
	/** Minimum closing speed (cm/s) for a boat-vs-boat hit to trigger the comedy response. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float MinHitSpeed = 120.f;

	/** Closing speed is clamped to this (cm/s) before scaling. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="1"))
	float MaxHitSpeed = 900.f;

	/** Extra shove away from the other boat, as velocity change per unit of closing speed. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitShoveScale = 0.35f;

	/** Small upward pop, as velocity change per unit of closing speed. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitPopScale = 0.06f;

	/** Cap on the comedic shove+pop velocity change (cm/s). Keeps full-speed rams from launching boats. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float MaxHitImpulse = 250.f;

	/** Extra roll away from the hit (rad/s angular velocity change per 100 cm/s closing speed). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitRollScale = 0.3f;

	/** Cap on the comedic roll (rad/s angular velocity change). Rocks hard but never flips. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float MaxHitRoll = 1.2f;

	/** Seconds before the same boat can register another hit. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitCooldown = 0.35f;

	/** Draw a debug sphere at splash points (placeholder until the Niagara splash lands). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit")
	bool bDebugSplash = true;

	// --- Input ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Axis1D: +1 forward, -1 reverse. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> ThrottleAction;

	/** Axis1D: +1 right, -1 left. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> SteerAction;

	// --- Debug ---
	/** Log Z / pitch / roll this many seconds after BeginPlay (0 = off). Warns if not level at the waterline. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Debug", meta=(ClampMin="0"))
	float SettleLogDelay = 3.f;

	/** Splash hook: called on boat hits (and later paddles/falls). Default draws a debug sphere. */
	UFUNCTION(BlueprintNativeEvent, Category="LF|Boat")
	void OnSplash(FVector Location, float Strength);

	/** Velocity before this frame's physics step (hit events fire after the solver has already separated the bodies). */
	FVector GetPreStepVelocity() const { return PreStepVelocity; }

	/** Signed speed along the hull's (flattened) forward axis, cm/s. */
	UFUNCTION(BlueprintPure, Category="LF|Boat")
	float GetForwardSpeed() const;

	/** Current rudder authority 0..1 (after reverse scaling). */
	UFUNCTION(BlueprintPure, Category="LF|Boat")
	float GetTurnAuthority() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

protected:
	virtual void BeginPlay() override;

private:
	void OnThrottle(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnThrottleReleased();
	void OnSteerReleased();

	void UpdateCamera(float DeltaSeconds);

	UFUNCTION()
	void OnHullHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void LogSettle();

	float ThrottleInput = 0.f;   // raw input
	float SpooledThrottle = 0.f; // what the motor actually delivers
	float SteerInput = 0.f;
	float LastHitTime = -1000.f;
	FVector PreStepVelocity = FVector::ZeroVector;
	FRotator CameraRot = FRotator::ZeroRotator;
};
