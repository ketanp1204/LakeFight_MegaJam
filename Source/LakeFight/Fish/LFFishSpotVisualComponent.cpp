#include "Fish/LFFishSpotVisualComponent.h"

#include "Audio/LFAudio.h"
#include "Water/LFWater.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ULFFishSpotVisualComponent::ULFFishSpotVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Ring(TEXT("/Game/LakeFight/Meshes/SM_LF_Crate.SM_LF_Crate"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Fish(TEXT("/Game/LakeFight/Meshes/SM_LF_Fish_A.SM_LF_Fish_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Gold(TEXT("/Game/LakeFight/Meshes/SM_LF_Fish_Gold.SM_LF_Fish_Gold"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FX(TEXT("/Game/LakeFight/FX/NS_LF_Splash.NS_LF_Splash"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cream(TEXT("/Game/LakeFight/Meshes/M_LF_PaintCream.M_LF_PaintCream"));
	RippleMaterial = Cream.Object;
	RingMesh = Ring.Object; FishMesh = Fish.Object; GoldMesh = Gold.Object; Splash = FX.Object;
}

void ULFFishSpotVisualComponent::Add(AActor* Spot, bool bGoldSpot)
{
	if (!Spot) return;
	ULFFishSpotVisualComponent* Visual = NewObject<ULFFishSpotVisualComponent>(Spot);
	Visual->bGold = bGoldSpot;
	Spot->AddInstanceComponent(Visual);
	Visual->RegisterComponent();
}

UStaticMeshComponent* ULFFishSpotVisualComponent::MakeMesh(UStaticMesh* Asset)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
	Mesh->SetStaticMesh(Asset);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	Mesh->SetupAttachment(GetOwner()->GetRootComponent());
	Mesh->SetAbsolute(true, true, true);
	GetOwner()->AddInstanceComponent(Mesh);
	Mesh->RegisterComponent();
	return Mesh;
}

void ULFFishSpotVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	for (int32 I = 0; I < 2; ++I)
	{
		UInstancedStaticMeshComponent* Ring = NewObject<UInstancedStaticMeshComponent>(GetOwner());
		Ring->SetStaticMesh(RingMesh);
		Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ring->SetCanEverAffectNavigation(false);
		Ring->SetCastShadow(false);
		Ring->SetupAttachment(GetOwner()->GetRootComponent());
		Ring->SetAbsolute(true, true, true);
		GetOwner()->AddInstanceComponent(Ring);
		Ring->RegisterComponent();
		for (int32 Slot = 0; Slot < Ring->GetNumMaterials(); ++Slot) Ring->SetMaterial(Slot, RippleMaterial);
		// Flatten our original crate into thin foam segments; two instanced draws per spot.
		if (RingMesh)
		{
			const FBox Bounds = RingMesh->GetBoundingBox();
			const FVector Scale = FVector(21.f, 2.f, 0.3f) / Bounds.GetSize().ComponentMax(FVector(1.f));
			for (int32 Segment = 0; Segment < 32; ++Segment)
			{
				const float Angle = Segment * 2.f * PI / 32.f;
				const FRotator Rotation(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f);
				const FVector At = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 100.f;
				Ring->AddInstance(FTransform(Rotation, At - Rotation.RotateVector(Bounds.GetCenter() * Scale), Scale));
			}
		}
		Rings.Add(Ring);
	}
	Leaper = MakeMesh(bGold ? GoldMesh : FishMesh);
	Leaper->SetVisibility(false);
	const FVector2D Interval = bGold ? GoldLeapInterval : LeapInterval;
	NextLeap = GetWorld()->GetTimeSeconds() + FMath::FRandRange(Interval.X, Interval.Y);
}

void ULFFishSpotVisualComponent::Land()
{
	const FVector At = Leaper->GetComponentLocation();
	if (Splash)
	{
		if (UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Splash, At)) FX->SetVariableFloat(TEXT("Strength"), 0.15f);
	}
	if (ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(this)) Audio->Play(ELFSound::Surface, At, 0.35f, 1.15f, TEXT("fish leap"));
	Leaper->SetVisibility(false);
	bLeaping = false;
}

void ULFFishSpotVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float Now = GetWorld()->GetTimeSeconds();
	FVector At = GetOwner()->GetActorLocation();
	const float Water = LF::Water::GetHeightAt(At, Now);
	// Reuse the original disconnected-piece ring: it bobs/tilts and slowly rotates as one shoal.
	GetOwner()->SetActorLocation(FVector(At.X, At.Y, Water + 12.f + BobHeight * FMath::Sin(Now * 2.f)));
	GetOwner()->SetActorRotation(FRotator(2.f * FMath::Sin(Now), Now * 7.f, 2.f * FMath::Cos(Now * 0.8f)));
	for (int32 I = 0; I < Rings.Num(); ++I)
	{
		const float T = FMath::Fmod(Now * 0.45f + I * 0.5f, 1.f);
		Rings[I]->SetWorldLocation(FVector(At.X, At.Y, Water + 2.f));
		Rings[I]->SetWorldRotation(FRotator(0.f, Now * -5.f, 0.f));
		Rings[I]->SetWorldScale3D(FVector(0.3f + T * 2.3f, 0.3f + T * 2.3f, 1.f));
		Rings[I]->SetVisibility(T < 0.92f);
	}
	if (!bLeaping && Now >= NextLeap)
	{
		LeapStart = Now;
		LeapYaw = FMath::FRandRange(0.f, 2.f * PI);
		bLeaping = true;
		Leaper->SetVisibility(true);
		const FVector2D Interval = bGold ? GoldLeapInterval : LeapInterval;
		NextLeap = Now + FMath::FRandRange(Interval.X, Interval.Y);
	}
	if (bLeaping)
	{
		const float T = FMath::Clamp((Now - LeapStart) / 0.85f, 0.f, 1.f);
		const FVector Direction(FMath::Cos(LeapYaw), FMath::Sin(LeapYaw), 0.f);
		Leaper->SetWorldLocation(FVector(At.X, At.Y, Water + 5.f + LeapHeight * FMath::Sin(T * PI)) + Direction * FMath::Lerp(-65.f, 65.f, T));
		Leaper->SetWorldRotation(FRotator(65.f * FMath::Cos(T * PI), FMath::RadiansToDegrees(LeapYaw), 0.f));
		if (T >= 1.f) Land();
	}
}
