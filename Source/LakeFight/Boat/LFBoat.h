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
struct FInputActionValue;

/**
 * Physics-simulated rowboat pawn. Box hull = physics body (root); VisualMesh is cosmetic only.
 * Thrust is applied at the stern below the waterline (bow lifts under power), turning applies yaw torque,
 * and an outward lean torque at speed. Boat-vs-boat hits add a shove + extra roll for comedy.
 * Unpossessed boats simply float (they are valid ram targets).
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

	// --- Drive ---
	/** Forward thrust as acceleration (cm/s^2). Top speed ~= ThrustAccel / Buoyancy.ForwardDrag. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Drive", meta=(ClampMin="0"))
	float ThrustAccel = 330.f;

	/** Reverse thrust as a fraction of forward thrust. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Drive", meta=(ClampMin="0", ClampMax="1"))
	float ReverseThrustScale = 0.5f;

	/** Where thrust is applied (local cm). Behind and below the centre of mass, so the bow lifts under power. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Drive")
	FVector ThrustPoint = FVector(-140.f, 0.f, -25.f);

	/** Thrust only works while the thrust point is no more than this far above the water (cm). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Drive", meta=(ClampMin="0"))
	float ThrustMaxAirGap = 10.f;

	/** Yaw angular accel at full steer and speed (deg/s^2). Steady turn rate ~= this / Buoyancy.AngularDrag. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Drive", meta=(ClampMin="0"))
	float TurnAccelDeg = 140.f;

	/** Turn strength when stationary, as a fraction of full (rowboats can pivot). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Drive", meta=(ClampMin="0", ClampMax="1"))
	float StationaryTurnScale = 0.45f;

	/** Speed (cm/s) at which turning and lean reach full strength. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Drive", meta=(ClampMin="1"))
	float FullEffectSpeed = 500.f;

	/** Outward roll accel when turning at full speed (deg/s^2). 0 = no lean. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Drive", meta=(ClampMin="0"))
	float LeanAccelDeg = 120.f;

	// --- Hits ---
	/** Minimum closing speed (cm/s) for a boat-vs-boat hit to trigger the comedy response. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float MinHitSpeed = 120.f;

	/** Closing speed is clamped to this (cm/s) before scaling. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="1"))
	float MaxHitSpeed = 900.f;

	/** Extra shove away from the other boat, as velocity change per unit of closing speed. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitShoveScale = 0.5f;

	/** Small upward pop, as velocity change per unit of closing speed. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitPopScale = 0.12f;

	/** Extra roll away from the hit (rad/s angular velocity change per 100 cm/s closing speed). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitRollScale = 0.45f;

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

	UFUNCTION(BlueprintPure, Category="LF|Boat")
	float GetForwardSpeed() const;

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

	UFUNCTION()
	void OnHullHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void LogSettle();

	float ThrottleInput = 0.f;
	float SteerInput = 0.f;
	float LastHitTime = -1000.f;
	FVector PreStepVelocity = FVector::ZeroVector;
};
