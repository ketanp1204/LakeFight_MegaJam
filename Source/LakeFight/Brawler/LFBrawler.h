#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LFBrawler.generated.h"

class ALFBoat;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;
class UStaticMeshComponent;
class UStaticMesh;
class ULFCaptainPersonality;
struct FInputActionValue;

UENUM(BlueprintType)
enum class ELFBrawlerState : uint8
{
	None,
	Standing,   // InBoat, free to walk inside the boat (based on the hull)
	Driving,    // InBoat, attached at the driver seat; the PlayerController possesses the boat
	Swimming,   // T02 step 3
	Climbing,   // T02 step 3
	Ragdoll     // knocked overboard: flying/tumbling, then in the water
};

UENUM(BlueprintType)
enum class ELFTeam : uint8
{
	Red,
	Blue,
	Green,
	Yellow
};

/** What the anim instance needs from the brawler each frame. SwingPhase: 0 idle, 1 charging, 2 swinging, 3 recover. */
struct FLFBrawlerAnimView
{
	ELFBrawlerState State = ELFBrawlerState::None;
	uint8 SwingPhase = 0;
	float SwingAlpha = 0.f;
	bool bStunned = false;
	float ClimbAlpha = 0.f;
	float DriveThrottle = 0.f; // spooled throttle of the boat we drive
	float BoatSpeed = 0.f;     // cm/s
};

UENUM()
enum class ELFSwingPhase : uint8
{
	Idle,
	Charging,   // holding Swing: wound up
	Swinging,   // the arc; sweeps for hits
	Recover
};

/**
 * T02 brawler: a character standing IN a physics boat.
 * Standing: the CMC walks on the hull (movement base = HullMesh) but the capsule is QUERY-ONLY, so it never pushes or
 * weighs on any boat (no physics contact; bEnablePhysicsInteraction off). A separate, clamped "fake weight" torque tilts
 * the boat gently toward where the brawler stands. Can't walk off ledges or step over the gunwale. The mesh leans with
 * the boat (procedural sway). Driving: attached at the boat's driver seat and the PlayerController possesses the boat.
 * Paddle (step 2): tap = light swing, hold = charged swing; sphere sweeps along the blade arc; knockback + stun; strong or
 * near-gunwale hits knock the victim overboard. Hitting another boat's hull rocks it.
 */
UCLASS()
class LAKEFIGHT_API ALFBrawler : public ACharacter
{
	GENERATED_BODY()

public:
	ALFBrawler();

	/** Crew. Same team = friends (AI won't target them). Player = Red. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LF|Brawler")
	ELFTeam Team = ELFTeam::Red;

	/** AI personality (behaviour + hat / face piece). Read by ALFCaptainAI; its look overrides HatOverride / FacePiece. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LF|Brawler")
	TObjectPtr<ULFCaptainPersonality> Personality;

	/** Take this brawler out of the fight (lf.ai.spectate): hidden, untargetable, no collision. */
	void SetSpectating();

	// --- Crew look (T02 step 5) ---
	/** Hat mesh override (personality). Null = SM_LF_Hat_Bucket_<Team>. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LF|Look")
	TObjectPtr<UStaticMesh> HatOverride;

	/** Face piece (beard / moustache) on the head, or null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LF|Look")
	TObjectPtr<UStaticMesh> FacePiece;

	/** Paddle absolute scale (slightly oversized reads as comedy on a 135 cm brawler). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0.1"))
	float PaddleScale = 0.85f;

	/** Grip points in the paddle's local space (cm, before scale; origin = T-grip, blade along +X). Stand-ins for Grip_R / Grip_L
	 *  sockets on SM_LF_Paddle (static-mesh sockets can't be added through MCP). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle")
	FVector GripROffset = FVector(4.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle")
	FVector GripLOffset = FVector(59.f, 0.f, 0.f);

	/** IK targets for both hands on the paddle (world). False when the paddle isn't in the hands (stowed / ragdoll). */
	bool GetPaddleGripTargets(FVector& OutRight, FVector& OutLeft) const;

	void GetAnimView(FLFBrawlerAnimView& Out) const;

