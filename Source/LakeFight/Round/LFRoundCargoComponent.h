#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LFRoundCargoComponent.generated.h"

/** Gold is part of the boat's fish count; this component tracks only its extra value. */
UCLASS()
class LAKEFIGHT_API ULFRoundCargoComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	static ULFRoundCargoComponent* Get(AActor* Boat);
	void AddGold();
	void Removing(int32 Amount, int32 CountBefore);
	int32 GetValue(int32 Count) const { return Count + Gold * 4; }
	int32 GetGold() const { return Gold; }
	int32 SpillGold();
private:
	UPROPERTY(Transient)
	int32 Gold = 0;
	int32 RemovedGold = 0;
};
