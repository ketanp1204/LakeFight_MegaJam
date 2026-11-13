#include "Brawler/LFBrawler.h"

#include "Brawler/LFBrawlerAnimInstance.h"
#include "AnimationRuntime.h"
#include "Engine/SkinnedAsset.h"
#include "Materials/MaterialInterface.h"

#include "Boat/LFBoat.h"
#include "Input/LFInput.h"
#include "Water/LFWater.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
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
	const FRotator PoseIdle(-50.f, -35.f, 0.f); // diagonal across the body, blade low front-left
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
	// Brawlers don't block each other's movement (two at one seat pushed one into the hull). Paddle sweeps query by
	// object type, so hits are unaffected.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MannyMesh.Object);
	}
	// Native anim instance: idle/walk + Two Bone IK of both arms onto the paddle (T02 step 5).
	GetMesh()->SetAnimInstanceClass(ULFBrawlerAnimInstance::StaticClass());
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -CapsuleHalfHeight), MeshBaseRotation);
	GetMesh()->SetRelativeScale3D(FVector(BrawlerScale));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Flat low-poly look (M_LF_CharacterFlat): body = cream/skin neutral, joints = WoodDark. Team identity is on hats + vests.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMat(TEXT("/Game/LakeFight/Materials/MI_LF_Char_Body.MI_LF_Char_Body"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> JointMat(TEXT("/Game/LakeFight/Materials/MI_LF_Char_Joints.MI_LF_Char_Joints"));
	if (BodyMat.Succeeded())
	{
		GetMesh()->SetMaterial(0, BodyMat.Object);
	}
	if (JointMat.Succeeded())
	{
		GetMesh()->SetMaterial(1, JointMat.Object);
	}
	// Same custom stencil as hulls (ALFBoat::HullStencil = 2): the lake treats water in front of a submerged body as deep,
	// so a swimmer's legs don't show through as a pale ghost. (Lake material/stencil value unchanged.)
	GetMesh()->SetRenderCustomDepth(true);
	GetMesh()->SetCustomDepthStencilValue(2);

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
	PaddlePivot->SetRelativeLocation(FVector(22.f, 12.f, 12.f)); // right-front of the waist: two-handed hold reachable by both arms
	PaddlePivot->SetRelativeRotation(PoseIdle);

	Paddle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Paddle"));
	Paddle->SetupAttachment(PaddlePivot);
	Paddle->SetRelativeLocation(FVector(-10.f, 0.f, 0.f)); // held near the grip end: ~130 cm reach to the blade centre
	Paddle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Paddle->SetUsingAbsoluteScale(true);
	Paddle->SetWorldScale3D(FVector(PaddleScale));
	Paddle->SetRenderCustomDepth(true);
	Paddle->SetCustomDepthStencilValue(2);
	Paddle->SetCastShadow(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PaddleMesh(TEXT("/Game/LakeFight/Meshes/SM_LF_Paddle.SM_LF_Paddle"));
	if (PaddleMesh.Succeeded())
	{
		Paddle->SetStaticMesh(PaddleMesh.Object);
	}
	PaddleRot = PoseIdle;

	// Crew gear (meshes authored for the 0.75 Manny: absolute scale 1, attached to bones so they follow the ragdoll).
	auto MakeGear = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* G = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		G->SetupAttachment(GetMesh());
		G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		G->SetUsingAbsoluteScale(true);
		G->SetRenderCustomDepth(true);
		G->SetCustomDepthStencilValue(2);
		return G;
	};
	Vest = MakeGear(TEXT("Vest"));
	Hat = MakeGear(TEXT("Hat"));
	Face = MakeGear(TEXT("Face"));

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
	ApplyCrewLook();
	Paddle->SetWorldScale3D(FVector(PaddleScale));
	if (!Paddle->GetStaticMesh())
	{
		UE_LOG(LogLF, Warning, TEXT("ALFBrawler %s: SM_LF_Paddle missing - paddle invisible (hits still work)."), *GetName());
	}
	if (StartBoat && !Boat.IsValid())
	{
		StandInBoat(StartBoat);
	}
}

void ALFBrawler::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyCrewLook(); // also in the editor, so placed crews show their gear
}