	// --- Boat ---
	/** Player-controlled brawlers without a boat spawn one of these under them on possession. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Boat")
	TSubclassOf<ALFBoat> HomeBoatClass;

	/** Placed brawlers (dummies) stand in this boat at BeginPlay. */
	UPROPERTY(EditAnywhere, Category="LF|Boat")
	TObjectPtr<ALFBoat> StartBoat;

	/** Fraction of the boat's roll/pitch the mesh leans with (0 = stays upright). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Boat", meta=(ClampMin="0", ClampMax="1.5"))
	float SwayScale = 0.7f;

	/** How fast the lean follows the boat (1/s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Boat", meta=(ClampMin="0.1"))
	float SwayFollowSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Boat", meta=(ClampMin="0"))
	float BoatWalkSpeed = 180.f;

	/** Step height while in a boat - below the ~40 cm floor-to-gunwale so you can't step out. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Boat", meta=(ClampMin="0"))
	float BoatStepHeight = 12.f;

	// --- Fake weight tilt (Standing only; pure torque, no added mass) ---
	/** Pretend weight (kg) whose torque about the hull's centre of mass tilts the boat toward the brawler. 0 = off. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Brawler", meta=(ClampMin="0"))
	float FakeWeightKg = 120.f;

	/** The torque fades out as the boat's lean toward the brawler reaches this (deg): it can never push past it. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Brawler", meta=(ClampMin="0", ClampMax="15"))
	float MaxFakeTiltDeg = 4.f;

	/** How fast the weight follows the brawler's position (1/s); also how fast it fades when driving/overboard. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Brawler", meta=(ClampMin="0.1"))
	float FakeTiltFollowSpeed = 3.f;

	// --- Paddle (step 2) ---
	/** Hold time (s) below which a release is a light swing. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float ChargeMinTime = 0.2f;

	/** Hold time (s) for a full-strength charged swing. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0.05"))
	float ChargeFullTime = 0.8f;

	/** Strength (0..1) of a light swing; charged swings go from ChargedMinStrength to 1. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0", ClampMax="1"))
	float LightStrength = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0", ClampMax="1"))
	float ChargedMinStrength = 0.55f;

	/** Swing arc duration (s) for light / full-charge swings. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0.05"))
	float LightSwingTime = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0.05"))
	float ChargedSwingTime = 0.3f;

	/** Blade sweep sphere radius (cm). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="1"))
	float PaddleHitRadius = 28.f;

	/** Knockback (cm/s) at strength 0 and 1 for a hit that stays in the boat. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float KnockbackMin = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float KnockbackMax = 320.f;

	/** At or above this strength the victim goes overboard (charged hits). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0", ClampMax="1"))
	float OverboardStrength = 0.75f;

	/** Victims standing within this distance of the side (cm from the hull centreline beyond which = "near the gunwale") go
	 *  overboard from any hit. The floor is ~36 cm each side of the centreline. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float GunwaleNearY = 20.f;

	/** Stun (s) at strength 0 and 1: no moving or swinging. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float StunMin = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float StunMax = 1.0f;

	/** Velocity change (cm/s) at the hit point when the paddle hits another boat's hull, at strength 1. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float HullHitImpulse = 120.f;

	// --- Swim / climb / steal (step 3) ---
	/** Swim speed (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0"))
	float SwimSpeed = 120.f;

	/** Capsule centre this far (fraction of half-height) below the water surface: head and shoulders out. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0", ClampMax="1"))
	float SwimSubmerge = 0.5f;

	/** Forward lean of the mesh at full swim speed (deg) - placeholder swim pose. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0", ClampMax="90"))
	float SwimLeanDeg = 45.f;

	/** A boat moving faster than this (cm/s) through a swimmer runs him over (small ragdoll launch); slower it just pushes him aside. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0"))
	float RunOverSpeed = 250.f;

	/** Grabbing a boat moving faster than ClimbSlowSpeed (cm/s) makes the climb take ClimbTimeMoving. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0"))
	float ClimbSlowSpeed = 150.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0.1"))
	float ClimbTimeMoving = 1.2f;

	/** Above this boat speed (cm/s) you HANG on the gunwale and get towed until it slows (or a hit / Interact lets you go). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0"))
	float HangSpeed = 350.f;

	/** Can grab a hull when within this distance (cm) of its gunwale outline. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0"))
	float GrabRange = 150.f;

	/** Climb duration (s): rise at the side, hop over the rail, land inside. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0.1"))
	float ClimbTime = 0.8f;

	/** How far above the landing spot (cm, capsule centre) the climb goes before dropping in - clears the rail. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0"))
	float ClimbOverHeight = 35.f;

	/** Interact drives/steals only within this distance (cm, horizontal) of the driver seat. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0"))
	float DriveReach = 80.f;

	/** AI-less dummies (no controller) swim back to their home boat and climb in after this delay (s). */
	UPROPERTY(EditAnywhere, Category="LF|Swim")
	bool bAutoReturnHome = true;

