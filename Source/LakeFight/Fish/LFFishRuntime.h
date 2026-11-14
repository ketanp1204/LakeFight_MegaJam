#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LFFishRuntime.generated.h"

class ALFBoat;
class ULFFishCargoComponent;

struct FLFFishBoat
{
	TWeakObjectPtr<ALFBoat> Boat;
	TWeakObjectPtr<ULFFishCargoComponent> Cargo;
};

/** Internal boat/cargo cache for fish simulation and presentation; includes boats spawned after BeginPlay. */
UCLASS()
class LAKEFIGHT_API ULFFishBoatCache : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	const TArray<FLFFishBoat>& GetBoats() const { return Boats; }
private:
	TArray<FLFFishBoat> Boats;
	FDelegateHandle SpawnHandle;
	void AddBoat(AActor* Actor);
};

namespace LF::Fish
{
	bool DebugEnabled();
	ALFBoat* PlayerBoat(UWorld* World);
	bool IsOpenWater(UWorld* World, const FVector& At);
	bool SpillPoint(ALFBoat* Boat, int32 Index, int32 Count, FVector& Out);
	ALFBoat* NearestCatcher(UWorld* World, const FVector& At, float Radius, float MaxSpeed);
}
