#include "Boat/LFBoat.h"

#include "Water/LFWater.h"
#include "Water/LFBuoyancyComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "DrawDebugHelpers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Gunwale outline of SM_LF_Rowboat (B01.1). Mirrors PLAN / sheer() in Art/BlenderScripts/SM_LF_Rowboat.py,
	// inner top edge = (x*0.958, y*0.91, sheer(x) - 0.014). Metres, +X = bow. If the boat script changes, update this.
	const FVector2D RowboatPlan[] = {
		{-1.58, -.38}, {-1.27, -.57}, {-.75, -.65}, {-.15, -.67}, {.48, -.63}, {.98, -.45}, {1.35, -.20},
		{1.58, 0.}, {1.35, .20}, {.98, .45}, {.48, .63}, {-.15, .67}, {-.75, .65}, {-1.27, .57}, {-1.58, .38}};

	double RowboatSheer(double X)
	{
		const double Bow = FMath::Max(X / 1.58, 0.);
		const double Stern = FMath::Max(-X / 1.58, 0.);
		return .25 + .28 * Bow * Bow + .105 * Stern * Stern;
	}
}

ALFBoat::ALFBoat()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// Root = the rowboat mesh itself, simulating with its own UCX convex collision.
	HullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullMesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RowboatMesh(TEXT("/Game/LakeFight/Meshes/SM_LF_Rowboat.SM_LF_Rowboat"));
	if (RowboatMesh.Succeeded())
	{
		HullMesh->SetStaticMesh(RowboatMesh.Object);
	}
	HullMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	HullMesh->SetSimulatePhysics(true);
	HullMesh->SetNotifyRigidBodyCollision(true);
	HullMesh->SetLinearDamping(0.f);   // drag comes from ULFBuoyancyComponent
	HullMesh->SetAngularDamping(0.f);
	RootComponent = HullMesh;

	WaterMask = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMask"));
	WaterMask->SetupAttachment(HullMesh);
	WaterMask->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterMask->SetCastShadow(false);
	WaterMask->SetRenderInMainPass(false);
	WaterMask->SetRenderInDepthPass(false);
	WaterMask->SetRenderCustomDepth(true);
	WaterMask->bVisibleInReflectionCaptures = false;
	WaterMask->bVisibleInRayTracing = false;

	Buoyancy = CreateDefaultSubobject<ULFBuoyancyComponent>(TEXT("Buoyancy"));

	// Camera pivot position AND rotation are driven manually in UpdateCamera on REAL time (yaw lags more than pitch;
	// the spring arm's own lag would use dilated time and freeze-then-snap during hitstop).
	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(HullMesh);
	CameraArm->SetUsingAbsoluteLocation(true);
	CameraArm->SetUsingAbsoluteRotation(true);
	CameraArm->TargetArmLength = 850.f;
	CameraArm->SocketOffset = FVector(0.f, 0.f, 120.f);
	CameraArm->bEnableCameraLag = false;
	CameraArm->bEnableCameraRotationLag = false;
	CameraArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(80.f);

	AutoPossessAI = EAutoPossessAI::Disabled;
	// Never refuse to spawn a boat (PIE spawn failed with the UCX hull at the PlayerStart); buoyancy settles it.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
}

void ALFBoat::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildWaterMask(); // also in the editor, so the hull water mask is visible/checkable outside PIE
}

void ALFBoat::BuildWaterMask()
{
	// Fan of the gunwale outline (cm), both windings so it renders into custom depth from above and below.
	TArray<FVector> Verts;
	FVector Centre = FVector::ZeroVector;
	for (const FVector2D& P : RowboatPlan)
	{
		const FVector V(P.X * .958 * 100., P.Y * .91 * 100., (RowboatSheer(P.X) - .014) * 100.);
		Verts.Add(V);
		Centre += V;
	}
	const int32 N = Verts.Num();
	Centre /= N;
	Verts.Add(Centre);
	TArray<int32> Tris;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 j = (i + 1) % N;
		Tris.Append({ N, i, j, N, j, i });
	}
	WaterMask->CreateMeshSection(0, Verts, Tris, {}, {}, {}, {}, /*bCreateCollision*/ false);
	WaterMask->SetCustomDepthStencilValue(WaterMaskStencil);
}

