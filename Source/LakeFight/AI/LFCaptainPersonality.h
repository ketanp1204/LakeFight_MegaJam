#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LFCaptainPersonality.generated.h"

class UStaticMesh;

/**
 * T03 stage 2: an AI captain's personality (behaviour weights + look). Assign to a placed brawler's `Personality`;
 * ALFCaptainAI reads it on possession, ALFBrawler uses its hat / face piece.
 */
UCLASS(BlueprintType)
class LAKEFIGHT_API ULFCaptainPersonality : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Personality")
	FText DisplayName;

	// --- Look ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UStaticMesh> Hat;      // null = team bucket hat

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TObjectPtr<UStaticMesh> FacePiece; // beard / moustache / none

	// --- Reactions / aim ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	FVector2D ReactionDelay = FVector2D(0.15f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float AimErrorDeg = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float SwingCooldown = 1.1f;

	/** Chance (0..1) to use a charged swing even when the target isn't near the gunwale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float ChargedSwingChance = 0.2f;

	// --- Fight (with hysteresis) ---
	/** Stand up and fight when the target is this close (cm) ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float FightEnterRange = 190.f;

	/** ... and keep fighting until they are beyond this (cm) or out of reach for FightGiveUpTime (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float FightExitRange = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float FightGiveUpTime = 2.f;

	/** Leave the seat to fight when the enemy's boat is alongside within this (cm, gunwale distance). 0 = never board. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float BoardRange = 120.f;

	// --- Ram ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float RamRange = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float RamAngleDeg = 20.f;

	// --- Targeting (distance multipliers: < 1 = more tempting) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float DriverTargetWeight = 1.f;

	/** Targets whose boat seat is empty (stealable / undefended). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float EmptySeatTargetWeight = 1.f;

	/** When swimming: own boat's distance multiplier vs enemy empty boats (> 1 = prefers stealing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float OwnBoatRecoverWeight = 0.6f;

	// --- Flee ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	int32 FleeHits = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour")
	float FleeTime = 4.f;
};
