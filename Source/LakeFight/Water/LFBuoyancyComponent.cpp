#include "Water/LFBuoyancyComponent.h"

#include "Water/LFWater.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

ULFBuoyancyComponent::ULFBuoyancyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// SM_LF_Rowboat hull corners at the waterline (stern is wide, bow tapers). Symmetric in X so the boat trims level.
	Pontoons = {
		FVector( 135.f,  35.f, 0.f),  // bow pontoons pulled inward to match the taper
		FVector( 135.f, -35.f, 0.f),
		FVector(-135.f,  55.f, 0.f),
		FVector(-135.f, -55.f, 0.f),
	};
}

float ULFBuoyancyComponent::GetWaterHeightAt(const FVector& WorldPos) const
{
	const UWorld* World = GetWorld();
	return LF::Water::GetHeightAt(WorldPos, World ? World->GetTimeSeconds() : 0.f);
}

void ULFBuoyancyComponent::BeginPlay()
{
	Super::BeginPlay();

	Body = GetOwner() ? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()) : nullptr;
	if (!Body || !Body->IsSimulatingPhysics())
	{
		UE_LOG(LogLF, Warning, TEXT("ULFBuoyancyComponent on %s: root is not a physics-simulating primitive - buoyancy disabled."),
			*GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}
	if (Pontoons.Num() == 0)
	{
		UE_LOG(LogLF, Warning, TEXT("ULFBuoyancyComponent on %s: no pontoons - buoyancy disabled."), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
	}
}

void ULFBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Body || !Body->IsSimulatingPhysics() || Pontoons.Num() == 0)
	{
		return;
	}

	const float Time = GetWorld()->GetTimeSeconds();
	const float Gravity = FMath::Abs(GetWorld()->GetGravityZ());
	const float Mass = Body->GetMass();
	const float MassPerPontoon = Mass / Pontoons.Num();
	const FTransform& Xf = Body->GetComponentTransform();

	int32 InWater = 0;
	for (const FVector& Local : Pontoons)
	{
		const FVector P = Xf.TransformPosition(Local);
		const float Depth = LF::Water::GetHeightAt(P, Time) - float(P.Z);

		// Spring around the waterline with gravity cancelled at Depth=0; zero force once the point is
		// Gravity/Stiffness above the water (continuous, so no jitter at the surface).
		const float SpringAccel = Gravity + Stiffness * Depth;
		if (SpringAccel <= 0.f)
		{
			continue;
		}
		++InWater;
		const float Vz = float(Body->GetPhysicsLinearVelocityAtPoint(P).Z);
		const float Accel = FMath::Clamp(SpringAccel - Damping * Vz, 0.f, MaxBuoyancyG * Gravity);
		Body->AddForceAtLocation(FVector(0.f, 0.f, Accel * MassPerPontoon), P);
	}
	SubmergedFraction = float(InWater) / Pontoons.Num();

	if (InWater == 0)
	{
		return; // airborne: no water drag
	}

	// Linear drag split in the hull frame (horizontal only, vertical is handled by pontoon damping).
	const FVector V = Body->GetPhysicsLinearVelocity();
	const FVector Fwd = Xf.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	const FVector Right = Xf.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
	const FVector DragAccel = -(ForwardDrag * FVector::DotProduct(V, Fwd)) * Fwd
	                          -(LateralDrag * FVector::DotProduct(V, Right)) * Right;
	Body->AddForce(DragAccel * SubmergedFraction, NAME_None, /*bAccelChange*/ true);

	// Angular drag: yaw vs. pitch/roll.
	const FVector W = Body->GetPhysicsAngularVelocityInRadians();
	const FVector Yaw(0.f, 0.f, W.Z);
	const FVector Tilt(W.X, W.Y, 0.f);
	Body->AddTorqueInRadians(-(AngularDrag * Yaw + TiltDrag * Tilt) * SubmergedFraction, NAME_None, /*bAccelChange*/ true);
}
