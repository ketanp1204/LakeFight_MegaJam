#include "Brawler/LFBrawler.h"

#include "Boat/LFBoat.h"
#include "Input/LFInput.h"
#include "Water/LFWater.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Brawler scale (director, T02 step 1a): the whole character is 0.75 of the UE mannequin.
	constexpr float BrawlerScale = 0.75f;
	constexpr float CapsuleRadius = 30.f * BrawlerScale;
	constexpr float CapsuleHalfHeight = 88.f * BrawlerScale;

	// Paddle poses (pivot yaw/pitch, deg; pivot at the right shoulder, blade along +X).
	const FRotator PoseIdle(-60.f, 75.f, 0.f);
	const FRotator PoseWindup(15.f, 140.f, 0.f);
	const FRotator PoseFollow(0.f, -90.f, 0.f);
	constexpr float RecoverTime = 0.3f;
	constexpr float BladeCentreCm = 140.f; // grip -> blade centre along the paddle (blade spans 105-160 cm)

	FRotator LerpRot(const FRotator& A, const FRotator& B, float T)
	{
		return FRotator(FMath::Lerp(A.Pitch, B.Pitch, T), FMath::Lerp(A.Yaw, B.Yaw, T), FMath::Lerp(A.Roll, B.Roll, T));
	}
}

ALFBrawler::ALFBrawler()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // sway/camera/paddle read this frame's boat pose

	// A rowboat cockpit is ~1 m wide: slim capsule.
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	// QUERY-ONLY: the CMC still sweeps against (and stands on) hulls, and paddles can hit it, but the capsule never makes
	// physics contact, so a character can't push, weigh down or launch any boat.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannyMesh.Object);
	}
	static ConstructorHelpers::FClassFinder<UAnimInstance> UnarmedAnim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (UnarmedAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(UnarmedAnim.Class);
	}
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -CapsuleHalfHeight), MeshBaseRotation);
	GetMesh()->SetRelativeScale3D(FVector(BrawlerScale));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	AutoPossessAI = EAutoPossessAI::Disabled;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->MaxWalkSpeed = BoatWalkSpeed;
	Move->BrakingDecelerationWalking = 1500.f;
	Move->bEnablePhysicsInteraction = false;   // no standing weight / push forces on the hull
	Move->bRunPhysicsWithNoController = true;  // dummies (no controller) still stand and ride the boat
	Move->bCanWalkOffLedges = false;
	Move->MaxStepHeight = BoatStepHeight;

	PaddlePivot = CreateDefaultSubobject<USceneComponent>(TEXT("PaddlePivot"));
	PaddlePivot->SetupAttachment(RootComponent);
	PaddlePivot->SetRelativeLocation(FVector(0.f, 18.f, 55.f) * BrawlerScale); // right shoulder
	PaddlePivot->SetRelativeRotation(PoseIdle);

	Paddle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Paddle"));
	Paddle->SetupAttachment(PaddlePivot);
	Paddle->SetRelativeLocation(FVector(-10.f, 0.f, 0.f)); // held near the grip end: ~130 cm reach to the blade centre
	Paddle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Paddle->SetCastShadow(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PaddleMesh(TEXT("/Game/LakeFight/Meshes/SM_LF_Paddle.SM_LF_Paddle"));
	if (PaddleMesh.Succeeded())
	{
		Paddle->SetStaticMesh(PaddleMesh.Object);
	}
	PaddleRot = PoseIdle;

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(RootComponent);
	CameraArm->SetUsingAbsoluteLocation(true); // pivot driven manually on real time (frames brawler + boat)
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->TargetArmLength = 500.f;
	CameraArm->SocketOffset = FVector(0.f, 0.f, 70.f);
	CameraArm->bDoCollisionTest = false;
	CameraArm->bEnableCameraLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(80.f);

	static ConstructorHelpers::FClassFinder<ALFBoat> BoatBP(TEXT("/Game/LakeFight/Blueprints/BP_LF_Boat"));
	if (BoatBP.Succeeded())
	{
		HomeBoatClass = BoatBP.Class;
	}
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Splash(TEXT("/Game/LakeFight/FX/NS_LF_Splash.NS_LF_Splash"));
	if (Splash.Succeeded())
	{
		SplashSystem = Splash.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCMouse(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	static ConstructorHelpers::FObjectFinder<UInputAction> IAMove(TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	static ConstructorHelpers::FObjectFinder<UInputAction> IALook(TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	static ConstructorHelpers::FObjectFinder<UInputAction> IAMouseLook(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	static ConstructorHelpers::FObjectFinder<UInputAction> IAJump(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	DefaultMappingContext = IMC.Object;
	MouseLookMappingContext = IMCMouse.Object;
	MoveAction = IAMove.Object;
	LookAction = IALook.Object;
	MouseLookAction = IAMouseLook.Object;
	JumpAction = IAJump.Object;
}

void ALFBrawler::BeginPlay()
{
	Super::BeginPlay();
	BaseFov = Camera->FieldOfView;
	if (!Paddle->GetStaticMesh())
	{
		UE_LOG(LogLF, Warning, TEXT("ALFBrawler %s: SM_LF_Paddle missing - paddle invisible (hits still work)."), *GetName());
	}
	if (StartBoat && !Boat.IsValid())
	{
		StandInBoat(StartBoat);
	}
}

void ALFBrawler::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (!Boat.IsValid() && NewController && NewController->IsPlayerController() && HomeBoatClass)
	{
		// The player starts standing in their own boat, spawned on the water under the start spot.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector L = GetActorLocation();
		const FRotator R(0.f, GetActorRotation().Yaw, 0.f);
		if (ALFBoat* Home = GetWorld()->SpawnActor<ALFBoat>(HomeBoatClass, FVector(L.X, L.Y, LF::Water::BaseHeight), R, Params))
		{
			StandInBoat(Home);
			UE_LOG(LogLF, Display, TEXT("[brawler] %s spawned home boat %s"), *GetName(), *Home->GetName());
		}
	}
	if (NewController)
	{
		NewController->SetControlRotation(FRotator(-20.f, GetActorRotation().Yaw, 0.f));
	}
}

void ALFBrawler::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	const APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Sub->AddMappingContext(DefaultMappingContext, 0);
		}
		if (MouseLookMappingContext)
		{
			Sub->AddMappingContext(MouseLookMappingContext, 0);
		}
		Sub->AddMappingContext(LF::Input::Get().Context, 1);
	}
	bCameraPivotInit = false;
}

void ALFBrawler::UnPossessed()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Sub->RemoveMappingContext(DefaultMappingContext);
			}
			if (MouseLookMappingContext)
			{
				Sub->RemoveMappingContext(MouseLookMappingContext);
			}
			Sub->RemoveMappingContext(LF::Input::Get().Context);
		}
	}
	Super::UnPossessed();
}