void ALFBoat::BeginPlay()
{
	Super::BeginPlay();

	if (!HullMesh->GetStaticMesh())
	{
		UE_LOG(LogLF, Warning, TEXT("ALFBoat %s: HullMesh has no static mesh (SM_LF_Rowboat missing?) - no collision/physics body."), *GetName());
	}

	UPhysicalMaterial* PhysMat = HullPhysMaterial;
	if (!PhysMat)
	{
		PhysMat = NewObject<UPhysicalMaterial>(this, TEXT("PM_LF_Hull_Runtime"));
		PhysMat->Friction = HullFriction;
		PhysMat->Restitution = HullRestitution;
		PhysMat->bOverrideFrictionCombineMode = true;
		PhysMat->FrictionCombineMode = EFrictionCombineMode::Min;
		PhysMat->bOverrideRestitutionCombineMode = true;
		PhysMat->RestitutionCombineMode = EFrictionCombineMode::Max;
	}
	HullMesh->SetPhysMaterialOverride(PhysMat);

	// Explicit mass, and centre of mass placed at CenterOfMassLocal (the nudge is relative to the shape's own COM).
	HullMesh->SetMassOverrideInKg(NAME_None, HullMassKg, true);
	const FTransform& Xf = HullMesh->GetComponentTransform();
	const FVector ShapeCom = Xf.InverseTransformPosition(HullMesh->GetCenterOfMass());
	HullMesh->SetCenterOfMass(CenterOfMassLocal - ShapeCom);
	UE_LOG(LogLF, Display, TEXT("[hull] %s mass=%.0f kg shapeCOM=%s -> COM=%s"), *GetName(), HullMesh->GetMass(),
		*ShapeCom.ToCompactString(), *Xf.InverseTransformPosition(HullMesh->GetCenterOfMass()).ToCompactString());

	HullMesh->OnComponentHit.AddDynamic(this, &ALFBoat::OnHullHit);

	if (WaterMask->GetNumSections() == 0)
	{
		BuildWaterMask();
	}

	BaseFov = Camera->FieldOfView;
	CameraRot = FRotator(CameraPitch, GetActorRotation().Yaw, 0.f);
	CameraArm->SetWorldRotation(CameraRot);
	CameraPivot = GetActorLocation();
	CameraArm->SetWorldLocation(CameraPivot);

	if (SettleLogDelay > 0.f)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &ALFBoat::LogSettle, SettleLogDelay, false);
	}
}

float ALFBoat::GetForwardSpeed() const
{
	return float(FVector::DotProduct(HullMesh->GetPhysicsLinearVelocity(), GetActorForwardVector().GetSafeNormal2D()));
}

float ALFBoat::GetTurnAuthority() const
{
	const float V = GetForwardSpeed();
	const float Authority = FMath::Clamp(FMath::Abs(V) / RudderFullSpeed, MinTurnAuthority, 1.f);
	return V < 0.f ? Authority * ReverseRudderScale : Authority;
}

