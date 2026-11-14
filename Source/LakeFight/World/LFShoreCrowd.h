#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LFShoreCrowd.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;
class UMaterialInterface;

/**
 * N1 item 10 (stretch): two Manny spectators on the shore beside each team dock. They idle, cheer (Victory) when their
 * team banks, and react to rams within ~35 m (Cheering / Insult). Rams and banks are read by polling (scores, boat closing
 * speeds) - no gameplay hooks. NoCollision, one tick every 0.2 s. Spawned by ULFAmbientLifeSubsystem.
 */
UCLASS()
class LAKEFIGHT_API ALFShoreCrowd : public AActor
{
	GENERATED_BODY()
public:
	ALFShoreCrowd();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Dev (lf.crowd.look). */
	bool GetFan(int32 I, FVector& At, FVector& Facing) const;

private:
	struct FFan
	{
		TObjectPtr<USkeletalMeshComponent> Mesh;
		int32 Team = 0;
		float BusyUntil = 0.f;
	};
	TArray<FFan> Fans;
	int32 LastScores[4] = { 0, 0, 0, 0 };
	float NextRamCheck = 0.f;

	UPROPERTY() TObjectPtr<USkeletalMesh> Manny;
	UPROPERTY() TObjectPtr<UMaterialInterface> BodyMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> JointMat;
	UPROPERTY() TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> VictoryAnim;
	UPROPERTY() TObjectPtr<UAnimSequence> InsultAnim;

	void Play(FFan& Fan, UAnimSequence* Anim, float Now, float Hold);
	void Setup();
	bool bSetup = false;
};