void ALFBrawler::AttachAligned(USceneComponent* Comp, FName Socket, FName Bone, const FVector& ActorOffset, const FRotator& ActorRot)
{
	// Gear/paddle authored in character space (X forward, Z up): attach to the bone (socket if present) with a relative
	// transform computed from the reference pose, so it sits aligned with the character and then rides the bone.
	USkeletalMeshComponent* M = GetMesh();
	const USkinnedAsset* Asset = M->GetSkinnedAsset();
	if (!Asset)
	{
		return;
	}
	const FReferenceSkeleton& RS = Asset->GetRefSkeleton();
	const int32 Idx = RS.FindBoneIndex(Bone);
	if (Idx == INDEX_NONE)
	{
		return;
	}
	const FTransform BoneCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(RS, Idx);
	const FTransform BoneActor = BoneCS * M->GetRelativeTransform();
	const FTransform Want(ActorRot.Quaternion(), BoneActor.GetLocation() + ActorOffset, FVector::OneVector);
	FTransform Rel = Want.GetRelativeTransform(BoneActor);
	Rel.SetScale3D(FVector::OneVector);
	const FName Attach = M->DoesSocketExist(Socket) ? Socket : Bone;
	Comp->AttachToComponent(M, FAttachmentTransformRules::KeepRelativeTransform, Attach);
	Comp->SetRelativeLocationAndRotation(Rel.GetLocation(), Rel.GetRotation());
}

void ALFBrawler::ApplyCrewLook()
{
	static const TCHAR* TeamNames[] = { TEXT("Red"), TEXT("Blue"), TEXT("Green"), TEXT("Yellow") };
	const TCHAR* T = TeamNames[FMath::Clamp(int32(Team), 0, 3)];
	UStaticMesh* VestMesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/LakeFight/Meshes/SM_LF_LifeVest_%s.SM_LF_LifeVest_%s"), T, T));
	UStaticMesh* HatMesh = HatOverride ? HatOverride.Get() : LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/LakeFight/Meshes/SM_LF_Hat_Bucket_%s.SM_LF_Hat_Bucket_%s"), T, T));
	Vest->SetStaticMesh(VestMesh);
	Hat->SetStaticMesh(HatMesh);
	Face->SetStaticMesh(FacePiece);
	AttachAligned(Vest, NAME_None, TEXT("spine_03"), FVector::ZeroVector, FRotator::ZeroRotator);
	AttachAligned(Hat, NAME_None, TEXT("head"), FVector::ZeroVector, FRotator::ZeroRotator);
	AttachAligned(Face, NAME_None, TEXT("head"), FVector::ZeroVector, FRotator::ZeroRotator);
	Vest->SetWorldScale3D(FVector::OneVector);
	Hat->SetWorldScale3D(FVector::OneVector);
	Face->SetWorldScale3D(FVector::OneVector);
}

void ALFBrawler::GetAnimView(FLFBrawlerAnimView& Out) const
{
	Out.State = State;
	Out.SwingPhase = uint8(SwingPhase);
	Out.SwingAlpha = SwingPhase == ELFSwingPhase::Swinging ? FMath::Clamp(SwingAge / FMath::Max(SwingDuration, 0.01f), 0.f, 1.f)
		: (SwingPhase == ELFSwingPhase::Recover ? FMath::Clamp(SwingAge / RecoverTime, 0.f, 1.f) : 0.f);
	Out.bStunned = IsStunned();
	Out.ClimbAlpha = FMath::Clamp(ClimbAge / FMath::Max(ClimbTime, 0.01f), 0.f, 1.f);
}

bool ALFBrawler::GetPaddleGripTargets(FVector& OutRight, FVector& OutLeft) const
{
	if (PaddleMount != EPaddleMount::Hands || State != ELFBrawlerState::Standing || !Paddle->GetStaticMesh())
	{
		return false;
	}
	const FTransform& P = Paddle->GetComponentTransform();
	OutRight = P.TransformPosition(GripROffset);
	OutLeft = P.TransformPosition(GripLOffset);
	return true;
}