	UPROPERTY(EditDefaultsOnly, Category="LF|Swim", meta=(ClampMin="0"))
	float AutoReturnDelay = 0.6f;

	// --- Overboard ragdoll (step 2.1) ---
	/** Horizontal launch speed (cm/s) away from the hitter for the weakest (light/near-gunwale) and strongest (full charge) hits. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float OverboardHorizSpeedMin = 230.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float OverboardHorizSpeedMax = 320.f;

	/** Small upward launch speed (cm/s) at strength 0 / 1. ~200 cm/s = ~20 cm of rise. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float OverboardUpSpeedMin = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float OverboardUpSpeedMax = 240.f;

	/** Extra horizontal speed (cm/s) on the upper body so he topples over the rail while the legs catch it. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float OverboardToppleSpeed = 160.f;

	/** Ragdoll body mass multiplier: keeps a flailing body from shoving a 180 kg hull while it can still touch it. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0.01", ClampMax="1"))
	float RagdollMassScale = 0.25f;

	/** The ragdoll collides with hulls this long (s) - the rail trips him - then ignores boats entirely. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float RagdollHullCollideTime = 0.3f;

	/** Buoyancy (cm/s^2 at >= 25 cm depth) for pelvis/chest/head; other bodies get WaterLimbBuoyancyScale of it. ~980 = neutral. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float WaterBuoyancyAccel = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0", ClampMax="2"))
	float WaterLimbBuoyancyScale = 0.6f;

	/** Velocity damping in the water (1/s) for every submerged body: limp, quickly settling bob. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float WaterDamping = 4.f;

	/** Limp float time (s) before recovering; recovers earlier if the pelvis is nearly still (FloatStillSpeed cm/s) after 0.6 s. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float FloatTime = 1.3f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float FloatStillSpeed = 15.f;

	/** Ragdoll -> upright blend (s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0.01"))
	float RecoverBlendTime = 0.3f;

	/** Overboard splash size relative to a hit splash. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Overboard", meta=(ClampMin="0"))
	float OverboardSplashScale = 1.8f;


	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle")
	TObjectPtr<UNiagaraSystem> SplashSystem;

	// --- Hit feedback (REAL time, like the boat camera in T01.3) ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0.01", ClampMax="1"))
	float HitstopDilation = 0.05f;

	/** Hitstop (real s) at strength 0 and 1. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float HitstopMinTime = 0.04f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float HitstopMaxTime = 0.09f;

	/** Camera shake: one decaying sine, restarted per hit. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0.01"))
	float ShakeDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float ShakeAmplitude = 8.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float ShakeFrequency = 14.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0"))
	float FovPunchDeg = 4.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Paddle", meta=(ClampMin="0.01"))
	float FovPunchTime = 0.25f;

	// --- Camera (third person, frames brawler + boat; real-time lag like the boat camera) ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0", ClampMax="1"))
	float CameraBoatFrame = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0.1"))
	float CameraLagSpeed = 6.f;

	/** The camera stays at least this far (cm) above the water (limits looking up from low, e.g. while swimming). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0"))
	float CameraWaterClearance = 40.f;

	// --- Input (template assets for move/look/jump; Interact/Swing are runtime actions in LF::Input) ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputMappingContext> MouseLookMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> JumpAction;

	// --- API ---
	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	ELFBrawlerState GetBrawlerState() const { return State; }

	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	ALFBoat* GetBoat() const { return Boat.Get(); }

	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	bool IsStunned() const;

	/** Put the brawler standing on the boat floor at the driver seat spot. */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	void StandInBoat(ALFBoat* InBoat);

	/** Standing -> Driving if the boat's seat is empty. The PlayerController possesses the boat. */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	bool TryDrive();

