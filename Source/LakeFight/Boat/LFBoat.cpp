#include "Boat/LFBoat.h"

#include "Fish/LFFishCargoComponent.h" // T04 hook

#include "Brawler/LFBrawler.h"
#include "Input/LFInput.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"


#include "Water/LFWater.h"
#include "Water/LFBuoyancyComponent.h"
#include "Audio/LFAudio.h"
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

// lf.debug.capsize [index]: kick boat #index (actor iteration order) over - for testing the capsize rule.
static FAutoConsoleCommandWithWorldAndArgs CmdLFCapsize(TEXT("lf.debug.capsize"), TEXT("lf.debug.capsize [index] - roll a boat over."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Want = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		int32 I = 0;
		for (TActorIterator<ALFBoat> It(World); It; ++It, ++I)
		{
			if (I == Want)
			{
				It->DebugCapsizeKick();
				UE_LOG(LogLF, Display, TEXT("[capsize] debug kick on %s"), *It->GetName());
			}
		}
	}));

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
	CreateDefaultSubobject<ULFFishCargoComponent>(TEXT("FishCargo")); // T04 hook
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> OarAsset(TEXT("/Game/LakeFight/Meshes/SM_LF_Oar.SM_LF_Oar"));
	OarMesh = OarAsset.Object;
	for (TObjectPtr<UStaticMeshComponent>* Oar : { &OarL, &OarR })
	{
		*Oar = CreateDefaultSubobject<UStaticMeshComponent>(Oar == &OarL ? TEXT("OarL") : TEXT("OarR"));
		(*Oar)->SetupAttachment(HullMesh, Oar == &OarL ? FName(TEXT("Oarlock_L")) : FName(TEXT("Oarlock_R")));
		(*Oar)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		(*Oar)->SetCanEverAffectNavigation(false);
	}
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
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCMouse(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	static ConstructorHelpers::FObjectFinder<UInputAction> IAMouseLook(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	MouseLookMappingContext = IMCMouse.Object;
	MouseLookAction = IAMouseLook.Object;
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
	HullMesh->SetRenderCustomDepth(true);
	HullMesh->SetCustomDepthStencilValue(HullStencil);
}

void ALFBoat::BeginPlay()
{
	Super::BeginPlay();
	{
		// Oars: SM_LF_Oar at the oarlock sockets (pivot = collar), unscaled.
		UStaticMesh* Mesh = OarMesh ? OarMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LakeFight/Meshes/SM_LF_Oar.SM_LF_Oar"));
		for (int32 Side = 0; Side < 2; ++Side)
		{
			UStaticMeshComponent* Oar = Side == 0 ? OarL.Get() : OarR.Get();
			const FName Socket = Side == 0 ? FName(TEXT("Oarlock_L")) : FName(TEXT("Oarlock_R"));
			Oar->SetStaticMesh(Mesh);
			Oar->AttachToComponent(HullMesh, FAttachmentTransformRules::KeepRelativeTransform, HullMesh->DoesSocketExist(Socket) ? Socket : NAME_None);
			Oar->SetWorldScale3D(FVector::OneVector);
		}
		const FTransform& H = HullMesh->GetComponentTransform();
		const FString GripInfo = OarL->DoesSocketExist(TEXT("Grip"))
			? FString::Printf(TEXT("found (X %.1f)"), OarL->GetSocketTransform(TEXT("Grip"), RTS_Component).GetLocation().X)
			: FString::Printf(TEXT("MISSING (OarGripX %.0f)"), OarGripX);
		UE_LOG(LogLF, Display, TEXT("[oars] %s: %s, oarlock sockets %s (L %s, R %s hull-local), grip socket %s"), *GetName(), *GetNameSafe(Mesh),
			HullMesh->DoesSocketExist(TEXT("Oarlock_L")) && HullMesh->DoesSocketExist(TEXT("Oarlock_R")) ? TEXT("found") : TEXT("MISSING (placeholder positions)"),
			*H.InverseTransformPosition(GetOarlockWorld(true)).ToCompactString(), *H.InverseTransformPosition(GetOarlockWorld(false)).ToCompactString(),
			*GripInfo);
		UpdateOars(1.f);
	}

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

	// Placed instances saved before SplashSystem existed carry a stale None override: fall back to the class default.
	if (!SplashSystem)
	{
		SplashSystem = GetClass()->GetDefaultObject<ALFBoat>()->SplashSystem;
	}

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

FVector ALFBoat::GetOarlockWorld(bool bLeft) const
{
	const FName Socket = bLeft ? FName(TEXT("Oarlock_L")) : FName(TEXT("Oarlock_R"));
	return HullMesh->DoesSocketExist(Socket) ? HullMesh->GetSocketLocation(Socket)
		: HullMesh->GetComponentTransform().TransformPosition(bLeft ? OarlockLocalL : OarlockLocalR);
}

void ALFBoat::ApplyOarPose(bool bLeft)
{
	UStaticMeshComponent* Oar = bLeft ? OarL.Get() : OarR.Get();
	if (!Oar)
	{
		return;
	}
	// Everything in hull space, then to world: the oars are hull children (they ride and flip with the hull).
	const FTransform& H = HullMesh->GetComponentTransform();
	const FVector Lock = H.InverseTransformPosition(GetOarlockWorld(bLeft));
	const float In = Lock.Y > 0.f ? -1.f : 1.f; // inboard = toward the centreline (by the lock's side, not its socket name)
	// Shipped: blade aft along the gunwale, turned a little inboard so the blade rests inside it, collar slid in/up a little.
	const FVector ShipLoc = Lock + FVector(0.f, In * OarShipInboard, OarShipUp);
	const FQuat ShipRot = FRotator(0.f, 180.f - In * OarShipYawDeg, 0.f).Quaternion();
	FVector Loc = ShipLoc;
	FQuat Rot = ShipRot;
	if (OarRowBlend > 0.f && bOarAimed)
	{
		// Rowing: collar in the oarlock, -X (the handle) toward the hand target, blade squared (its width vertical).
		const FVector X = (Lock - OarAimLocal[bLeft ? 0 : 1]).GetSafeNormal();
		if (!X.IsNearlyZero())
		{
			const FQuat RowRot = FRotationMatrix::MakeFromXY(X, FVector::UpVector).ToQuat();
			Loc = FMath::Lerp(ShipLoc, Lock, OarRowBlend);
			Rot = FQuat::Slerp(ShipRot, RowRot, OarRowBlend).GetNormalized();
		}
	}
	Oar->SetWorldLocationAndRotation(H.TransformPosition(Loc), H.TransformRotation(Rot));
}

void ALFBoat::AimOars(const FVector& AimLWorld, const FVector& AimRWorld)
{
	const FTransform& H = HullMesh->GetComponentTransform();
	OarAimLocal[0] = H.InverseTransformPosition(AimLWorld);
	OarAimLocal[1] = H.InverseTransformPosition(AimRWorld);
	bOarAimed = true;
	ApplyOarPose(true);
	ApplyOarPose(false);
	// Glitch metric (lf.debug.ik log): the largest frame-to-frame jump of either grip in hull space since the last log line.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVector G = H.InverseTransformPosition(GetOarGrip(Side == 0).GetLocation());
		if (bOarGripPrevValid)
		{
			OarGripMaxStep = FMath::Max(OarGripMaxStep, float(FVector::Dist(G, OarGripPrevLocal[Side])));
		}
		OarGripPrevLocal[Side] = G;
	}
	bOarGripPrevValid = true;
}

FTransform ALFBoat::GetOarGrip(bool bLeft) const
{
	const UStaticMeshComponent* Oar = bLeft ? OarL.Get() : OarR.Get();
	static const FName Grip(TEXT("Grip"));
	const FTransform& O = Oar->GetComponentTransform();
	const FVector Loc = Oar->DoesSocketExist(Grip) ? Oar->GetSocketLocation(Grip) : O.TransformPosition(FVector(OarGripX, 0.f, 0.f));
	// The socket's own axes come through the FBX empty conversion; the shaft axis is the oar's X.
	return FTransform(O.GetRotation(), Loc);
}

float ALFBoat::GetOarGripReach() const
{
	static const FName Grip(TEXT("Grip"));
	const float X = OarL && OarL->DoesSocketExist(Grip) ? float(OarL->GetSocketTransform(Grip, RTS_Component).GetLocation().X) : OarGripX;
	return FMath::Max(FMath::Abs(X), 1.f); // oars are unscaled
}

void ALFBoat::UpdateOars(float DeltaSeconds)
{
	const ALFBrawler* D = Driver.Get();
	const bool bRowing = D && D->GetBrawlerState() == ELFBrawlerState::Driving && !bCapsized;
	OarRowBlend = FMath::FInterpConstantTo(OarRowBlend, bRowing ? 1.f : 0.f, DeltaSeconds, 1.f / OarBlendTime);
	if (OarRowBlend <= 0.f)
	{
		bOarAimed = false; // the next driver aims them afresh
		bOarGripPrevValid = false;
	}
	ApplyOarPose(true);
	ApplyOarPose(false);
	static const IConsoleVariable* DebugIK = IConsoleManager::Get().FindConsoleVariable(TEXT("lf.debug.ik"));
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (DebugIK && DebugIK->GetInt() != 0 && D && bRowing && Now - LastOarLogTime >= 1.0)
	{
		LastOarLogTime = Now;
		const FTransform& H = HullMesh->GetComponentTransform();
		const float Water = LF::Water::GetHeightAt(H.GetLocation(), float(Now));
		auto BladeZ = [&](const UStaticMeshComponent* Oar) { return float(Oar->GetComponentTransform().TransformPosition(FVector(117.f, 0.f, 0.f)).Z) - Water; };
		// How far the hand targets were from the oar grips (the two-bone IK pulls the hands this far), and the gap between the grips.
		const FVector GL = GetOarGrip(true).GetLocation(), GR = GetOarGrip(false).GetLocation();
		UE_LOG(LogLF, Log, TEXT("[oars] %s rowing (throttle %.2f): blade centres L %.0f R %.0f cm vs water; hand target -> grip L %.1f R %.1f cm; grips %.1f cm apart; largest grip step %.1f cm/frame"),
			*GetName(), GetThrottleOutput(), BladeZ(OarL.Get()), BladeZ(OarR.Get()),
			FVector::Dist(H.TransformPosition(OarAimLocal[0]), GL), FVector::Dist(H.TransformPosition(OarAimLocal[1]), GR), FVector::Dist(GL, GR),
			OarGripMaxStep);
		OarGripMaxStep = 0.f;
	}
}

void ALFBoat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PreStepVelocity = HullMesh->GetPhysicsLinearVelocity();

	// Player driving: the live action values are the truth (AI drives through SetDriveInput instead).
	if (IsPlayerControlled() && ThrottleValue && SteerValue)
	{
		ThrottleInput = DebugThrottle != 0.f ? DebugThrottle : FMath::Clamp(ThrottleValue->GetValue().Get<float>(), -1.f, 1.f);
		SteerInput = FMath::Clamp(SteerValue->GetValue().Get<float>(), -1.f, 1.f);
	}
	UpdateCapsize(DeltaSeconds);
	if (bCapsized)
	{
		ThrottleInput = 0.f; // no driving while capsized
		SteerInput = 0.f;
	}

	// Throttle spool: rise toward a larger demand, fall toward a smaller one / reversal.
	const bool bRising = FMath::Abs(ThrottleInput) > FMath::Abs(SpooledThrottle) && ThrottleInput * SpooledThrottle >= 0.f;
	const float SpoolRate = 1.f / (bRising ? ThrottleRiseTime : ThrottleFallTime);
	SpooledThrottle = FMath::FInterpConstantTo(SpooledThrottle, ThrottleInput, DeltaSeconds, SpoolRate);

	// Camera and all its feedback run on REAL time (undilated), so hitstop doesn't freeze-then-snap it.
	const UWorld* World = GetWorld();
	UpdateCamera(FMath::Min(World ? World->DeltaRealTimeSeconds : DeltaSeconds, 0.1f));
	UpdateOars(DeltaSeconds);

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
	const bool bFreeLook = Controller && (CameraRecenterDelay <= 0.f || FPlatformTime::Seconds() - LastLookRealTime < CameraRecenterDelay);
	if (bFreeLook && FPlatformTime::Seconds() - LastLookRealTime < 1e6)
	{
		// Player is looking around: the camera follows the control rotation (mouse / right stick).
		const FRotator C = Controller->GetControlRotation();
		CameraRot.Yaw = C.Yaw;
		CameraRot.Pitch = FMath::ClampAngle(C.Pitch, CameraMinPitch, CameraMaxPitch);
	}
	else
	{
		CameraRot.Yaw = CameraRot.Yaw + FMath::FindDeltaAngleDegrees(CameraRot.Yaw, BoatRot.Yaw) * Follow(CameraYawLagSpeed);
		CameraRot.Pitch = FMath::Lerp(CameraRot.Pitch, CameraPitch + BoatRot.Pitch * CameraPitchFollow, Follow(CameraPitchLagSpeed));
		if (Controller)
		{
			Controller->SetControlRotation(CameraRot); // free-look starts from where the camera is
		}
	}
	CameraRot.Roll = 0.f;
	{
		// Keep the lens above the water: camera Z ~= pivot + socket - arm * sin(pitch). Waves add up to ~12.5 cm.
		const float Water = LF::Water::GetHeightAt(CameraPivot, GetWorld()->GetTimeSeconds()) + 12.5f;
		const float Room = float(CameraPivot.Z + CameraArm->SocketOffset.Z) - Water - CameraWaterClearance;
		const float MaxPitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Room / FMath::Max(CameraArm->TargetArmLength, 1.f), -1.f, 1.f)));
		CameraRot.Pitch = FMath::Min(FRotator::NormalizeAxis(CameraRot.Pitch), MaxPitch);
		if (Controller && FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) > CameraRot.Pitch)
		{
			FRotator C = Controller->GetControlRotation();
			C.Pitch = CameraRot.Pitch; // no dead zone when looking back down
			Controller->SetControlRotation(C);
		}
	}
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
		if (ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(this))
		{
			Audio->Play(ELFSound::BoatCreak, Contact, 0.35f, 1.f, TEXT("bump"));
		}
		return; // bump: physics only
	}
	if (ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(this))
	{
		Audio->Play(ELFSound::RamWood, Contact, FMath::Lerp(0.7f, 1.f, Strength), 1.f, TEXT("ram"));
		Audio->Play(ELFSound::RamBass, Contact, FMath::Lerp(0.6f, 1.f, Strength), 1.f, TEXT("ram"));
	}
	if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this))
	{
		Music->Duck(TEXT("ram"));
	}

	// T04 hook
	if (ULFFishCargoComponent* Cargo = ULFFishCargoComponent::Get(Target))
	{
		Cargo->Spill(Strength >= 0.5f ? 2 : 1, TEXT("ram"));
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
	// 2 deg of wave slop + up to 4 deg of deliberate crew fake-weight tilt (ALFBrawler::MaxFakeTiltDeg, T02).
	const bool bLevel = FMath::Abs(R.Pitch) < 6.f && FMath::Abs(R.Roll) < 6.f && FMath::Abs(float(L.Z) - Water) < 10.f;
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
		Sub->AddMappingContext(LF::Input::Get().Context, 1); // Interact = leave the driver seat
		Sub->AddMappingContext(LF::Input::Get().BoatContext, 1); // right-stick free-look
		if (MouseLookMappingContext)
		{
			Sub->AddMappingContext(MouseLookMappingContext, 0);
		}
	}
}