void ALFBoat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PreStepVelocity = HullMesh->GetPhysicsLinearVelocity();

	// Throttle spool: rise toward a larger demand, fall toward a smaller one / reversal.
	const bool bRising = FMath::Abs(ThrottleInput) > FMath::Abs(SpooledThrottle) && ThrottleInput * SpooledThrottle >= 0.f;
	const float SpoolRate = 1.f / (bRising ? ThrottleRiseTime : ThrottleFallTime);
	SpooledThrottle = FMath::FInterpConstantTo(SpooledThrottle, ThrottleInput, DeltaSeconds, SpoolRate);

	// Camera and all its feedback run on REAL time (undilated), so hitstop doesn't freeze-then-snap it.
	const UWorld* World = GetWorld();
	UpdateCamera(FMath::Min(World ? World->DeltaRealTimeSeconds : DeltaSeconds, 0.1f));

	if (!HullMesh->IsSimulatingPhysics() || Buoyancy->GetSubmergedFraction() <= 0.f)
	{
		return; // no grip in the air
	}

	const FTransform& Xf = HullMesh->GetComponentTransform();
	const FVector Fwd = Xf.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	const FVector Right = Xf.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
	const float Mass = HullMesh->GetMass();
	const float FwdSpeed = GetForwardSpeed();
	const float Authority = GetTurnAuthority();

	// Thrust at the stern, below the waterline (bow lifts because it acts below the centre of mass).
	if (!FMath::IsNearlyZero(SpooledThrottle))
	{
		const FVector P = Xf.TransformPosition(ThrustPoint);
		if (float(P.Z) - Buoyancy->GetWaterHeightAt(P) <= ThrustMaxAirGap)
		{
			const float Scale = SpooledThrottle > 0.f ? SpooledThrottle : SpooledThrottle * ReverseThrustScale;
			HullMesh->AddForceAtLocation(Fwd * (Scale * ThrustAccel * Mass), P);
		}

		// Squat: stern dips while accelerating from low speed (nose-up torque about -Right).
		const float SpeedAlpha = FMath::Clamp(FMath::Abs(FwdSpeed) / RudderFullSpeed, 0.f, 1.f);
		const float Squat = FMath::DegreesToRadians(SquatAccelDeg) * FMath::Max(SpooledThrottle, 0.f) * (1.f - SpeedAlpha);
		HullMesh->AddTorqueInRadians(-Right * Squat, NAME_None, /*bAccelChange*/ true);
	}

	// Rudder: lateral force at the stern. Steer > 0 (right) pushes the stern left so the bow swings right.
	// Moving backwards, the water hits the rudder from behind, so the force flips (steering inverts naturally).
	if (!FMath::IsNearlyZero(SteerInput))
	{
		const FVector P = Xf.TransformPosition(RudderPoint);
		if (float(P.Z) - Buoyancy->GetWaterHeightAt(P) <= ThrustMaxAirGap)
		{
			const float FlowSign = FwdSpeed < 0.f ? -1.f : 1.f;
			const float Lateral = -SteerInput * FlowSign * Authority * RudderForceAccel * Mass;
			HullMesh->AddForceAtLocation(Right * Lateral, P);
		}

		// Outward lean: steer right -> deck tips toward the boat's left. Torque axis Up x Dir tips the deck top toward Dir.
		const FVector Outward = -Right * FMath::Sign(SteerInput);
		const FVector LeanAxis = FVector::CrossProduct(FVector::UpVector, Outward);
		const float LeanAccel = FMath::DegreesToRadians(LeanAccelDeg) * FMath::Abs(SteerInput) * Authority;
		HullMesh->AddTorqueInRadians(LeanAxis * LeanAccel, NAME_None, /*bAccelChange*/ true);
	}
}