void ALFBrawler::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogLF, Warning, TEXT("ALFBrawler %s: no Enhanced Input component - no input."), *GetName());
		return;
	}
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALFBrawler::OnMove);
	}
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALFBrawler::OnLook);
	}
	if (MouseLookAction)
	{
		EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ALFBrawler::OnLook);
	}
	// Jump is bound in step 4 (boat-to-boat); until swimming exists, leaving the boat would strand the character.
	const LF::Input::FActions& A = LF::Input::Get();
	EIC->BindAction(A.Interact, ETriggerEvent::Started, this, &ALFBrawler::OnInteract);
	EIC->BindAction(A.Swing, ETriggerEvent::Started, this, &ALFBrawler::OnSwingPressed);
	EIC->BindAction(A.Swing, ETriggerEvent::Completed, this, &ALFBrawler::OnSwingReleased);
}

bool ALFBrawler::IsStunned() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < StunUntil;
}

void ALFBrawler::OnMove(const FInputActionValue& Value)
{
	if (State != ELFBrawlerState::Standing || !Controller || IsStunned())
	{
		return;
	}
	const FVector2D V = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
}

void ALFBrawler::OnLook(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(V.Y);
}

void ALFBrawler::OnInteract()
{
	if (State == ELFBrawlerState::Standing && !IsStunned())
	{
		TryDrive();
	}
}

// ---------------------------------------------------------------- Boat

