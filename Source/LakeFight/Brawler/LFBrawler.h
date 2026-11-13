#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LFBrawler.generated.h"

class ALFBoat;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UENUM(BlueprintType)
enum class ELFBrawlerState : uint8
{
	None,
	Standing,   // InBoat, free to walk inside the boat (based on the hull)
	Driving,    // InBoat, attached at the driver seat; the PlayerController possesses the boat
	Swimming,   // T02 step 3
	Climbing,   // T02 step 3
	Ragdoll     // T02 step 2 (ragdoll-lite)
};

/**
 * T02 brawler: a character standing IN a physics boat.
 * Standing: the CMC walks on the hull (movement base = HullMesh) but the capsule is QUERY-ONLY, so it never pushes or
 * weighs on any boat (no physics contact; bEnablePhysicsInteraction off). Can't walk off ledges or step over the gunwale.
 * The mesh leans with the boat's roll/pitch (procedural sway). Driving: attached at the boat's driver seat and the
 * PlayerController possesses the boat (T01 controls + boat camera). Interact toggles.
 */
UCLASS()
class LAKEFIGHT_API ALFBrawler : public ACharacter
{
	GENERATED_BODY()

public:
	ALFBrawler();

	// --- Boat ---
	/** Player-controlled brawlers without a boat spawn one of these under them on possession. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Boat")
	TSubclassOf<ALFBoat> HomeBoatClass;

	/** Placed brawlers (dummies) stand in this boat at BeginPlay. */
	UPROPERTY(EditAnywhere, Category="LF|Boat")
	TObjectPtr<ALFBoat> StartBoat;

	/** Fraction of the boat's roll/pitch the mesh leans with (0 = stays upright). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Boat", meta=(ClampMin="0", ClampMax="1.5"))
	float SwayScale = 0.7f;

	/** How fast the lean follows the boat (1/s). */
	UPROPERTY(EditDefaultsOnly, Category="LF|Boat", meta=(ClampMin="0.1"))
	float SwayFollowSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Boat", meta=(ClampMin="0"))
	float BoatWalkSpeed = 220.f;

	/** Step height while in a boat - below the ~40 cm floor-to-gunwale so you can't step out. */
	UPROPERTY(EditDefaultsOnly, Category="LF|Boat", meta=(ClampMin="0"))
	float BoatStepHeight = 15.f;

	// --- Camera (third person, frames brawler + boat; real-time lag like the boat camera) ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0", ClampMax="1"))
	float CameraBoatFrame = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category="LF|Camera", meta=(ClampMin="0.1"))
	float CameraLagSpeed = 6.f;

	// --- Input (template assets for move/look/jump; Interact/Swing are runtime actions in LF::Input) ---
	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputMappingContext> MouseLookMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category="LF|Input")
	TObjectPtr<UInputAction> JumpAction;

	// --- API ---
	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	ELFBrawlerState GetBrawlerState() const { return State; }

	UFUNCTION(BlueprintPure, Category="LF|Brawler")
	ALFBoat* GetBoat() const { return Boat.Get(); }

	/** Put the brawler standing on the boat floor at the driver seat spot. */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	void StandInBoat(ALFBoat* InBoat);

	/** Standing -> Driving if the boat's seat is empty. The PlayerController possesses the boat. */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	bool TryDrive();

	/** Driving -> Standing; the PlayerController possesses the brawler again. */
	UFUNCTION(BlueprintCallable, Category="LF|Brawler")
	void LeaveDriving();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="LF|Camera")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category="LF|Camera")
	TObjectPtr<UCameraComponent> Camera;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnInteract();

	void SetBoatMovementRules(bool bInBoat);
	void UpdateSway(float DeltaSeconds);
	void UpdateCamera(float RealDelta);

	ELFBrawlerState State = ELFBrawlerState::None;
	TWeakObjectPtr<ALFBoat> Boat;
	FQuat SwayQuat = FQuat::Identity;
	FRotator MeshBaseRotation = FRotator(0.f, -90.f, 0.f);
	FVector CameraPivot = FVector::ZeroVector;
	bool bCameraPivotInit = false;
};