void ALFBoat::UpdateCamera(float RealDelta)
{
	// Frame-rate independent exponential follow, on real time.
	auto Follow = [RealDelta](float Speed) { return 1.f - FMath::Exp(-Speed * RealDelta); };

	CameraPivot = FMath::Lerp(CameraPivot, GetActorLocation(), Follow(CameraLocationLagSpeed));
	CameraArm->SetWorldLocation(CameraPivot);

	const FRotator BoatRot = GetActorRotation();
	CameraRot.Yaw = CameraRot.Yaw + FMath::FindDeltaAngleDegrees(CameraRot.Yaw, BoatRot.Yaw) * Follow(CameraYawLagSpeed);
	CameraRot.Pitch = FMath::Lerp(CameraRot.Pitch, CameraPitch + BoatRot.Pitch * CameraPitchFollow, Follow(CameraPitchLagSpeed));
	CameraRot.Roll = 0.f;
	CameraArm->SetWorldRotation(CameraRot);

	// Ram shake (player only): one smooth decaying sine, mostly vertical. Starts at 0 (sin), envelope (1-t)^2 -> no pops.
	FVector ShakeOffset = FVector::ZeroVector;
	if (ShakeAge >= 0.f)
	{
		ShakeAge += RealDelta;
		const float U = ShakeAge / ShakeDuration;
		if (U >= 1.f)
		{
			ShakeAge = -1.f;
		}
		else
		{
			const float Env = FMath::Square(1.f - U);
			const float Phase = ShakeAge * ShakeFrequency * UE_TWO_PI;
			const float Amp = ShakeAmplitude * ShakeStrength * Env;
			ShakeOffset = FVector(0.f, FMath::Sin(Phase * 0.5f) * Amp * ShakeHorizontalShare, FMath::Sin(Phase) * Amp);
		}
	}
	Camera->SetRelativeLocation(ShakeOffset);

	// FOV punch: instant widen, cubic ease-out back to base over FovPunchTime real seconds.
	float Fov = BaseFov;
	if (FovPunchAge >= 0.f)
	{
		FovPunchAge += RealDelta;
		const float U = FovPunchAge / FovPunchTime;
		if (U >= 1.f)
		{
			FovPunchAge = -1.f;
		}
		else
		{
			Fov += FovPunchDeg * FovPunchStrength * FMath::Cube(1.f - U);
		}
	}
	Camera->SetFieldOfView(Fov);
}

// ---------------------------------------------------------------- Hits

void ALFBoat::OnHullHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	ALFBoat* Other = Cast<ALFBoat>(OtherActor);
	if (!Other)
	{
		return;
	}
	// Per-pair cooldown on REAL time. Every contact event refreshes it (both mirrored entries), so a sustained
	// contact never re-fires; the first event of a new contact resolves the pair once, from the rammer's side.
	const double Now = GetWorld()->GetRealTimeSeconds();
	FPairContact& Mine = PairContacts.FindOrAdd(Other);
	FPairContact& Theirs = Other->PairContacts.FindOrAdd(this);
	if (Now == Mine.LastResolved)
	{
		return; // the other boat's event for the contact already resolved this frame
	}
	const bool bCoolingDown = Now - Mine.LastContact < PairHitCooldown;
	Mine.LastContact = Theirs.LastContact = Now;

	// Flat contact normal, pointing from the other boat toward me.
	FVector N = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
	const FVector OtherToMe = (GetActorLocation() - Other->GetActorLocation()).GetSafeNormal2D();
	if (N.IsNearlyZero())
	{
		N = OtherToMe;
	}
	if (FVector::DotProduct(N, OtherToMe) < 0.f)
	{
		N = -N;
	}

	// Closing speed along the normal, from pre-solve velocities (both boats see the same value).
	const FVector MyV = PreStepVelocity;
	const FVector OtherV = Other->GetPreStepVelocity();
	const float Closing = float(FVector::DotProduct(OtherV - MyV, N));
	if (Closing < MinContactSpeed)
	{
		return;
	}
	if (bCoolingDown)
	{
		++Mine.Suppressed;
		++Theirs.Suppressed;
		return;
	}

	// The boat moving faster into the contact is the rammer; the pair is resolved once, from its side.
	const float MyApproach = float(FVector::DotProduct(MyV, -N));
	const float OtherApproach = float(FVector::DotProduct(OtherV, N));
	const bool bIAmRammer = MyApproach > OtherApproach || (MyApproach == OtherApproach && GetUniqueID() < Other->GetUniqueID());
	const FVector Contact = Hit.ImpactPoint.IsNearlyZero() ? (GetActorLocation() + Other->GetActorLocation()) * 0.5 : FVector(Hit.ImpactPoint);
	if (bIAmRammer)
	{
		ResolveContact(Other, -N, Closing, Contact, Now);
	}
	else
	{
		Other->ResolveContact(this, N, Closing, Contact, Now);
	}
}