void ALFBrawler::UpdatePaddleStow()
{
	// Standing: in the hands (pivot + IK). Driving / swimming / climbing: stowed on the back. Ragdoll: right hand (set in StartRagdoll).
	const EPaddleMount Want = State == ELFBrawlerState::Standing ? EPaddleMount::Hands
		: (State == ELFBrawlerState::Ragdoll ? EPaddleMount::Hand : EPaddleMount::Back);
	if (Want == PaddleMount || Want == EPaddleMount::Hand)
	{
		return;
	}
	PaddleMount = Want;
	if (Want == EPaddleMount::Hands)
	{
		Paddle->AttachToComponent(PaddlePivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Paddle->SetRelativeLocationAndRotation(FVector(-10.f, 0.f, 0.f), FRotator::ZeroRotator);
	}
	else
	{
		// Diagonal across the back: T-grip over the right shoulder, blade low on the left.
		const FVector Dir = FVector(0.f, -0.55f, -0.83f).GetSafeNormal();
		AttachAligned(Paddle, TEXT("PaddleBack"), TEXT("spine_03"), FVector(-17.f, 16.f, 22.f), Dir.Rotation());
	}
	Paddle->SetWorldScale3D(FVector(PaddleScale));
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

bool ALFBrawler::CanSwing() const
{
	return State == ELFBrawlerState::Standing && !IsStunned() && SwingPhase == ELFSwingPhase::Idle;
}

bool ALFBrawler::IsNearGunwale() const
{
	const ALFBoat* B = Boat.Get();
	if (!B || State != ELFBrawlerState::Standing)
	{
		return false;
	}
	const FVector L = B->GetHull()->GetComponentTransform().InverseTransformPosition(GetActorLocation());
	return FMath::Abs(L.Y) > GunwaleNearY || L.X > 80.f || L.X < -110.f;
}

int32 ALFBrawler::RecentHits(float Window) const
{
	const double Now = GetWorld()->GetTimeSeconds();
	int32 N = 0;
	for (double T : HitTimes)
	{
		N += (Now - T) <= Window ? 1 : 0;
	}
	return N;
}

bool ALFBrawler::IsStunned() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < StunUntil;
}

void ALFBrawler::OnMove(const FInputActionValue& Value)
{
	const bool bCanMove = State == ELFBrawlerState::Standing || State == ELFBrawlerState::Swimming;
	if (!bCanMove || !Controller || IsStunned())
	{
		return;
	}
	bHasSwimTarget = false; // player input overrides an AI swim target
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
	if (IsStunned())
	{
		return;
	}
	if (State == ELFBrawlerState::Standing)
	{
		TryDrive();
	}
	else if (State == ELFBrawlerState::Swimming)
	{
		TryGrabNearestHull();
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
	if (InBoat)
	{
		StandInBoatAt(InBoat, InBoat->DriverSeatLocal);
	}
}

void ALFBrawler::StandInBoatAt(ALFBoat* InBoat, const FVector& LocalSpot)
{
	if (!InBoat)
	{
		return;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Boat = InBoat;
	if (!HomeBoat.IsValid())
	{
		HomeBoat = InBoat; // the first boat you stand in is home
	}
	State = ELFBrawlerState::Standing;
	bInWater = false;
	bHasSwimTarget = false;
	StandFallTime = 0.f;
	SetBoatMovementRules(true);

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Spot = InBoat->GetHull()->GetComponentTransform().TransformPosition(LocalSpot) + FVector(0.f, 0.f, HalfHeight + 3.f);
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
	// Interact AT the seat.
	if (FVector::Dist2D(GetActorLocation(), B->GetDriverSeatWorld()) > DriveReach)
	{
		UE_LOG(LogLF, Display, TEXT("[brawler] %s too far from %s's driver seat (%.0f cm > %.0f)"), *GetName(), *B->GetName(),
			FVector::Dist2D(GetActorLocation(), B->GetDriverSeatWorld()), DriveReach);
		return false;
	}
	if (B != HomeBoat.Get())
	{
		UE_LOG(LogLF, Display, TEXT("[steal] %s steals %s"), *GetName(), *B->GetName());
		OnSteal(B);
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
		// Safety net: a standing brawler should never fall for long (knockback hops last ~0.3 s). If he does - pushed into
		// the hull, boat yanked away - put him back on the floor where he is, or into the water if he is outside the boat.
		StandFallTime = GetCharacterMovement()->IsFalling() ? StandFallTime + DeltaSeconds : 0.f;
		if (StandFallTime > 0.8f && Boat.IsValid())
		{
			ALFBoat* B = Boat.Get();
			FVector2D Unused;
			const float D = B->GunwaleDistance(GetActorLocation(), Unused);
			const FVector Local = B->GetHull()->GetComponentTransform().InverseTransformPosition(GetActorLocation());
			UE_LOG(LogLF, Display, TEXT("[brawler] %s fell %.1f s while standing in %s (local %.0f, %.0f, %.0f; gunwale dist %.0f) -> %s"),
				*GetName(), StandFallTime, *B->GetName(), Local.X, Local.Y, Local.Z, D, D < 0.f ? TEXT("re-snap to the floor") : TEXT("swim"));
			StandFallTime = 0.f;
			if (D < 0.f)
			{
				StandInBoatAt(B, FVector(FMath::Clamp(float(Local.X), -100.f, 75.f), FMath::Clamp(float(Local.Y), -14.f, 14.f), B->DriverSeatLocal.Z));
			}
			else
			{
				EnterSwimming();
			}
		}
	}
	else if (State == ELFBrawlerState::Ragdoll)
	{
		UpdateRagdoll(DeltaSeconds);
	}
	else if (State == ELFBrawlerState::Swimming)
	{
		UpdateSwim(DeltaSeconds);
	}
	else if (State == ELFBrawlerState::Climbing)
	{
		UpdateClimb(DeltaSeconds);
	}
	UpdateFakeWeight(DeltaSeconds);
	UpdatePaddleStow();
	UpdatePaddle(DeltaSeconds);
	if (IsLocallyControlled())
	{
		const UWorld* World = GetWorld();
		UpdateCamera(FMath::Min(World ? World->DeltaRealTimeSeconds : DeltaSeconds, 0.1f));
		UpdatePrompt();
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
	Swing(Strength);
}

void ALFBrawler::Swing(float Strength)
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

void ALFBrawler::LFOverboard(float Side)
{
	if (State == ELFBrawlerState::Driving)
	{
		LeaveDriving();
	}
	if (State == ELFBrawlerState::Standing)
	{
		StartRagdoll(GetActorRightVector().GetSafeNormal2D() * (Side >= 0.f ? 1.f : -1.f), 0.3f);
	}
}

void ALFBrawler::LFCam(float YawOffset, float Pitch)
{
	if (Controller)
	{
		Controller->SetControlRotation(FRotator(Pitch, GetActorRotation().Yaw + YawOffset, 0.f));
	}
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
	if (State == ELFBrawlerState::Ragdoll)
	{
		return false;
	}
	if (State == ELFBrawlerState::Swimming || State == ELFBrawlerState::Climbing)
	{
		// Knocked off the side / back into the water.
		if (State == ELFBrawlerState::Climbing)
		{
			UE_LOG(LogLF, Display, TEXT("[climb] %s knocked off %s"), *GetName(), ClimbBoat.IsValid() ? *ClimbBoat->GetName() : TEXT("?"));
		}
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		ClimbBoat = nullptr;
		Boat = nullptr;
		StartRagdoll(Dir, Strength * 0.6f);
		if (IsLocallyControlled())
		{
			PlayHitFeedback(Strength);
		}
		return true;
	}
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
	HitTimes.Add(World->GetTimeSeconds());
	if (HitTimes.Num() > 8)
	{
		HitTimes.RemoveAt(0);
	}
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
		// Out over the nearest side, mostly along the hit.
		const float Side = FMath::Abs(Local.Y) > 2.f ? FMath::Sign(float(Local.Y)) : (FVector::DotProduct(Dir, Hull.GetUnitAxis(EAxis::Y)) >= 0.f ? 1.f : -1.f);
		const FVector Out = (Dir + Hull.GetUnitAxis(EAxis::Y).GetSafeNormal2D() * Side * 0.8f).GetSafeNormal2D();
		StartRagdoll(Out, Strength);
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

void ALFBrawler::StartRagdoll(const FVector& Out, float Strength)
{
	UWorld* World = GetWorld();
	USkeletalMeshComponent* M = GetMesh();
	State = ELFBrawlerState::Ragdoll;
	RagdollPhase = ERagdollPhase::Flying;
	RagdollStart = LaunchTime = World->GetTimeSeconds();
	bInWater = false;
	bRagdollIgnoresBoats = false;
	ApexZ = -1e9f;
	GunwaleZ = float(GetActorLocation().Z);
	if (const ALFBoat* B = Boat.Get())
	{
		GunwaleZ = float(B->GetHull()->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, 25.f)).Z);
	}

	// The capsule stops driving the body.
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Paddle stays in the right hand for now.
	Paddle->AttachToComponent(M, FAttachmentTransformRules::KeepWorldTransform, M->DoesSocketExist(TEXT("PaddleGrip_R")) ? FName(TEXT("PaddleGrip_R")) : FName(TEXT("hand_r")));
	PaddleMount = EPaddleMount::Hand;

	// Ragdoll: Manny's physics asset (PA_Mannequin on the mesh), Ragdoll profile, light bodies so a flailing ragdoll can't
	// shove a 180 kg hull while it is still allowed to touch it.
	M->SetCollisionProfileName(TEXT("Ragdoll"));
	M->SetAllMassScale(RagdollMassScale);
	M->SetSimulatePhysics(true);
	M->SetAllBodiesPhysicsBlendWeight(1.f);
	M->WakeAllRigidBodies();

	// Mostly horizontal away from the hitter, small upward; the upper body gets extra horizontal speed so the legs catch the
	// rail and he topples backward over it.
	const float Horiz = FMath::Lerp(OverboardHorizSpeedMin, OverboardHorizSpeedMax, Strength);
	const float Up = FMath::Lerp(OverboardUpSpeedMin, OverboardUpSpeedMax, Strength);
	const FVector Base = Out * Horiz + FVector(0.f, 0.f, Up);
	M->SetAllPhysicsLinearVelocity(Base);
	static const FName UpperBones[] = { TEXT("spine_03"), TEXT("spine_05"), TEXT("neck_01"), TEXT("head") };
	for (const FName& Bone : UpperBones)
	{
		if (FBodyInstance* BI = M->GetBodyInstance(Bone))
		{
			BI->SetLinearVelocity(Base + Out * OverboardToppleSpeed, false);
		}
	}
	UE_LOG(LogLF, Display, TEXT("[overboard] %s ragdoll launch horiz=%.0f up=%.0f topple=+%.0f cm/s (strength %.2f, %d bodies)"), *GetName(),
		Horiz, Up, OverboardToppleSpeed, Strength, M->Bodies.Num());
}

void ALFBrawler::UpdateWaterForces(float DeltaSeconds)
{
	// Per-body buoyancy + strong damping for every body below the surface: limp, quickly settling bob.
	USkeletalMeshComponent* M = GetMesh();
	const float Time = GetWorld()->GetTimeSeconds();
	for (FBodyInstance* BI : M->Bodies)
	{
		if (!BI || !BI->IsInstanceSimulatingPhysics())
		{
			continue;
		}
		const FVector P = BI->GetUnrealWorldTransform().GetLocation();
		const float Depth = LF::Water::GetHeightAt(P, Time) - float(P.Z);
		if (Depth <= 0.f)
		{
			continue;
		}
		const FName Bone = BI->BodySetup.IsValid() ? BI->BodySetup->BoneName : NAME_None;
		const bool bCore = Bone == TEXT("pelvis") || Bone == TEXT("spine_03") || Bone == TEXT("spine_05") || Bone == TEXT("head");
		const float Buoy = WaterBuoyancyAccel * (bCore ? 1.f : WaterLimbBuoyancyScale) * FMath::Clamp(Depth / 25.f, 0.f, 1.f);
		const FVector Accel = FVector(0.f, 0.f, Buoy) - BI->GetUnrealWorldVelocity() * WaterDamping;
		BI->AddForce(Accel, true, /*bAccelChange*/ true);
	}
}

void ALFBrawler::UpdateRagdoll(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	USkeletalMeshComponent* M = GetMesh();
	const double Now = World->GetTimeSeconds();
	const FVector Pelvis = M->GetBoneLocation(TEXT("pelvis"));
	const float Water = LF::Water::GetHeightAt(Pelvis, float(Now));
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	if (RagdollPhase == ERagdollPhase::Flying || RagdollPhase == ERagdollPhase::Floating || RagdollPhase == ERagdollPhase::Recovering)
	{
		// Touch hulls only briefly (the rail trips him), then ignore boats so a flailing ragdoll can't shove one.
		if (!bRagdollIgnoresBoats && Now - LaunchTime >= RagdollHullCollideTime)
		{
			M->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
			M->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
			M->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			bRagdollIgnoresBoats = true;
		}
		UpdateWaterForces(DeltaSeconds);
	}

	switch (RagdollPhase)
	{
	case ERagdollPhase::Flying:
		ApexZ = FMath::Max(ApexZ, float(Pelvis.Z));
		if (Pelvis.Z < Water || Now - RagdollStart > 3.0)
		{
			RagdollPhase = ERagdollPhase::Floating;
			RagdollStart = Now;
			bInWater = true;
			const FVector SplashAt(Pelvis.X, Pelvis.Y, Water);
			if (SplashSystem)
			{
				if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SplashSystem, SplashAt, FRotator::ZeroRotator, FVector(OverboardSplashScale)))
				{
					NC->SetVariableFloat(TEXT("Strength"), 1.f);
				}
			}
			OnSplashIn(SplashAt);
			// Landing distance outside the victim boat's hull (local; half-beam 67 cm, half-length 158 cm).
			float Outside = -1.f;
			if (const ALFBoat* B = Boat.Get())
			{
				const FVector Lp = B->GetHull()->GetComponentTransform().InverseTransformPosition(Pelvis);
				Outside = FMath::Max3(FMath::Abs(float(Lp.Y)) - 67.f, FMath::Abs(float(Lp.X)) - 158.f, 0.f);
			}
			UE_LOG(LogLF, Display, TEXT("[overboard] %s splash: apex %.0f cm above the gunwale, landed %.0f cm outside the hull, flight %.2f s"),
				*GetName(), ApexZ - GunwaleZ, Outside, float(Now - LaunchTime));
		}
		break;

	case ERagdollPhase::Floating:
	{
		const float Elapsed = float(Now - RagdollStart);
		const float Speed = float(M->GetPhysicsLinearVelocity(TEXT("pelvis")).Size());
		if (Elapsed >= FloatTime || (Elapsed > 0.6f && Speed < FloatStillSpeed))
		{
			UE_LOG(LogLF, Display, TEXT("[overboard] %s floated %.2f s (pelvis speed %.0f cm/s) -> recover"), *GetName(), Elapsed, Speed);
			// Put the (animated) capsule upright at the pelvis, half in the water, then blend the ragdoll pose into it.
			// No teleport: the simulated bodies stay where they are while the blend weight falls.
			const float Yaw = M->GetBoneQuaternion(TEXT("pelvis")).GetForwardVector().GetSafeNormal2D().Rotation().Yaw;
			SetActorLocationAndRotation(FVector(Pelvis.X, Pelvis.Y, Water - HalfHeight * 0.4f), FRotator(0.f, Yaw, 0.f));
			RagdollPhase = ERagdollPhase::Recovering;
			RecoverAge = 0.f;
		}
		break;
	}

	case ERagdollPhase::Recovering:
	{
		RecoverAge += DeltaSeconds;
		const FVector L = GetActorLocation();
		SetActorLocation(FVector(L.X, L.Y, LF::Water::GetHeightAt(L, float(Now)) - HalfHeight * 0.4f));
		M->SetAllBodiesPhysicsBlendWeight(FMath::Clamp(1.f - RecoverAge / RecoverBlendTime, 0.f, 1.f));
		if (RecoverAge >= RecoverBlendTime)
		{
			EndRagdoll();
			RagdollPhase = ERagdollPhase::Upright;
			EnterSwimming();
		}
		break;
	}

	case ERagdollPhase::Upright:
		break;
	}
}

void ALFBrawler::EndRagdoll()
{
	USkeletalMeshComponent* M = GetMesh();
	M->SetSimulatePhysics(false);
	M->SetAllBodiesPhysicsBlendWeight(0.f);
	M->SetAllMassScale(1.f);
	M->SetCollisionProfileName(TEXT("CharacterMesh"));
	M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Simulating may have detached the mesh: put it back on the capsule.
	M->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	M->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), MeshBaseRotation);
	PaddleMount = EPaddleMount::Hands; // force a re-mount by UpdatePaddleStow (-> back while swimming)
	Paddle->AttachToComponent(PaddlePivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SwayQuat = FQuat::Identity;
	UE_LOG(LogLF, Display, TEXT("[overboard] %s upright in the water"), *GetName());
}