void ALFBrawler::SetBoatMovementRules(bool bInBoat)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bCanWalkOffLedges = !bInBoat;
	Move->MaxStepHeight = bInBoat ? BoatStepHeight : 45.f * BrawlerScale;
	Move->MaxWalkSpeed = BoatWalkSpeed;
}

void ALFBrawler::StandInBoat(ALFBoat* InBoat)
{
	if (!InBoat)
	{
		return;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Boat = InBoat;
	State = ELFBrawlerState::Standing;
	bInWater = false;
	SetBoatMovementRules(true);

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Spot = InBoat->GetDriverSeatWorld() + FVector(0.f, 0.f, HalfHeight + 3.f);
	SetActorLocationAndRotation(Spot, FRotator(0.f, InBoat->GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->Velocity = FVector::ZeroVector;
	Move->SetMovementMode(MOVE_Walking);
	GetMesh()->SetRelativeRotation(MeshBaseRotation);
	SwayQuat = FQuat::Identity;
	UE_LOG(LogLF, Display, TEXT("[brawler] %s Standing in %s"), *GetName(), *InBoat->GetName());
}

bool ALFBrawler::TryDrive()
{
	ALFBoat* B = Boat.Get();
	if (State != ELFBrawlerState::Standing || !B || B->HasDriver())
	{
		return false;
	}
	State = ELFBrawlerState::Driving;
	SwingPhase = ELFSwingPhase::Idle;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();
	// Query-only stays on so a driver can still be paddled out of the seat.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FTransform& Hull = B->GetHull()->GetComponentTransform();
	SetActorLocationAndRotation(B->GetDriverSeatWorld() + Hull.GetUnitAxis(EAxis::Z) * HalfHeight,
		FRotator(0.f, B->GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	AttachToComponent(B->GetHull(), FAttachmentTransformRules::KeepWorldTransform);
	GetMesh()->SetRelativeRotation(MeshBaseRotation);
	SwayQuat = FQuat::Identity;

	B->SetDriver(this);
	UE_LOG(LogLF, Display, TEXT("[brawler] %s Driving %s"), *GetName(), *B->GetName());
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->Possess(B); // T01 controls + boat camera
	}
	return true;
}

void ALFBrawler::LeaveDriving()
{
	ALFBoat* B = Boat.Get();
	if (State != ELFBrawlerState::Driving || !B)
	{
		return;
	}
	AController* C = B->GetController();
	B->SetDriver(nullptr);
	StandInBoat(B);
	if (APlayerController* PC = Cast<APlayerController>(C))
	{
		PC->Possess(this);
	}
}

// ---------------------------------------------------------------- Tick

void ALFBrawler::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == ELFBrawlerState::Standing)
	{
		UpdateSway(DeltaSeconds);
	}
	else if (State == ELFBrawlerState::Ragdoll)
	{
		UpdateRagdoll(DeltaSeconds);
	}
	UpdateFakeWeight(DeltaSeconds);
	UpdatePaddle(DeltaSeconds);
	if (IsLocallyControlled())
	{
		const UWorld* World = GetWorld();
		UpdateCamera(FMath::Min(World ? World->DeltaRealTimeSeconds : DeltaSeconds, 0.1f));
	}
}

void ALFBrawler::UpdateSway(float DeltaSeconds)
{
	const ALFBoat* B = Boat.Get();
	if (!B)
	{
		return;
	}
	// Lean the mesh (about the feet) with the boat's tilt; the capsule itself stays upright.
	const FVector BoatUp = B->GetHull()->GetComponentTransform().GetUnitAxis(EAxis::Z);
	const FQuat Full = FQuat::FindBetweenNormals(FVector::UpVector, BoatUp);
	const FQuat Target = FQuat::Slerp(FQuat::Identity, Full, SwayScale);
	SwayQuat = FQuat::Slerp(SwayQuat, Target, 1.f - FMath::Exp(-SwayFollowSpeed * DeltaSeconds));
	GetMesh()->SetWorldRotation(SwayQuat * (GetActorQuat() * MeshBaseRotation.Quaternion()));
}

void ALFBrawler::UpdateFakeWeight(float DeltaSeconds)
{
	ALFBoat* B = Boat.Get();
	if (!B || FakeWeightKg <= 0.f)
	{
		return;
	}
	UStaticMeshComponent* Hull = B->GetHull();
	if (!Hull->IsSimulatingPhysics())
	{
		return;
	}
	// Smoothed horizontal offset of the standing brawler from the hull's centre of mass; decays to zero otherwise
	// (driving at the stern adds none).
	const FVector Com = Hull->GetCenterOfMass();
	FVector Target = FVector::ZeroVector;
	if (State == ELFBrawlerState::Standing)
	{
		Target = GetActorLocation() - Com;
		Target.Z = 0.f;
		Target = Target.GetClampedToMaxSize(150.f);
	}
	FakeWeightOffset = FMath::Lerp(FakeWeightOffset, Target, 1.f - FMath::Exp(-FakeTiltFollowSpeed * DeltaSeconds));
	const float Arm = FakeWeightOffset.Size();
	if (Arm < 1.f)
	{
		return;
	}
	// Pure torque (no net force: no sinking, no added mass) = r x weight. It fades out as the boat's lean toward the
	// brawler approaches MaxFakeTiltDeg, so it can never tilt the boat past that by itself, let alone flip it.
	const FVector Dir = FakeWeightOffset / Arm;
	const FVector Up = Hull->GetComponentTransform().GetUnitAxis(EAxis::Z);
	const float LeanDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(FVector::DotProduct(Up, Dir)), -1.f, 1.f)));
	const float Fade = 1.f - FMath::SmoothStep(MaxFakeTiltDeg * 0.5f, MaxFakeTiltDeg, LeanDeg);
	if (Fade <= 0.f)
	{
		return;
	}
	const FVector Torque = FVector::CrossProduct(FakeWeightOffset, FVector(0.f, 0.f, -FakeWeightKg * 980.f));
	Hull->AddTorqueInRadians(Torque * Fade);
}