void ALFBoat::UnPossessed()
{
	// Remove our contexts so W/S etc. don't keep consuming keys the brawler needs.
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Sub->RemoveMappingContext(DefaultMappingContext);
			}
			Sub->RemoveMappingContext(LF::Input::Get().Context);
			Sub->RemoveMappingContext(LF::Input::Get().BoatContext);
			if (MouseLookMappingContext)
			{
				Sub->RemoveMappingContext(MouseLookMappingContext);
			}
		}
	}
	ThrottleInput = 0.f;
	SteerInput = 0.f;
	ThrottleValue = nullptr; // the input component goes away with the controller
	SteerValue = nullptr;
	Super::UnPossessed();
}

float ALFBoat::GetTiltDeg() const
{
	const FVector Up = HullMesh->GetComponentTransform().GetUnitAxis(EAxis::Z);
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(Up.Z), -1.f, 1.f)));
}

void ALFBoat::UpdateCapsize(float DeltaSeconds)
{
	if (!HullMesh->IsSimulatingPhysics())
	{
		return;
	}
	const float Tilt = GetTiltDeg();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bCapsized)
	{
		CapsizeTimer = Tilt > CapsizeRollDeg ? CapsizeTimer + DeltaSeconds : 0.f;
		if (CapsizeTimer >= CapsizeHoldTime)
		{
			Capsize();
		}
		return;
	}
	const float Since = float(Now - CapsizedAt);
	if (Since < CapsizedTime)
	{
		return; // lies capsized
	}
	if (Since < CapsizedTime + RightingTime)
	{
		// Deliberate righting: torque toward upright (proportional to the tilt) with angular damping.
		const FVector Up = HullMesh->GetComponentTransform().GetUnitAxis(EAxis::Z);
		FVector Axis = FVector::CrossProduct(Up, FVector::UpVector);
		if (Axis.IsNearlyZero())
		{
			Axis = HullMesh->GetComponentTransform().GetUnitAxis(EAxis::X); // upside down exactly: roll about the keel
		}
		const float Angle = FMath::DegreesToRadians(Tilt);
		const FVector W = HullMesh->GetPhysicsAngularVelocityInRadians();
		HullMesh->AddTorqueInRadians(Axis.GetSafeNormal() * (Angle * RightingAccel) - W * RightingDamping, NAME_None, /*bAccelChange*/ true);
		return;
	}
	if (Tilt < 25.f)
	{
		bCapsized = false;
		CapsizeTimer = 0.f;
		UE_LOG(LogLF, Display, TEXT("[capsize] %s righted itself (tilt %.0f deg, %.1f s after capsizing)"), *GetName(), Tilt, Since);
		if (SplashSystem)
		{
			if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), SplashSystem, GetActorLocation(), FRotator::ZeroRotator, FVector(2.5f)))
			{
				NC->SetVariableFloat(TEXT("Strength"), 1.f);
			}
		}
		OnSplash(GetActorLocation(), 1.f);
		if (ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(this))
		{
			Audio->Play(ELFSound::Surface, GetActorLocation(), 1.8f, 0.7f, TEXT("boat righted"));
		}
	}
	else if (Since > CapsizedTime + RightingTime * 3.f)
	{
		CapsizedAt = Now - CapsizedTime; // still over: push again
	}
}