void ALFBoat::ResolveContact(ALFBoat* Target, const FVector& TargetDir, float Closing, const FVector& Contact, double Now)
{
	FPairContact& Mine = PairContacts.FindOrAdd(Target);
	FPairContact& Theirs = Target->PairContacts.FindOrAdd(this);
	const double SinceLast = Now - Mine.LastResolved;
	const int32 Suppressed = Mine.Suppressed;
	Mine.LastResolved = Theirs.LastResolved = Now;
	Mine.Suppressed = Theirs.Suppressed = 0;

	const bool bRam = Closing >= RamMinSpeed;
	const float Strength = bRam ? FMath::Clamp((Closing - RamMinSpeed) / FMath::Max(RamFullSpeed - RamMinSpeed, 1.f), 0.f, 1.f) : 0.f;

	UE_LOG(LogLF, Display, TEXT("[hit] %s -> %s tier=%s closing=%.0f cm/s strength=%.2f | t=%.2f real s, %.2f s since last pair hit, %d contacts suppressed by %.1f s pair cooldown"),
		*GetName(), *Target->GetName(), bRam ? TEXT("RAM") : TEXT("bump"), Closing, Strength,
		Now, SinceLast > 1e6 ? -1.0 : SinceLast, Suppressed, PairHitCooldown);

	if (!bRam)
	{
		return; // bump: physics only
	}

	ApplyRamTo(Target, TargetDir, Contact, Strength);

	OnSplash(Contact, Strength);
	OnRamImpact(Strength);
	Target->OnRamImpact(Strength);
	PlayRamFeedback(Strength);
	Target->PlayRamFeedback(Strength);
	StartHitstop(Strength);
}

void ALFBoat::ApplyRamTo(ALFBoat* Target, const FVector& TargetDir, const FVector& Contact, float Strength)
{
	UPrimitiveComponent* TBody = Target->HullMesh;
	const float Shove = RamShove * Strength;

	// Target: shove along the contact normal + heave, applied at the contact point's horizontal position but at the
	// target's centre-of-mass height (so it yaws from off-centre hits, while roll stays within the explicit clamp).
	FVector At = Contact;
	At.Z = TBody->GetCenterOfMass().Z;
	const FVector TargetDv = (TargetDir * (Shove * TargetShoveShare) + FVector::UpVector * (RamHeave * Strength)).GetClampedToMaxSize(MaxHitImpulse);
	TBody->AddImpulseAtLocation(TargetDv * TBody->GetMass(), At);

	// Target roll away from the hit + nose-up pitch kick: lifts and slaps down.
	const FVector RollAxis = FVector::CrossProduct(FVector::UpVector, TargetDir);
	const FVector PitchAxis = -Target->GetActorRightVector();
	const FVector Kick = (RollAxis * (RamRollKick * Strength) + PitchAxis * (RamPitchKick * Strength)).GetClampedToMaxSize(MaxHitRoll);
	TBody->AddAngularImpulseInRadians(Kick, NAME_None, /*bVelChange*/ true);

	// Rammer: recoil + loses most of its remaining speed toward the target.
	const FVector Back = -TargetDir;
	FVector MyDv = (Back * (Shove * RammerRecoilShare)).GetClampedToMaxSize(MaxHitImpulse);
	const float Toward = float(FVector::DotProduct(HullMesh->GetPhysicsLinearVelocity(), TargetDir));
	if (Toward > 0.f)
	{
		MyDv += Back * (Toward * (1.f - RammerKeepSpeed) * Strength);
	}
	HullMesh->AddImpulse(MyDv, NAME_None, /*bVelChange*/ true);
}