// ---------------------------------------------------------------- Paddle

FVector ALFBrawler::GetBladePoint() const
{
	return Paddle->GetComponentTransform().TransformPosition(FVector(BladeCentreCm, 0.f, 0.f));
}

void ALFBrawler::OnSwingPressed()
{
	if (State != ELFBrawlerState::Standing || IsStunned() || SwingPhase != ELFSwingPhase::Idle)
	{
		return;
	}
	SwingPhase = ELFSwingPhase::Charging;
	ChargeStartTime = GetWorld()->GetTimeSeconds();
	// Aim where the camera looks.
	if (Controller)
	{
		SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
	}
}

void ALFBrawler::OnSwingReleased()
{
	if (SwingPhase != ELFSwingPhase::Charging)
	{
		return;
	}
	const float Held = float(GetWorld()->GetTimeSeconds() - ChargeStartTime);
	const bool bCharged = Held >= ChargeMinTime;
	const float Charge = FMath::Clamp((Held - ChargeMinTime) / FMath::Max(ChargeFullTime - ChargeMinTime, 0.01f), 0.f, 1.f);
	SwingStrength = bCharged ? FMath::Lerp(ChargedMinStrength, 1.f, Charge) : LightStrength;
	SwingDuration = bCharged ? ChargedSwingTime : LightSwingTime;
	SwingPhase = ELFSwingPhase::Swinging;
	SwingAge = 0.f;
	bSwingHit = false;
	bHullHit = false;
	if (Controller)
	{
		SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
	}
	PaddlePivot->SetRelativeRotation(PoseWindup);
	PrevBladePoint = GetBladePoint();
	UE_LOG(LogLF, Display, TEXT("[paddle] %s swing %s strength=%.2f (held %.2f s)"), *GetName(), bCharged ? TEXT("CHARGED") : TEXT("light"), SwingStrength, Held);
}

void ALFBrawler::LFSwing(float Strength)
{
	if (State != ELFBrawlerState::Standing || IsStunned() || SwingPhase != ELFSwingPhase::Idle)
	{
		UE_LOG(LogLF, Display, TEXT("[paddle] LFSwing ignored (state/stun/busy)"));
		return;
	}
	SwingPhase = ELFSwingPhase::Charging;
	// Fake a hold that yields the requested strength, then release.
	const float Held = Strength < 0.f ? 0.f : ChargeMinTime + (FMath::Clamp(Strength, ChargedMinStrength, 1.f) - ChargedMinStrength)
		/ FMath::Max(1.f - ChargedMinStrength, 0.01f) * (ChargeFullTime - ChargeMinTime);
	ChargeStartTime = GetWorld()->GetTimeSeconds() - Held;
	OnSwingReleased();
}