// ---------------------------------------------------------------- Swim / climb / steal (step 3)

void ALFBrawler::EnterSwimming()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	State = ELFBrawlerState::Swimming;
	Boat = nullptr; // not in a boat: no fake weight, the camera frames just us
	bInWater = true;
	bHasSwimTarget = false;
	SwimStartTime = GetWorld()->GetTimeSeconds();
	SwingPhase = ELFSwingPhase::Idle;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->SetMovementMode(MOVE_Flying);
	Move->MaxFlySpeed = SwimSpeed;
	Move->BrakingDecelerationFlying = 300.f;
	Move->Velocity = FVector::ZeroVector;
	UE_LOG(LogLF, Display, TEXT("[swim] %s swimming"), *GetName());
}

void ALFBrawler::StartSwimTo(FVector Target)
{
	SwimTarget = Target;
	bHasSwimTarget = true;
}

void ALFBrawler::StopSwim()
{
	bHasSwimTarget = false;
}

float ALFBrawler::GrabDistanceTo(const ALFBoat* B, FVector2D* OutLocal) const
{
	if (!B)
	{
		return TNumericLimits<float>::Max();
	}
	FVector2D Local;
	const float D = B->GunwaleDistance(GetActorLocation(), Local);
	if (OutLocal)
	{
		*OutLocal = Local;
	}
	return D;
}

