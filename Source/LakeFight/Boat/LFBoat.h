#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LFBoat.generated.h"

class UProceduralMeshComponent;
class UNiagaraSystem;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class ULFBuoyancyComponent;
class UInputMappingContext;
class UInputAction;
class UPhysicalMaterial;
class ALFBrawler;
struct FInputActionValue;

/**
 * Physics-simulated rowboat pawn. Root = SM_LF_Rowboat simulating with its own UCX convexes (origin at the waterline).
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
	/** Root + physics body: SM_LF_Rowboat simulating with its own 6 UCX convexes (what you see is what collides). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Boat")
	TObjectPtr<UStaticMeshComponent> HullMesh;

	/** Invisible cap over the open cockpit at gunwale height. Custom depth/stencil only; M_LF_Lake clips water behind it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Boat")
	TObjectPtr<UProceduralMeshComponent> WaterMask;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Boat")
	TObjectPtr<ULFBuoyancyComponent> Buoyancy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Camera")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LF|Camera")
	TObjectPtr<UCameraComponent> Camera;

	// --- Hull ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Hull", meta=(ClampMin="10"))
	float HullMassKg = 180.f;

	/** Centre of mass in hull-local space (cm; origin = waterline centre). Low = stable. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hull")
	FVector CenterOfMassLocal = FVector(0.f, 0.f, -15.f);

	/** Custom stencil value written by WaterMask (M_LF_Lake clips water where stencil == this and the water is behind the cap). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hull", meta=(ClampMin="1", ClampMax="255"))
	int32 WaterMaskStencil = 1;

	/** Custom stencil value written by the hull mesh. M_LF_Lake treats water in front of a hull as deep (no shallow tint, no
	 *  depth foam), so submerged hull edges never show through the water. Must match the material (2). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hull", meta=(ClampMin="1", ClampMax="255"))
	int32 HullStencil = 2;

	/** PM_LF_Hull. If unset, a runtime material is built from HullFriction / HullRestitution. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hull")
	TObjectPtr<UPhysicalMaterial> HullPhysMaterial;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hull", meta=(ClampMin="0"))
	float HullFriction = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hull", meta=(ClampMin="0", ClampMax="1"))
	float HullRestitution = 0.15f;

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

	/** Free-look while driving: mouse / right stick orbit the camera; after this long (real s) without look input it
	 *  eases back behind the boat with the normal lag. 0 = never recentre. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0"))
	float CameraRecenterDelay = 2.f;

	/** Free-look pitch limits (deg). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera")
	float CameraMinPitch = -60.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Camera")
	float CameraMaxPitch = 10.f;

	/** The camera stays at least this far (cm) above the water: looking up from low is limited so the lens never dips under. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0"))
	float CameraWaterClearance = 40.f;

	/** How fast the camera pivot follows the boat's position (1/s). Replaces the spring arm's built-in lag so it runs
	 *  on REAL (undilated) time: hitstop never freezes-then-snaps the camera. All camera feedback uses real time too. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0.1"))
	float CameraLocationLagSpeed = 5.f;

	// --- Hits: bump vs ram ---
	/** Contacts slower than this (cm/s along the contact normal) are resting/scraping: ignored, not logged. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float MinContactSpeed = 30.f;

	/** Closing speed (cm/s, along the contact normal) where a Bump becomes a Ram. Bumps: physics only, no extras. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float RamMinSpeed = 150.f;

	/** Closing speed (cm/s) at which ram strength reaches 1 (linear from 0 at RamMinSpeed). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="1"))
	float RamFullSpeed = 600.f;

	/** Total extra shove at full strength (cm/s velocity change), split between target and rammer. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float RamShove = 250.f;

	/** Share of RamShove given to the target (along the contact normal, at the contact point). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0", ClampMax="1"))
	float TargetShoveShare = 0.7f;

	/** Share of RamShove given back to the rammer as recoil. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0", ClampMax="1"))
	float RammerRecoilShare = 0.3f;

	/** Fraction of its speed toward the target the rammer keeps after a full-strength ram. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0", ClampMax="1"))
	float RammerKeepSpeed = 0.25f;

	/** Upward kick on the target at full strength (cm/s): it lifts and slaps down. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float RamHeave = 120.f;

	/** Roll kick on the target at full strength (rad/s), tipping its deck away from the hit. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float RamRollKick = 1.0f;

	/** Nose-up pitch kick on the target at full strength (rad/s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float RamPitchKick = 0.5f;

	/** Cap on any comedic linear velocity change (cm/s). Rams rock but never launch. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float MaxHitImpulse = 250.f;

	/** Cap on any comedic angular velocity change (rad/s). Rams rock but never flip. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float MaxHitRoll = 1.2f;

	/** Per-pair cooldown (REAL s). Every contact of the pair refreshes it, so one sustained contact = one bump/ram;
	 *  the pair can register again only after being apart this long. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float PairHitCooldown = 0.5f;

	/** Hitstop: global time dilation during the freeze frame. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0.01", ClampMax="1"))
	float HitstopDilation = 0.05f;

	/** Hitstop length in REAL seconds at strength 0 and 1. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitstopMinTime = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float HitstopMaxTime = 0.10f;

	/** Player camera shake: ONE smooth decaying sine (real time). A new ram restarts it (never stacks).
	 *  Duration (s), vertical amplitude at full strength (cm, scaled by ram strength), frequency (Hz). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0.01"))
	float ShakeDuration = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float ShakeAmplitude = 12.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float ShakeFrequency = 13.f;

	/** Sideways share of the shake (0 = purely vertical). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0", ClampMax="1"))
	float ShakeHorizontalShare = 0.15f;

	/** FOV punch at full strength (deg), easing out over FovPunchTime (REAL s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float FovPunchDeg = 5.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0.01"))
	float FovPunchTime = 0.3f;

	/** Gamepad rumble at full strength (0..1) and duration (s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0", ClampMax="1"))
	float RumbleIntensity = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Hit", meta=(ClampMin="0"))
	float RumbleDuration = 0.2f;

	/** NS_LF_Splash, spawned at the contact point with user float "Strength". If unset, a debug sphere is drawn instead. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit")
	TObjectPtr<UNiagaraSystem> SplashSystem;

	/** Draw a debug sphere at splash points when SplashSystem is unset. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Hit")
	bool bDebugSplash = true;

	// --- Input ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Mouse look while driving (template IMC_MouseLook / IA_MouseLook - same feel as on foot). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputMappingContext> MouseLookMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> MouseLookAction;

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

	/** Splash hook: called on rams (later paddles/falls). Default spawns SplashSystem (or a debug sphere). */
	UFUNCTION(BlueprintNativeEvent, Category="LF|Boat")
	void OnSplash(FVector Location, float Strength);

	/** Called on BOTH boats of a ram (Ram tier only). Empty - for audio later. */
	UFUNCTION(BlueprintImplementableEvent, Category="LF|Boat")
	void OnRamImpact(float Strength);

	/** Velocity before this frame's physics step (hit events fire after the solver has already separated the bodies). */
	FVector GetPreStepVelocity() const { return PreStepVelocity; }

	/** Signed speed along the hull's (flattened) forward axis, cm/s. */
	UFUNCTION(BlueprintPure, Category="LF|Boat")
	float GetForwardSpeed() const;

	/** Current rudder authority 0..1 (after reverse scaling). */
	UFUNCTION(BlueprintPure, Category="LF|Boat")
	float GetTurnAuthority() const;

	// --- Crew (T02) ---
	/** Driver seat on the floor (local cm, floor top ~ -9 cm). A driver is attached here; leaving the seat stands here. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Crew")
	FVector DriverSeatLocal = FVector(-55.f, 0.f, -9.f);

	UFUNCTION(BlueprintPure, Category="LF|Crew")
	ALFBrawler* GetDriver() const { return Driver.Get(); }

	UFUNCTION(BlueprintPure, Category="LF|Crew")
	bool HasDriver() const { return Driver.IsValid(); }

	/** Losing the driver (knocked out, left the seat) clears the drive input so an empty boat coasts to a stop
	 *  (an AI's last throttle/steer kept the boat circling - the AI never possesses the boat, so UnPossessed didn't reset it). */
	void SetDriver(ALFBrawler* InDriver)
	{
		Driver = InDriver;
		if (!InDriver)
		{
			ThrottleInput = 0.f;
			SteerInput = 0.f;
		}
	}

	/** World position of the driver seat on the floor. */
	FVector GetDriverSeatWorld() const;

	/** Signed horizontal distance (cm) from WorldPos to the hull's outer gunwale outline (RowboatPlan, boat yaw/pitch/roll
	 *  via the hull transform); negative = inside the outline. OutLocalNearest = nearest outline point in hull-local cm. */
	float GunwaleDistance(const FVector& WorldPos, FVector2D& OutLocalNearest) const;

	UStaticMeshComponent* GetHull() const { return HullMesh; }

	// --- Capsize (T03.1) ---
	/** Tilt beyond this (deg) for CapsizeHoldTime = Capsized: everyone aboard goes overboard, no driving. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Capsize", meta=(ClampMin="10", ClampMax="180"))
	float CapsizeRollDeg = 70.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Capsize", meta=(ClampMin="0"))
	float CapsizeHoldTime = 0.3f;

	/** Time (s) capsized before the boat rights itself. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Capsize", meta=(ClampMin="0"))
	float CapsizedTime = 3.f;

	/** Righting: torque toward upright over RightingTime (s). Accel per radian of tilt (1/s^2) and angular damping (1/s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Capsize", meta=(ClampMin="0.1"))
	float RightingTime = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Capsize", meta=(ClampMin="0"))
	float RightingAccel = 14.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Capsize", meta=(ClampMin="0"))
	float RightingDamping = 4.f;

	UFUNCTION(BlueprintPure, Category="LF|Capsize")
	bool IsCapsized() const { return bCapsized; }

	/** Tilt of the hull from upright (deg). */
	float GetTiltDeg() const;

	/** Debug: kick the boat over (lf.debug.capsize). */
	void DebugCapsizeKick();

	/** Throttle the motor actually delivers (-1..1, spooled). */
	float GetThrottleOutput() const { return SpooledThrottle; }

	/** Debug console while driving (the controller possesses the boat): leave the seat (same as Interact). */
	UFUNCTION(Exec)
	void LFDrive() { OnInteract(); }

	/** Drive input from an AI captain (same path as the player's Throttle/Steer actions): throttle -1..1, steer -1..1. */
	UFUNCTION(BlueprintCallable, Category="LF|Crew")
	void SetDriveInput(float Throttle, float Steer)
	{
		ThrottleInput = FMath::Clamp(Throttle, -1.f, 1.f);
		SteerInput = FMath::Clamp(Steer, -1.f, 1.f);
	}

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void UnPossessed() override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void OnThrottle(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnThrottleReleased();
	void OnSteerReleased();
	void OnInteract();
	struct FEnhancedInputActionValueBinding* ThrottleValue = nullptr; // valid while possessed by a player
	struct FEnhancedInputActionValueBinding* SteerValue = nullptr;
	void OnLook(const FInputActionValue& Value);
	double LastLookRealTime = -100.0;

	void UpdateCamera(float DeltaSeconds);
	void BuildWaterMask();
	void ResolveContact(ALFBoat* Target, const FVector& TargetDir, float Closing, const FVector& Contact, double Now);
	void ApplyRamTo(ALFBoat* Target, const FVector& TargetDir, const FVector& Contact, float Strength);
	void PlayRamFeedback(float Strength);
	void StartHitstop(float Strength);

	UFUNCTION()
	void OnHullHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void LogSettle();

	float ThrottleInput = 0.f;   // raw input
	float SpooledThrottle = 0.f; // what the motor actually delivers
	float SteerInput = 0.f;
	FVector PreStepVelocity = FVector::ZeroVector;
	FRotator CameraRot = FRotator::ZeroRotator;
	FVector CameraPivot = FVector::ZeroVector;
	float BaseFov = 80.f;
	float ShakeAge = -1.f;     // real s since the current shake started; < 0 = idle
	float ShakeStrength = 0.f;
	float FovPunchAge = -1.f;  // real s; < 0 = idle
	float FovPunchStrength = 0.f;

	/** Per-pair contact bookkeeping (real time). Both boats keep a mirrored entry for each other. */
	struct FPairContact
	{
		double LastContact = -1e9; // real s of the most recent contact event of any speed
		double LastResolved = -1e9;
		int32 Suppressed = 0;      // contacts at >= MinContactSpeed swallowed by the cooldown since the last resolve
	};
	TMap<TWeakObjectPtr<ALFBoat>, FPairContact> PairContacts;

	TWeakObjectPtr<ALFBrawler> Driver;

	bool bCapsized = false;
	float CapsizeTimer = 0.f;
	double CapsizedAt = 0.0;
	void UpdateCapsize(float DeltaSeconds);
	void Capsize();
};