void ALFBrawler::LFDrive()
{
	if (State == ELFBrawlerState::Driving)
	{
		LeaveDriving();
	}
	else
	{
		OnInteract();
	}
}

void ALFBrawler::UpdatePaddle(float DeltaSeconds)
{
	const float Follow = 1.f - FMath::Exp(-14.f * DeltaSeconds);
	switch (SwingPhase)
	{
	case ELFSwingPhase::Idle:
		PaddleRot = LerpRot(PaddleRot, PoseIdle, Follow);
		break;
	case ELFSwingPhase::Charging:
	{
		PaddleRot = LerpRot(PaddleRot, PoseWindup, Follow);
		// Full charge: a little tremble.
		const float Held = float(GetWorld()->GetTimeSeconds() - ChargeStartTime);
		if (Held >= ChargeFullTime)
		{
			PaddleRot.Roll = FMath::Sin(Held * 60.f) * 4.f;
		}
		break;
	}
	case ELFSwingPhase::Swinging:
	{
		SwingAge += DeltaSeconds;
		const float A = FMath::Clamp(SwingAge / SwingDuration, 0.f, 1.f);
		PaddleRot = LerpRot(PoseWindup, PoseFollow, FMath::SmoothStep(0.f, 1.f, A));
		PaddlePivot->SetRelativeRotation(PaddleRot);
		if (A > 0.1f && A < 0.95f)
		{
			SweepPaddle();
		}
		PrevBladePoint = GetBladePoint();
		if (A >= 1.f)
		{
			SwingPhase = ELFSwingPhase::Recover;
			SwingAge = 0.f;
		}
		return;
	}
	case ELFSwingPhase::Recover:
		SwingAge += DeltaSeconds;
		PaddleRot = LerpRot(PoseFollow, PoseIdle, FMath::SmoothStep(0.f, 1.f, FMath::Clamp(SwingAge / RecoverTime, 0.f, 1.f)));
		if (SwingAge >= RecoverTime)
		{
			SwingPhase = ELFSwingPhase::Idle;
		}
		break;
	}
	PaddlePivot->SetRelativeRotation(PaddleRot);
}

void ALFBrawler::SweepPaddle()
{
	UWorld* World = GetWorld();
	const FVector To = GetBladePoint();
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LFPaddle), false, this);
	if (ALFBoat* Own = Boat.Get())
	{
		Params.AddIgnoredActor(Own);
	}
	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, PrevBladePoint, To, FQuat::Identity, Objects, FCollisionShape::MakeSphere(PaddleHitRadius), Params);

	for (const FHitResult& Hit : Hits)
	{
		AActor* Other = Hit.GetActor();
		if (ALFBrawler* Victim = Cast<ALFBrawler>(Other))
		{
			if (bSwingHit || Victim == this || Victim->GetBrawlerState() == ELFBrawlerState::Ragdoll)
			{
				continue;
			}
			bSwingHit = true;
			const FVector Dir = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			const bool bOverboard = Victim->ReceivePaddleHit(this, Dir, SwingStrength);
			UE_LOG(LogLF, Display, TEXT("[paddle] %s HIT %s strength=%.2f -> %s"), *GetName(), *Victim->GetName(), SwingStrength,
				bOverboard ? TEXT("OVERBOARD") : TEXT("knockback+stun"));
			OnPaddleHit(Victim, SwingStrength, bOverboard);
			PlayHitFeedback(SwingStrength);
			StartHitstop(SwingStrength);
		}
		else if (ALFBoat* HitBoat = Cast<ALFBoat>(Other))
		{
			if (bHullHit || bSwingHit)
			{
				continue;
			}
			bHullHit = true;
			UPrimitiveComponent* HullComp = HitBoat->GetHull();
			const FVector Dir = (HitBoat->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			const FVector At = Hit.ImpactPoint.IsNearlyZero() ? To : FVector(Hit.ImpactPoint);
			HullComp->AddImpulseAtLocation((Dir + FVector(0.f, 0.f, -0.3f)) * (HullHitImpulse * SwingStrength * HullComp->GetMass()), At);
			UE_LOG(LogLF, Display, TEXT("[paddle] %s hit hull of %s strength=%.2f"), *GetName(), *HitBoat->GetName(), SwingStrength);
			OnPaddleHit(nullptr, SwingStrength, false);
			PlayHitFeedback(SwingStrength * 0.5f);
		}
	}
}

