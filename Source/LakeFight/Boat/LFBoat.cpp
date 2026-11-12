#include "Boat/LFBoat.h"

#include "Water/LFWater.h"
#include "Water/LFBuoyancyComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ALFBoat::ALFBoat()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// Physics hull fitted to SM_LF_Rowboat: 3.0 x 1.35 x 0.5 m, centred on the waterline origin (keel at -25 cm).
	Hull = CreateDefaultSubobject<UBoxComponent>(TEXT("Hull"));
	Hull->SetBoxExtent(FVector(150.f, 67.5f, 25.f));
	Hull->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Hull->SetSimulatePhysics(true);
	Hull->SetNotifyRigidBodyCollision(true);
	Hull->SetLinearDamping(0.f);   // drag comes from ULFBuoyancyComponent
	Hull->SetAngularDamping(0.f);
	Hull->SetHiddenInGame(true);
	RootComponent = Hull;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Hull);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	VisualMesh->SetRelativeScale3D(FVector::OneVector);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RowboatMesh(TEXT("/Game/LakeFight/Meshes/SM_LF_Rowboat.SM_LF_Rowboat"));
	if (RowboatMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(RowboatMesh.Object);
	}

	Buoyancy = CreateDefaultSubobject<ULFBuoyancyComponent>(TEXT("Buoyancy"));

	// Camera rotation is driven manually (UpdateCamera) so yaw can lag more than pitch.
	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Hull);
	CameraArm->SetUsingAbsoluteRotation(true);
	CameraArm->TargetArmLength = 850.f;
	CameraArm->SocketOffset = FVector(0.f, 0.f, 120.f);
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 5.f;
	CameraArm->bEnableCameraRotationLag = false;
	CameraArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(80.f);

	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ALFBoat::BeginPlay()
{
	Super::BeginPlay();

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
	Hull->SetPhysMaterialOverride(PhysMat);

	Hull->SetMassOverrideInKg(NAME_None, HullMassKg, true);
	Hull->SetCenterOfMass(CenterOfMassOffset);
	Hull->OnComponentHit.AddDynamic(this, &ALFBoat::OnHullHit);

	CameraRot = FRotator(CameraPitch, GetActorRotation().Yaw, 0.f);
	CameraArm->SetWorldRotation(CameraRot);

	if (SettleLogDelay > 0.f)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &ALFBoat::LogSettle, SettleLogDelay, false);
	}
}

float ALFBoat::GetForwardSpeed() const
{
	return float(FVector::DotProduct(Hull->GetPhysicsLinearVelocity(), GetActorForwardVector().GetSafeNormal2D()));
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
	PreStepVelocity = Hull->GetPhysicsLinearVelocity();

	// Throttle spool: rise toward a larger demand, fall toward a smaller one / reversal.
	const bool bRising = FMath::Abs(ThrottleInput) > FMath::Abs(SpooledThrottle) && ThrottleInput * SpooledThrottle >= 0.f;
	const float SpoolRate = 1.f / (bRising ? ThrottleRiseTime : ThrottleFallTime);
	SpooledThrottle = FMath::FInterpConstantTo(SpooledThrottle, ThrottleInput, DeltaSeconds, SpoolRate);

	UpdateCamera(DeltaSeconds);

	if (!Hull->IsSimulatingPhysics() || Buoyancy->GetSubmergedFraction() <= 0.f)
	{
		return; // no grip in the air
	}

	const FTransform& Xf = Hull->GetComponentTransform();
	const FVector Fwd = Xf.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	const FVector Right = Xf.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
	const float Mass = Hull->GetMass();
	const float FwdSpeed = GetForwardSpeed();
	const float Authority = GetTurnAuthority();

	// Thrust at the stern, below the waterline (bow lifts because it acts below the centre of mass).
	if (!FMath::IsNearlyZero(SpooledThrottle))
	{
		const FVector P = Xf.TransformPosition(ThrustPoint);
		if (float(P.Z) - Buoyancy->GetWaterHeightAt(P) <= ThrustMaxAirGap)
		{
			const float Scale = SpooledThrottle > 0.f ? SpooledThrottle : SpooledThrottle * ReverseThrustScale;
			Hull->AddForceAtLocation(Fwd * (Scale * ThrustAccel * Mass), P);
		}

		// Squat: stern dips while accelerating from low speed (nose-up torque about -Right).
		const float SpeedAlpha = FMath::Clamp(FMath::Abs(FwdSpeed) / RudderFullSpeed, 0.f, 1.f);
		const float Squat = FMath::DegreesToRadians(SquatAccelDeg) * FMath::Max(SpooledThrottle, 0.f) * (1.f - SpeedAlpha);
		Hull->AddTorqueInRadians(-Right * Squat, NAME_None, /*bAccelChange*/ true);
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
			Hull->AddForceAtLocation(Right * Lateral, P);
		}

		// Outward lean: steer right -> deck tips toward the boat's left. Torque axis Up x Dir tips the deck top toward Dir.
		const FVector Outward = -Right * FMath::Sign(SteerInput);
		const FVector LeanAxis = FVector::CrossProduct(FVector::UpVector, Outward);
		const float LeanAccel = FMath::DegreesToRadians(LeanAccelDeg) * FMath::Abs(SteerInput) * Authority;
		Hull->AddTorqueInRadians(LeanAxis * LeanAccel, NAME_None, /*bAccelChange*/ true);
	}
}

