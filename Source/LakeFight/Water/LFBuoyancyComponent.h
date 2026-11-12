#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LFBuoyancyComponent.generated.h"

class UPrimitiveComponent;

/**
 * Per-pontoon spring + damping toward LF::Water::GetHeightAt, plus forward/lateral/angular drag in the hull frame.
 * Acts on the owner's root primitive (must simulate physics). Forces are mass-normalized, so tuning does not
 * change with hull mass. Gravity is cancelled at depth 0, so at rest each pontoon sits exactly on the waterline.
 */
UCLASS(ClassGroup=(LakeFight), meta=(BlueprintSpawnableComponent))
class LAKEFIGHT_API ULFBuoyancyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULFBuoyancyComponent();

	/** Pontoon positions in the hull's local space (cm). Z = 0 means "this point floats at the waterline". */
	UPROPERTY(EditDefaultsOnly, Category="LF|Buoyancy")
	TArray<FVector> Pontoons;

	/** Extra upward accel per cm of submersion (1/s^2). Higher = stiffer, bobs faster. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Buoyancy", meta=(ClampMin="1"))
	float Stiffness = 60.f;

	/** Vertical velocity damping at each pontoon (1/s). ~2*sqrt(Stiffness)=15 is critical; lower = bouncier. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Buoyancy", meta=(ClampMin="0"))
	float Damping = 9.f;

	/** Cap on buoyant accel per pontoon, in multiples of gravity (stops rockets when dropped deep). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Buoyancy", meta=(ClampMin="1"))
	float MaxBuoyancyG = 4.f;

	/** Drag along the hull's forward axis (1/s). 1/ForwardDrag = coast time constant. Low = long coast. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float ForwardDrag = 0.45f;

	/** Drag along the hull's right axis (1/s). Allows a short skid in hard turns; keep well above ForwardDrag. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float LateralDrag = 2.0f;

	/** Yaw angular drag (1/s). 1/AngularDrag ~= how long a turn carries on after releasing steer. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float AngularDrag = 1.0f;

	/** Pitch/roll angular drag (1/s), on top of pontoon damping. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Handling", meta=(ClampMin="0"))
	float TiltDrag = 1.0f;

	/** Fraction of pontoons currently in the water (0..1). */
	UFUNCTION(BlueprintPure, Category="LF|Buoyancy")
	float GetSubmergedFraction() const { return SubmergedFraction; }

	/** Water height under a world point right now. */
	UFUNCTION(BlueprintPure, Category="LF|Buoyancy")
	float GetWaterHeightAt(const FVector& WorldPos) const;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> Body;

	float SubmergedFraction = 0.f;
};