bool ALFBrawler::ReceivePaddleHit(ALFBrawler* Attacker, const FVector& Dir, float Strength)
{
	if (State == ELFBrawlerState::Driving)
	{
		LeaveDriving(); // knocked out of the seat (hands control back if a player was driving)
	}
	ALFBoat* B = Boat.Get();
	if (State != ELFBrawlerState::Standing || !B)
	{
		return false;
	}
	UWorld* World = GetWorld();
	StunUntil = World->GetTimeSeconds() + FMath::Lerp(StunMin, StunMax, Strength);
	SwingPhase = ELFSwingPhase::Recover;
	SwingAge = 0.f;

	const FTransform& Hull = B->GetHull()->GetComponentTransform();
	const FVector Local = Hull.InverseTransformPosition(GetActorLocation());
	const bool bNearGunwale = FMath::Abs(Local.Y) > GunwaleNearY || Local.X > 80.f || Local.X < -110.f;
	const bool bOverboard = Strength >= OverboardStrength || bNearGunwale;
	UE_LOG(LogLF, Display, TEXT("[paddle] %s hit at local (%.0f, %.0f) in %s: strength %.2f%s%s"), *GetName(), Local.X, Local.Y,
		*B->GetName(), Strength, Strength >= OverboardStrength ? TEXT(" >= overboard strength") : TEXT(""), bNearGunwale ? TEXT(" near gunwale") : TEXT(""));

	if (bOverboard)
	{
		// Launch up and out over the nearest side, mostly along the hit.
		const float Side = FMath::Abs(Local.Y) > 2.f ? FMath::Sign(float(Local.Y)) : (FVector::DotProduct(Dir, Hull.GetUnitAxis(EAxis::Y)) >= 0.f ? 1.f : -1.f);
		const FVector Out = (Dir + Hull.GetUnitAxis(EAxis::Y).GetSafeNormal2D() * Side * 0.8f).GetSafeNormal2D();
		State = ELFBrawlerState::Ragdoll;
		RagdollStart = World->GetTimeSeconds();
		bInWater = false;
		GetCharacterMovement()->bCanWalkOffLedges = true;
		LaunchCharacter(Out * OverboardSpeed + FVector(0.f, 0.f, OverboardPop), true, true);
		TumbleAxis = FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal();
		TumbleAngle = 0.f;
	}
	else
	{
		LaunchCharacter(Dir * FMath::Lerp(KnockbackMin, KnockbackMax, Strength) + FVector(0.f, 0.f, 120.f), true, true);
	}
	if (IsLocallyControlled())
	{
		PlayHitFeedback(Strength);
	}
	return bOverboard;
}