ALFBoat* ALFBrawler::FindGrabbableHull() const
{
	ALFBoat* Best = nullptr;
	float BestD = GrabRange;
	for (TActorIterator<ALFBoat> It(GetWorld()); It; ++It)
	{
		const float D = GrabDistanceTo(*It);
		if (D <= BestD)
		{
			BestD = D;
			Best = *It;
		}
	}
	return Best;
}

bool ALFBrawler::TryGrabNearestHull()
{
	return TryGrabHull(FindGrabbableHull());
}

bool ALFBrawler::TryGrabHull(ALFBoat* Hull)
{
	if (State != ELFBrawlerState::Swimming || !Hull || IsStunned())
	{
		return false;
	}
	FVector2D Nearest;
	const float D = GrabDistanceTo(Hull, &Nearest);
	if (D > GrabRange)
	{
		return false;
	}
	// Climb onto the side nearest you: land on the floor just inside that side (floor is ~36 cm each side of the centreline).
	const FTransform& HT = Hull->GetHull()->GetComponentTransform();
	const float Side = Nearest.Y >= 0.f ? 1.f : -1.f;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	ClimbTargetLocal = FVector(FMath::Clamp(float(Nearest.X), -100.f, 75.f), Side * 14.f, Hull->DriverSeatLocal.Z + HalfHeight + 3.f);
	ClimbStartLocal = HT.InverseTransformPosition(GetActorLocation());
	ClimbBoat = Hull;
	ClimbAge = 0.f;
	State = ELFBrawlerState::Climbing;
	bHasSwimTarget = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();
	UE_LOG(LogLF, Display, TEXT("[climb] %s grabs %s (%.0f cm from the gunwale, %s side)"), *GetName(), *Hull->GetName(), D,
		Side > 0.f ? TEXT("starboard") : TEXT("port"));
	return true;
}

