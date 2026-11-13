#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LFLakeSurface.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialParameterCollection;

/**
 * The lake: a flat, evenly subdivided grid (so the material's World Position Offset waves are visible) centred
 * on the actor, at Z = LF::Water::BaseHeight. No collision - boats float via ULFBuoyancyComponent.
 * On BeginPlay it pushes the C++ wave table into WaterMPC, which keeps the material in sync with physics.
 */
UCLASS()
class LAKEFIGHT_API ALFLakeSurface : public AActor
{
	GENERATED_BODY()

public:
	ALFLakeSurface();

	/** Lake size in cm (X, Y). */
	UPROPERTY(EditAnywhere, Category="LF|Lake")
	FVector2D Size = FVector2D(12000.f, 8000.f);

	/** Grid cell size in cm. Must be well under the shortest wavelength (600 cm). */
	UPROPERTY(EditAnywhere, Category="LF|Lake", meta=(ClampMin="25"))
	float CellSize = 100.f;

	UPROPERTY(EditAnywhere, Category="LF|Lake")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** MPC_LF_Water. Receives the wave constants at BeginPlay. */
	UPROPERTY(EditAnywhere, Category="LF|Lake")
	TObjectPtr<UMaterialParameterCollection> WaterMPC;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Also tick in the editor viewport, so boat foam shows outside PIE. */
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** MPC slots Boat0..N-1 = (X, Y, yaw rad, horizontal speed cm/s) for the material's boat foam ring + wake. */
	static constexpr int32 MaxFoamBoats = 4;

private:
	UPROPERTY(VisibleAnywhere, Category="LF|Lake")
	TObjectPtr<UProceduralMeshComponent> Surface;

	void BuildGrid();
};
