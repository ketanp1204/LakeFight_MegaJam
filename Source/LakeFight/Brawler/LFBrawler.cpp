#include "Brawler/LFBrawler.h"

#include "Boat/LFBoat.h"
#include "Input/LFInput.h"
#include "Water/LFWater.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

ALFBrawler::ALFBrawler()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // sway/camera read this frame's boat pose

	// A rowboat cockpit is ~1 m wide: slim capsule.
	GetCapsuleComponent()->InitCapsuleSize(30.f, 88.f);
	// QUERY-ONLY: the CMC still sweeps against (and stands on) hulls, but the capsule never makes physics contact,
	// so a character can't push, weigh down or launch any boat.
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
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -88.f), MeshBaseRotation);
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

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(RootComponent);
	CameraArm->SetUsingAbsoluteLocation(true); // pivot driven manually on real time (frames brawler + boat)
	CameraArm->bUsePawnControlRotation = true;
	CameraArm->TargetArmLength = 650.f;
	CameraArm->SocketOffset = FVector(0.f, 0.f, 90.f);
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
	EIC->BindAction(LF::Input::Get().Interact, ETriggerEvent::Started, this, &ALFBrawler::OnInteract);
}

void ALFBrawler::OnMove(const FInputActionValue& Value)
{
	if (State != ELFBrawlerState::Standing || !Controller)
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
	if (State == ELFBrawlerState::Standing)
	{
		TryDrive();
	}
}

void ALFBrawler::SetBoatMovementRules(bool bInBoat)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bCanWalkOffLedges = !bInBoat;
	Move->MaxStepHeight = bInBoat ? BoatStepHeight : 45.f;
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
	SetBoatMovementRules(true);

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Spot = InBoat->GetDriverSeatWorld() + FVector(0.f, 0.f, HalfHeight + 3.f);
	SetActorLocationAndRotation(Spot, FRotator(0.f, InBoat->GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->Velocity = FVector::ZeroVector;
	Move->SetMovementMode(MOVE_Walking);
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
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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

void ALFBrawler::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == ELFBrawlerState::Standing)
	{
		UpdateSway(DeltaSeconds);
	}
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
}