void ALFBrawler::UpdateSwim(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	FVector L = GetActorLocation();

	// AI / dummy: swim home and climb in.
	if (!Controller && bAutoReturnHome && HomeBoat.IsValid() && Now - SwimStartTime >= AutoReturnDelay)
	{
		if (GrabDistanceTo(HomeBoat.Get()) <= GrabRange * 0.8f)
		{
			TryGrabHull(HomeBoat.Get());
			return;
		}
		StartSwimTo(HomeBoat->GetActorLocation());
	}
	if (bHasSwimTarget)
	{
		const FVector To = (SwimTarget - L) * FVector(1.f, 1.f, 0.f);
		if (To.Size() < 30.f)
		{
			bHasSwimTarget = false;
		}
		else
		{
			AddMovementInput(To.GetSafeNormal(), 1.f);
		}
	}

	// Hulls: the capsule is query-only (it never pushes a boat), so a boat would pass straight through a swimmer. Push the
	// swimmer out to the hull outline instead; a fast boat runs him over.
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	for (TActorIterator<ALFBoat> It(World); It; ++It)
	{
		ALFBoat* B = *It;
		FVector2D NearLocal;
		const float D = B->GunwaleDistance(L, NearLocal);
		if (D >= Radius)
		{
			continue;
		}
		const FVector NearWorld = B->GetHull()->GetComponentTransform().TransformPosition(FVector(NearLocal.X, NearLocal.Y, 0.f));
		FVector Out = (D >= 0.f ? (L - NearWorld) : (NearWorld - L)).GetSafeNormal2D();
		if (Out.IsNearlyZero())
		{
			Out = (L - B->GetActorLocation()).GetSafeNormal2D();
		}
		const FVector BoatVel = B->GetVelocity() * FVector(1.f, 1.f, 0.f);
		if (BoatVel.Size() > RunOverSpeed && FVector::DotProduct(BoatVel, Out) > 0.f)
		{
			UE_LOG(LogLF, Display, TEXT("[swim] %s run over by %s at %.0f cm/s"), *GetName(), *B->GetName(), BoatVel.Size());
			StartRagdoll((BoatVel.GetSafeNormal() + Out).GetSafeNormal2D(), 0.2f);
			return;
		}
		L.X = NearWorld.X + Out.X * (Radius + 2.f);
		L.Y = NearWorld.Y + Out.Y * (Radius + 2.f);
	}

	// Bob on the water: capsule centre a fixed fraction of the half-height under the surface.
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Water = LF::Water::GetHeightAt(L, Now);
	L.Z = Water - HalfHeight * SwimSubmerge;
	SetActorLocation(L);
	GetCharacterMovement()->Velocity.Z = 0.f;

	// Swim stroke hook (audio later): about every 0.7 s while moving.
	if (GetVelocity().Size2D() > 30.f)
	{
		SwimStrokeAccum += DeltaSeconds;
		if (SwimStrokeAccum >= 0.7f)
		{
			SwimStrokeAccum = 0.f;
			OnSwimStroke();
		}
	}

	// Placeholder swim pose: lean forward with speed.
	const float Speed01 = FMath::Clamp(float(GetVelocity().Size2D()) / FMath::Max(SwimSpeed, 1.f), 0.f, 1.f);
	const FQuat Lean(GetActorRightVector(), FMath::DegreesToRadians(SwimLeanDeg * Speed01));
	SwayQuat = FQuat::Slerp(SwayQuat, Lean, 1.f - FMath::Exp(-6.f * DeltaSeconds));
	GetMesh()->SetWorldRotation(SwayQuat * (GetActorQuat() * MeshBaseRotation.Quaternion()));
}