void ALFBoat::UpdateCamera(float DeltaSeconds)
{
	const FRotator BoatRot = GetActorRotation();
	const float YawAlpha = FMath::Clamp(DeltaSeconds * CameraYawLagSpeed, 0.f, 1.f);
	const float PitchAlpha = FMath::Clamp(DeltaSeconds * CameraPitchLagSpeed, 0.f, 1.f);
	CameraRot.Yaw = CameraRot.Yaw + FMath::FindDeltaAngleDegrees(CameraRot.Yaw, BoatRot.Yaw) * YawAlpha;
	CameraRot.Pitch = FMath::Lerp(CameraRot.Pitch, CameraPitch + BoatRot.Pitch * CameraPitchFollow, PitchAlpha);
	CameraRot.Roll = 0.f;
	CameraArm->SetWorldRotation(CameraRot);
}

void ALFBoat::OnHullHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const ALFBoat* OtherBoat = Cast<ALFBoat>(OtherActor);
	if (!OtherBoat)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHitTime < HitCooldown)
	{
		return;
	}

	// Direction from the other boat to me, flat. Closing speed uses pre-solve velocities.
	FVector Away = (GetActorLocation() - OtherActor->GetActorLocation()).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
	}
	const float Closing = float(FVector::DotProduct(OtherBoat->GetPreStepVelocity() - PreStepVelocity, Away));
	if (Closing < MinHitSpeed)
	{
		return;
	}
	LastHitTime = Now;

	const float S = FMath::Min(Closing, MaxHitSpeed);
	const FVector Shove = (Away * (S * HitShoveScale) + FVector::UpVector * (S * HitPopScale)).GetClampedToMaxSize(MaxHitImpulse);
	Hull->AddImpulse(Shove, NAME_None, /*bVelChange*/ true);

	const FVector RollAxis = FVector::CrossProduct(FVector::UpVector, Away); // tips my deck away from the hit
	const float Roll = FMath::Min(HitRollScale * S / 100.f, MaxHitRoll);
	Hull->AddAngularImpulseInRadians(RollAxis * Roll, NAME_None, /*bVelChange*/ true);

	const FVector SplashAt = Hit.ImpactPoint.IsNearlyZero() ? (GetActorLocation() + OtherActor->GetActorLocation()) * 0.5 : FVector(Hit.ImpactPoint);
	OnSplash(SplashAt, S / MaxHitSpeed);
}

void ALFBoat::OnSplash_Implementation(FVector Location, float Strength)
{
	if (bDebugSplash)
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