	/** Driving -> Standing; the PlayerController possesses the brawler again. */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	void LeaveDriving();

	/** Debug console: swing now with this strength (0..1, <0 = light). Works while standing. */
	UFUNCTION(Exec)
	void LFSwing(float Strength = -1.f);

	/** Debug console: throw yourself overboard to the right (Side 1) or left (-1) - for testing swim/climb/steal. */
	UFUNCTION(Exec)
	void LFOverboard(float Side = 1.f);

	/** Debug console: set the camera's yaw/pitch relative to the brawler's facing (for screenshots). */
	UFUNCTION(Exec)
	void LFCam(float YawOffset = 180.f, float Pitch = -15.f);

	/** Debug console: toggle drive/stand (same as Interact). */
	UFUNCTION(Exec)
	void LFDrive();

	// --- AI-ready actions (T03 AI drives brawlers through these; the player input maps onto the same functions) ---
	/** While swimming, swim toward this world point until StopSwim / arrival (30 cm). */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	void StartSwimTo(FVector Target);

	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	void StopSwim();

	/** Swimming: grab the nearest hull within GrabRange and climb in (side nearest you). */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	bool TryGrabNearestHull();

	/** Swimming: grab this specific hull if within GrabRange. */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	bool TryGrabHull(ALFBoat* Hull);

	/** Standing near an empty driver seat: drive (stealing if it isn't your home boat). */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	bool TryDriveBoat() { return TryDrive(); }

	/** Swing the paddle now: Strength < 0 = light, else charged strength 0..1. */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	void Swing(float Strength = -1.f);

	/** Nearest grabbable hull within GrabRange (null if none). */
	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	ALFBoat* FindGrabbableHull() const;

	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	ALFBoat* GetHomeBoat() const { return HomeBoat.Get(); }

	/** True when the paddle is idle and we can swing (standing, not stunned). */
	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	bool CanSwing() const;

	/** Standing near our boat's gunwale (a light hit would knock us overboard). */
	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	bool IsNearGunwale() const;

	/** Number of paddle hits received in the last Window seconds. */
	int32 RecentHits(float Window) const;

	/** Debug text drawn above the head when lf.ai.debug is on (set by the AI). */
	FString DebugState;

	/** Thrown overboard by something other than a paddle (capsize): ragdoll away along Dir. Returns true if it happened. */
	bool KnockOverboard(const FVector& Dir, float Strength);

	/** Paddle hit received. Dir = horizontal direction from the attacker. Returns true if it knocked us overboard. */
	bool ReceivePaddleHit(ALFBrawler* Attacker, const FVector& Dir, float Strength);

	/** Audio/FX hook: our paddle hit a brawler (Victim) or a hull (Victim null). */
	UFUNCTION(BlueprintImplementableEvent, Category="LF|Brawler")
	void OnPaddleHit(ALFBrawler* Victim, float Strength, bool bOverboard);

	/** Audio/FX hook: we climbed aboard a boat. */
	UFUNCTION(BlueprintImplementableEvent, Category="LF|Brawler")
	void OnClimbAboard(ALFBoat* Aboard);

	/** Audio/FX hook: we took the seat of a boat that isn't ours. */
	UFUNCTION(BlueprintImplementableEvent, Category="LF|Brawler")
	void OnSteal(ALFBoat* Stolen);

	/** Audio/FX hook: a swim stroke while moving in the water (~every SwimStrokeInterval). */
	UFUNCTION(BlueprintImplementableEvent, Category="LF|Brawler")
	void OnSwimStroke();

	/** Audio/FX hook: we hit the water. */
	UFUNCTION(BlueprintImplementableEvent, Category="LF|Brawler")
	void OnSplashIn(FVector Location);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(VisibleAnywhere, Category="LF|Camera")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category="LF|Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Shoulder pivot the paddle swings around (procedural placeholder animation). */
	UPROPERTY(VisibleAnywhere, Category="LF|Paddle")
	TObjectPtr<USceneComponent> PaddlePivot;

	UPROPERTY(VisibleAnywhere, Category="LF|Look")
	TObjectPtr<UStaticMeshComponent> Vest;

	UPROPERTY(VisibleAnywhere, Category="LF|Look")
	TObjectPtr<UStaticMeshComponent> Hat;