void ALFBrawler::UpdateClimb(float DeltaSeconds)
{
	ALFBoat* B = ClimbBoat.Get();
	if (!B)
	{
		EnterSwimming();
		return;
	}
	ClimbAge += DeltaSeconds;
	const float T = FMath::Clamp(ClimbAge / ClimbTime, 0.f, 1.f);
	// In hull space so a moving/rocking boat carries the climb. Phase 1 (60 %): rise at the side to above the rail,
	// moving a little toward it. Phase 2: hop over and drop onto the floor.
	const FVector Over = FVector(ClimbTargetLocal.X, FMath::Lerp(ClimbStartLocal.Y, ClimbTargetLocal.Y, 0.35f), ClimbTargetLocal.Z + ClimbOverHeight);
	FVector Local;
	if (T < 0.6f)
	{
		const float A = FMath::SmoothStep(0.f, 1.f, T / 0.6f);
		Local = FMath::Lerp(ClimbStartLocal, Over, A);
	}
	else
	{
		const float A = (T - 0.6f) / 0.4f;
		Local = FMath::Lerp(Over, ClimbTargetLocal, FMath::SmoothStep(0.f, 1.f, A));
		Local.Z += FMath::Sin(A * UE_PI) * 12.f; // small hop
	}
	const FTransform& HT = B->GetHull()->GetComponentTransform();
	SetActorLocationAndRotation(HT.TransformPosition(Local), FRotator(0.f, (HT.TransformVector(ClimbTargetLocal - ClimbStartLocal) * FVector(1, 1, 0)).Rotation().Yaw, 0.f));
	// Lean into the climb, then straighten.
	const FQuat Lean(GetActorRightVector(), FMath::DegreesToRadians(35.f * FMath::Sin(T * UE_PI)));
	GetMesh()->SetWorldRotation(Lean * (GetActorQuat() * MeshBaseRotation.Quaternion()));
	if (T >= 1.f)
	{
		FinishClimb();
	}
}

