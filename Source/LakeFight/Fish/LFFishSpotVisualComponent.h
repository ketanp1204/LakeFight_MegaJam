#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LFFishSpotVisualComponent.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UNiagaraSystem;
class UMaterialInterface;

/** Mesh-only life for ordinary/gold spots: floating pieces, spreading ripples and a leaping fish. */
UCLASS()
class LAKEFIGHT_API ULFFishSpotVisualComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	ULFFishSpotVisualComponent();
	static void Add(AActor* Spot, bool bGold);
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, Category="LF|Fish|Look")
	FVector2D LeapInterval = FVector2D(2.f, 4.f);
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish|Look")
	FVector2D GoldLeapInterval = FVector2D(1.f, 2.f);
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish|Look")
	float LeapHeight = 110.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Fish|Look")
	float BobHeight = 7.f;
private:
	UPROPERTY() TObjectPtr<UStaticMesh> RingMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> RippleMaterial;
	UPROPERTY() TObjectPtr<UStaticMesh> FishMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> GoldMesh;
	UPROPERTY() TObjectPtr<UNiagaraSystem> Splash;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Leaper;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Rings;
	bool bGold = false;
	float NextLeap = 0.f;
	float LeapStart = -100.f;
	float LeapYaw = 0.f;
	bool bLeaping = false;
	UStaticMeshComponent* MakeMesh(UStaticMesh* Asset);
	void Land();
};