void ALFBoat::PlayRamFeedback(float Strength)
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	// Restart (never stack): a new ram replaces the running shake/punch.
	ShakeAge = 0.f;
	ShakeStrength = FMath::Max(Strength, 0.25f);
	FovPunchAge = 0.f;
	FovPunchStrength = FMath::Max(Strength, 0.25f);
	PC->PlayDynamicForceFeedback(RumbleIntensity * FMath::Max(Strength, 0.3f), RumbleDuration, true, true, true, true);
}

void ALFBoat::StartHitstop(float Strength)
{
	UWorld* World = GetWorld();
	if (!World || HitstopDilation >= 1.f)
	{
		return;
	}
	const float RealSeconds = FMath::Lerp(HitstopMinTime, HitstopMaxTime, Strength);
	UGameplayStatics::SetGlobalTimeDilation(World, HitstopDilation);
	// Restore on a REAL-time ticker (game timers would be slowed by the dilation itself).
	TWeakObjectPtr<UWorld> WeakWorld(World);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld](float) -> bool
	{
		if (UWorld* W = WeakWorld.Get())
		{
			UGameplayStatics::SetGlobalTimeDilation(W, 1.f);
		}
		return false; // one-shot
	}), RealSeconds);
}

void ALFBoat::OnSplash_Implementation(FVector Location, float Strength)
{
	if (SplashSystem)
	{
		if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), SplashSystem, Location))
		{
			NC->SetVariableFloat(TEXT("Strength"), Strength);
		}
	}
	else if (bDebugSplash)
	{
		DrawDebugSphere(GetWorld(), Location, 40.f + 120.f * Strength, 12, FColor::Cyan, false, 0.6f, 0, 3.f);
	}
}

void ALFBoat::LogSettle()
{
	const FVector L = GetActorLocation();
	const FRotator R = GetActorRotation();
	const float Water = Buoyancy->GetWaterHeightAt(L);
	const bool bLevel = FMath::Abs(R.Pitch) < 2.f && FMath::Abs(R.Roll) < 2.f && FMath::Abs(float(L.Z) - Water) < 10.f;
	if (bLevel)
	{
		UE_LOG(LogLF, Display, TEXT("[settle] %s t=%.1fs Z=%.1f water=%.1f (diff %.1f cm) pitch=%.2f roll=%.2f -> level"),
			*GetName(), GetWorld()->GetTimeSeconds(), L.Z, Water, L.Z - Water, R.Pitch, R.Roll);
	}
	else
	{
		UE_LOG(LogLF, Warning, TEXT("[settle] %s t=%.1fs Z=%.1f water=%.1f (diff %.1f cm) pitch=%.2f roll=%.2f -> NOT level"),
			*GetName(), GetWorld()->GetTimeSeconds(), L.Z, Water, L.Z - Water, R.Pitch, R.Roll);
	}
}

// ---------------------------------------------------------------- Input

void ALFBoat::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		return;
	}
	if (!DefaultMappingContext)
	{
		UE_LOG(LogLF, Warning, TEXT("ALFBoat %s: DefaultMappingContext not set - no input."), *GetName());
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Sub->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ALFBoat::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC || !ThrottleAction || !SteerAction)
	{
		UE_LOG(LogLF, Warning, TEXT("ALFBoat %s: Enhanced Input component or Throttle/Steer actions missing - no input."), *GetName());
		return;
	}
	EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ALFBoat::OnThrottle);
	EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ALFBoat::OnThrottleReleased);
	EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &ALFBoat::OnSteer);
	EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &ALFBoat::OnSteerReleased);
}

void ALFBoat::OnThrottle(const FInputActionValue& Value)
{
	ThrottleInput = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
}

void ALFBoat::OnSteer(const FInputActionValue& Value)
{
	SteerInput = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
}

void ALFBoat::OnThrottleReleased()
{
	ThrottleInput = 0.f;
}

void ALFBoat::OnSteerReleased()
{
	SteerInput = 0.f;
}
