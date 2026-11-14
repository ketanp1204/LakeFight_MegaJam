#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LFFishCargoComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** Fish belong to the hull: changing drivers never changes this inventory. */
UCLASS(ClassGroup=(LF), meta=(BlueprintSpawnableComponent))
class LAKEFIGHT_API ULFFishCargoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULFFishCargoComponent();
	static ULFFishCargoComponent* Get(const AActor* Boat); // T04 hook

	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	int32 VisibleFishLimit = 8;

	UPROPERTY(EditDefaultsOnly, Category="LF|Fish")
	TObjectPtr<UStaticMesh> FishMesh;

	UFUNCTION(BlueprintPure, Category="LF|Fish")
	int32 GetCount() const { return Count; }

	void AddFish(int32 Amount);
	int32 RemoveFish(int32 Amount);
	void Spill(int32 Amount, const TCHAR* Reason = TEXT("unknown")); // T04 hook

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pile;

	UPROPERTY(Transient)
	int32 Count = 0;

	void UpdatePile();
};