void ALFBoat::Capsize()
{
	// T04 hook
	if (ULFFishCargoComponent* Cargo = ULFFishCargoComponent::Get(this))
	{
		Cargo->Spill(Cargo->GetCount(), TEXT("capsize"));
	}
	bCapsized = true;
	CapsizedAt = GetWorld()->GetTimeSeconds();
	ThrottleInput = 0.f;
	SteerInput = 0.f;
	int32 Thrown = 0;
	for (TActorIterator<ALFBrawler> It(GetWorld()); It; ++It)
	{
		ALFBrawler* B = *It;
		if (B->GetBoat() == this && B->KnockOverboard((B->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), 0.5f))
		{
			++Thrown;
		}
	}
	Driver = nullptr;
	if (ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(this))
	{
		Audio->Play(ELFSound::RamBass, GetActorLocation(), 1.f, 1.f, TEXT("capsize (Overboard per person follows on splash-in)"));
	}
	if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this))
	{
		Music->Duck(TEXT("capsize"));
	}
	UE_LOG(LogLF, Display, TEXT("[capsize] %s CAPSIZED (tilt %.0f deg) - %d aboard thrown overboard, driving disabled for %.1f s"),
		*GetName(), GetTiltDeg(), Thrown, CapsizedTime);
}

void ALFBoat::DebugCapsizeKick()
{
	// Roll the hull 150 deg about its keel (teleport) - the capsize rule then has to take over.
	const FRotator R = HullMesh->GetComponentRotation();
	HullMesh->SetWorldLocationAndRotation(HullMesh->GetComponentLocation() + FVector(0.f, 0.f, 30.f), FRotator(0.f, R.Yaw, 150.f), false, nullptr, ETeleportType::TeleportPhysics);
	HullMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

float ALFBoat::GunwaleDistance(const FVector& WorldPos, FVector2D& OutLocalNearest) const
{
	const FVector L = HullMesh->GetComponentTransform().InverseTransformPosition(WorldPos);
	const FVector2D P(L.X, L.Y);
	constexpr int32 N = UE_ARRAY_COUNT(RowboatPlan);
	float Best = TNumericLimits<float>::Max();
	bool bInside = false;
	for (int32 i = 0, j = N - 1; i < N; j = i++)
	{
		const FVector2D A = RowboatPlan[j] * 100.;
		const FVector2D B = RowboatPlan[i] * 100.;
		const FVector2D E = B - A;
		const float T = FMath::Clamp(float(FVector2D::DotProduct(P - A, E) / E.SizeSquared()), 0.f, 1.f);
		const FVector2D Q = A + E * T;
		const float D = float(FVector2D::Distance(P, Q));
		if (D < Best)
		{
			Best = D;
			OutLocalNearest = Q;
		}
		if (((B.Y > P.Y) != (A.Y > P.Y)) && (P.X < (A.X - B.X) * (P.Y - B.Y) / (A.Y - B.Y) + B.X))
		{
			bInside = !bInside;
		}
	}
	return bInside ? -Best : Best;
}

FVector ALFBoat::GetDriverSeatWorld() const
{
	return HullMesh->GetComponentTransform().TransformPosition(DriverSeatLocal);
}

void ALFBoat::OnLook(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (V.IsNearlyZero())
	{
		return;
	}
	AddControllerYawInput(V.X);
	AddControllerPitchInput(V.Y);
	LastLookRealTime = FPlatformTime::Seconds();
}

void ALFBoat::OnInteract()
{
	if (ALFBrawler* D = Driver.Get())
	{
		D->LeaveDriving();
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
	// Polled every tick (see Tick): a key held before this mapping was added never sends Completed on release.
	ThrottleValue = &EIC->BindActionValue(ThrottleAction);
	SteerValue = &EIC->BindActionValue(SteerAction);
	EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ALFBoat::OnThrottle);
	EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ALFBoat::OnThrottleReleased);
	EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &ALFBoat::OnSteer);
	EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &ALFBoat::OnSteerReleased);
	EIC->BindAction(LF::Input::Get().Interact, ETriggerEvent::Started, this, &ALFBoat::OnInteract);
	EIC->BindAction(LF::Input::Get().Look, ETriggerEvent::Triggered, this, &ALFBoat::OnLook);
	if (MouseLookAction)
	{
		EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ALFBoat::OnLook);
	}
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
