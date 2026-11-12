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
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ALFBoat::ALFBoat()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Hull = CreateDefaultSubobject<UBoxComponent>(TEXT("Hull"));
	Hull->SetBoxExtent(FVector(150.f, 55.f, 30.f));
	Hull->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Hull->SetSimulatePhysics(true);
	Hull->SetNotifyRigidBodyCollision(true);
	Hull->SetLinearDamping(0.f);   // drag comes from ULFBuoyancyComponent
	Hull->SetAngularDamping(0.f);
	Hull->SetHiddenInGame(true);
	RootComponent = Hull;

	// Placeholder visual: engine cube scaled to the hull. BP_LF_Boat swaps in SM_LF_Rowboat when it exists.
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Hull);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CubeMesh.Object);
		VisualMesh->SetRelativeScale3D(FVector(3.0f, 1.1f, 0.6f));
	}

	Buoyancy = CreateDefaultSubobject<ULFBuoyancyComponent>(TEXT("Buoyancy"));

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(Hull);
	CameraArm->TargetArmLength = 850.f;
	CameraArm->SocketOffset = FVector(0.f, 0.f, 120.f);
	CameraArm->SetRelativeRotation(FRotator(-18.f, 0.f, 0.f));
	CameraArm->bInheritPitch = false; // don't seasick the player: follow yaw only
	CameraArm->bInheritRoll = false;
	CameraArm->bInheritYaw = true;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 4.f;
	CameraArm->bEnableCameraRotationLag = true;
	CameraArm->CameraRotationLagSpeed = 3.5f;
	CameraArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(80.f);

	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ALFBoat::BeginPlay()
{
	Super::BeginPlay();

	Hull->SetMassOverrideInKg(NAME_None, HullMassKg, true);
	Hull->SetCenterOfMass(CenterOfMassOffset);
	Hull->OnComponentHit.AddDynamic(this, &ALFBoat::OnHullHit);

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

void ALFBoat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PreStepVelocity = Hull->GetPhysicsLinearVelocity();

	if (!Hull->IsSimulatingPhysics() || Buoyancy->GetSubmergedFraction() <= 0.f)
	{
		return; // no grip in the air
	}

	const FTransform& Xf = Hull->GetComponentTransform();
	const FVector Fwd = Xf.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	const float SpeedAlpha = FMath::Clamp(FMath::Abs(GetForwardSpeed()) / FullEffectSpeed, 0.f, 1.f);

	// Thrust at the stern, below the waterline.
	if (!FMath::IsNearlyZero(ThrottleInput))
	{
		const FVector P = Xf.TransformPosition(ThrustPoint);
		const float AirGap = float(P.Z) - Buoyancy->GetWaterHeightAt(P);
		if (AirGap <= ThrustMaxAirGap)
		{
			const float Scale = ThrottleInput > 0.f ? ThrottleInput : ThrottleInput * ReverseThrustScale;
			Hull->AddForceAtLocation(Fwd * (Scale * ThrustAccel * Hull->GetMass()), P);
		}
	}

	// Turn (yaw torque) + outward lean (roll torque) when turning at speed.
	if (!FMath::IsNearlyZero(SteerInput))
	{
		const float TurnScale = FMath::Lerp(StationaryTurnScale, 1.f, SpeedAlpha);
		const float YawAccel = FMath::DegreesToRadians(TurnAccelDeg) * SteerInput * TurnScale;
		Hull->AddTorqueInRadians(FVector(0.f, 0.f, YawAccel), NAME_None, /*bAccelChange*/ true);

		// Steer > 0 turns right, so "outward" is the boat's left. Torque axis Up x Dir tips the deck top toward Dir.
		const FVector Outward = -Xf.GetUnitAxis(EAxis::Y).GetSafeNormal2D() * FMath::Sign(SteerInput);
		const FVector LeanAxis = FVector::CrossProduct(FVector::UpVector, Outward);
		const float LeanAccel = FMath::DegreesToRadians(LeanAccelDeg) * FMath::Abs(SteerInput) * SpeedAlpha;
		Hull->AddTorqueInRadians(LeanAxis * LeanAccel, NAME_None, /*bAccelChange*/ true);
	}
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
	Hull->AddImpulse(Away * (S * HitShoveScale) + FVector::UpVector * (S * HitPopScale), NAME_None, /*bVelChange*/ true);
	const FVector RollAxis = FVector::CrossProduct(FVector::UpVector, Away); // tips my deck away from the hit
	Hull->AddAngularImpulseInRadians(RollAxis * (HitRollScale * S / 100.f), NAME_None, /*bVelChange*/ true);

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