void ALFBrawler::FinishClimb()
{
	ALFBoat* B = ClimbBoat.Get();
	ClimbBoat = nullptr;
	if (!B)
	{
		EnterSwimming();
		return;
	}
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	StandInBoatAt(B, ClimbTargetLocal - FVector(0.f, 0.f, HalfHeight + 3.f));
	OnClimbAboard(B);
	UE_LOG(LogLF, Display, TEXT("[climb] %s aboard %s%s"), *GetName(), *B->GetName(),
		B == HomeBoat.Get() ? TEXT(" (home)") : (B->HasDriver() ? TEXT(" (someone is driving - knock them out)") : TEXT(" (seat empty: steal it!)")));
}

void ALFBrawler::UpdatePrompt()
{
	// Placeholder on-screen prompts (dev builds) until there is UI.
	if (!GEngine)
	{
		return;
	}
	const uint64 Key = 0x4C460000ull + GetUniqueID();
	if (State == ELFBrawlerState::Swimming)
	{
		if (const ALFBoat* B = FindGrabbableHull())
		{
			GEngine->AddOnScreenDebugMessage(Key, 0.f, FColor::White, FString::Printf(TEXT("[E / Y]  climb aboard %s"), *B->GetName()));
		}
	}
	else if (State == ELFBrawlerState::Standing && Boat.IsValid() && !Boat->HasDriver()
		&& FVector::Dist2D(GetActorLocation(), Boat->GetDriverSeatWorld()) <= DriveReach)
	{
		GEngine->AddOnScreenDebugMessage(Key, 0.f, FColor::White, Boat.Get() == HomeBoat.Get() ? FString(TEXT("[E / Y]  drive")) : FString(TEXT("[E / Y]  STEAL this boat")));
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

	// Keep the lens above the water (camera Z ~= pivot + socket - arm * sin(pitch)): clamp the control pitch.
	if (Controller)
	{
		const float Water = LF::Water::GetHeightAt(CameraPivot, GetWorld()->GetTimeSeconds()) + 12.5f;
		const float Room = float(CameraPivot.Z + CameraArm->SocketOffset.Z) - Water - CameraWaterClearance;
		const float MaxPitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Room / FMath::Max(CameraArm->TargetArmLength, 1.f), -1.f, 1.f)));
		FRotator C = Controller->GetControlRotation();
		const float Pitch = FRotator::NormalizeAxis(C.Pitch);
		if (Pitch > MaxPitch)
		{
			C.Pitch = MaxPitch;
			Controller->SetControlRotation(C);
		}
	}

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