void ALFBrawler::UpdateRagdoll(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const FVector L = GetActorLocation();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (!bInWater)
	{
		// Comedy tumble about the feet while flying.
		TumbleAngle += 720.f * DeltaSeconds;
		const FQuat Tumble(TumbleAxis, FMath::DegreesToRadians(TumbleAngle));
		GetMesh()->SetWorldRotation(Tumble * (GetActorQuat() * MeshBaseRotation.Quaternion()));

		const float Water = LF::Water::GetHeightAt(L, World->GetTimeSeconds());
		if (L.Z - HalfHeight * 0.3f < Water && GetCharacterMovement()->Velocity.Z < 0.f)
		{
			bInWater = true;
			RagdollStart = World->GetTimeSeconds();
			GetCharacterMovement()->StopMovementImmediately();
			GetCharacterMovement()->DisableMovement();
			// Head-and-shoulders out, lying back in the water (placeholder until swimming in step 3).
			SetActorLocation(FVector(L.X, L.Y, Water - HalfHeight * 0.4f));
			GetMesh()->SetWorldRotation(FQuat(TumbleAxis, FMath::DegreesToRadians(-70.f)) * (GetActorQuat() * MeshBaseRotation.Quaternion()));
			const FVector SplashAt(L.X, L.Y, Water);
			if (SplashSystem)
			{
				if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SplashSystem, SplashAt))
				{
					NC->SetVariableFloat(TEXT("Strength"), 0.8f);
				}
			}
			OnSplashIn(SplashAt);
			UE_LOG(LogLF, Display, TEXT("[overboard] %s splash at (%.0f, %.0f)"), *GetName(), L.X, L.Y);
		}
		else if (World->GetTimeSeconds() - RagdollStart > 5.0)
		{
			bInWater = true; // stuck somewhere: let the reset below handle it
			RagdollStart = World->GetTimeSeconds();
		}
		return;
	}
	// In the water: bob, then (temporary, until step 3 swimming) go back into the boat.
	const float Water = LF::Water::GetHeightAt(L, World->GetTimeSeconds());
	SetActorLocation(FVector(L.X, L.Y, Water - HalfHeight * 0.4f));
	if (World->GetTimeSeconds() - RagdollStart >= OverboardResetDelay && Boat.IsValid())
	{
		UE_LOG(LogLF, Display, TEXT("[overboard] %s reset into %s (placeholder until swimming)"), *GetName(), *Boat->GetName());
		StandInBoat(Boat.Get());
	}
}

// ---------------------------------------------------------------- Feedback / camera (real time)

void ALFBrawler::PlayHitFeedback(float Strength)
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	ShakeAge = 0.f; // restart, never stack
	ShakeStrength = FMath::Max(Strength, 0.25f);
	FovPunchAge = 0.f;
	FovPunchStrength = FMath::Max(Strength, 0.25f);
	PC->PlayDynamicForceFeedback(0.6f * FMath::Max(Strength, 0.3f), 0.15f, true, true, true, true);
}

void ALFBrawler::StartHitstop(float Strength)
{
	UWorld* World = GetWorld();
	if (!World || HitstopDilation >= 1.f)
	{
		return;
	}
	const float RealSeconds = FMath::Lerp(HitstopMinTime, HitstopMaxTime, Strength);
	UGameplayStatics::SetGlobalTimeDilation(World, HitstopDilation);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld](float) -> bool
	{
		if (UWorld* W = WeakWorld.Get())
		{
			UGameplayStatics::SetGlobalTimeDilation(W, 1.f);
		}
		return false;
	}), RealSeconds);
}

void ALFBrawler::UpdateCamera(float RealDelta)
{
	FVector Target = GetActorLocation();
	if (const ALFBoat* B = Boat.Get())
	{
		Target = FMath::Lerp(Target, B->GetActorLocation(), CameraBoatFrame);
	}
	if (!bCameraPivotInit)
	{
		CameraPivot = Target;
		bCameraPivotInit = true;
	}
	CameraPivot = FMath::Lerp(CameraPivot, Target, 1.f - FMath::Exp(-CameraLagSpeed * RealDelta));
	CameraArm->SetWorldLocation(CameraPivot);

	FVector ShakeOffset = FVector::ZeroVector;
	if (ShakeAge >= 0.f)
	{
		ShakeAge += RealDelta;
		const float U = ShakeAge / ShakeDuration;
		if (U >= 1.f)
		{
			ShakeAge = -1.f;
		}
		else
		{
			const float Phase = ShakeAge * ShakeFrequency * UE_TWO_PI;
			const float Amp = ShakeAmplitude * ShakeStrength * FMath::Square(1.f - U);
			ShakeOffset = FVector(0.f, FMath::Sin(Phase * 0.5f) * Amp * 0.2f, FMath::Sin(Phase) * Amp);
		}
	}
	Camera->SetRelativeLocation(ShakeOffset);

	float Fov = BaseFov;
	if (FovPunchAge >= 0.f)
	{
		FovPunchAge += RealDelta;
		const float U = FovPunchAge / FovPunchTime;
		if (U >= 1.f)
		{
			FovPunchAge = -1.f;
		}
		else
		{
			Fov += FovPunchDeg * FovPunchStrength * FMath::Cube(1.f - U);
		}
	}
	Camera->SetFieldOfView(Fov);
}