	UPROPERTY(VisibleAnywhere, Category="LF|Look")
	TObjectPtr<UStaticMeshComponent> Face;

	/** SM_LF_Paddle: origin at the grip, blade along +X. */
	UPROPERTY(VisibleAnywhere, Category="LF|Paddle")
	TObjectPtr<UStaticMeshComponent> Paddle;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnInteract();
	void OnSwingPressed();
	void OnSwingReleased();

	void SetBoatMovementRules(bool bInBoat);
	void UpdateSway(float DeltaSeconds);
	void UpdateFakeWeight(float DeltaSeconds);
	void UpdatePaddle(float DeltaSeconds);
	void SweepPaddle();
	void UpdateRagdoll(float DeltaSeconds);
	void UpdateCamera(float RealDelta);
	void PlayHitFeedback(float Strength);
	void StartHitstop(float Strength);
	FVector GetBladePoint() const;

	ELFBrawlerState State = ELFBrawlerState::None;
	TWeakObjectPtr<ALFBoat> Boat;
	FQuat SwayQuat = FQuat::Identity;
	FRotator MeshBaseRotation = FRotator(0.f, -90.f, 0.f);
	FVector CameraPivot = FVector::ZeroVector;
	bool bCameraPivotInit = false;

	FVector FakeWeightOffset = FVector::ZeroVector; // smoothed horizontal offset from the hull CoM (cm)

	ELFSwingPhase SwingPhase = ELFSwingPhase::Idle;
	float SwingAge = 0.f;
	float SwingDuration = 0.2f;
	float SwingStrength = 0.f;
	double ChargeStartTime = 0.0;
	bool bSwingHit = false;      // one victim per swing
	bool bHullHit = false;       // one hull per swing
	FVector PrevBladePoint = FVector::ZeroVector;
	FRotator PaddleRot = FRotator::ZeroRotator;

	double StunUntil = -1.0;
	enum class ERagdollPhase : uint8 { Flying, Floating, Recovering, Upright };
	ERagdollPhase RagdollPhase = ERagdollPhase::Flying;
	double RagdollStart = 0.0;   // phase start (game s)
	bool bInWater = false;
	bool bRagdollIgnoresBoats = false;
	float RecoverAge = 0.f;
	float GunwaleZ = 0.f;        // victim boat's gunwale height at launch (for the apex log)
	float ApexZ = -1e9f;         // highest pelvis Z while flying
	double LaunchTime = 0.0;

	void StartRagdoll(const FVector& Out, float Strength);
	void ApplyCrewLook();
	void AttachAligned(USceneComponent* Comp, FName Socket, FName Bone, const FVector& ActorOffset, const FRotator& ActorRot);
	void UpdatePaddleStow();
	enum class EPaddleMount : uint8 { Hands, Back, Hand };
	EPaddleMount PaddleMount = EPaddleMount::Hands;
	float SwimStrokeAccum = 0.f;
	void EnterSwimming();
	void UpdateSwim(float DeltaSeconds);
	void UpdateClimb(float DeltaSeconds);
	void FinishClimb();
	void StandInBoatAt(ALFBoat* InBoat, const FVector& LocalSpot);
	void UpdatePrompt();
	float GrabDistanceTo(const ALFBoat* B, FVector2D* OutLocal = nullptr) const;

	TWeakObjectPtr<ALFBoat> HomeBoat;
	TArray<double> HitTimes; // recent paddle hits received (game s)
	float StandFallTime = 0.f;  // how long a Standing brawler has been falling (safety re-snap)
	FVector SwimTarget = FVector::ZeroVector;
	bool bHasSwimTarget = false;
	double SwimStartTime = 0.0;
	TWeakObjectPtr<ALFBoat> ClimbBoat;
	FVector ClimbStartLocal = FVector::ZeroVector;
	FVector ClimbTargetLocal = FVector::ZeroVector;
	float ClimbAge = 0.f;
	float ClimbDuration = 0.8f;
	bool bHanging = false;
	FVector ClimbHangLocal = FVector::ZeroVector;
	void UpdateWaterForces(float DeltaSeconds);
	void EndRagdoll();

	float ShakeAge = -1.f;
	float ShakeStrength = 0.f;
	float FovPunchAge = -1.f;
	float FovPunchStrength = 0.f;
	float BaseFov = 80.f;
};
