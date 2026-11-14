#include "Fish/LFFishCargoComponent.h"

#include "Fish/LFFishSubsystem.h"
#include "Brawler/LFBrawler.h"
#include "Boat/LFBoat.h"
#include "Water/LFWater.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ULFFishCargoComponent::ULFFishCargoComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Fish(TEXT("/Game/LakeFight/Meshes/SM_LF_Fish_A.SM_LF_Fish_A"));
	FishMesh = Fish.Object;
}

void ULFFishCargoComponent::AddFish(int32 Amount)
{
	Count += FMath::Clamp(Amount, 0, MAX_int32 - Count);
	UpdatePile();
}

int32 ULFFishCargoComponent::RemoveFish(int32 Amount)
{
	const int32 Removed = FMath::Clamp(Amount, 0, Count);
	Count -= Removed;
	UpdatePile();
	return Removed;
}

void ULFFishCargoComponent::Spill(int32 Amount)
{
	ALFBoat* Boat = Cast<ALFBoat>(GetOwner());
	ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
	if (Boat && Fish)
	{
		const int32 Spilled = RemoveFish(Amount);
		Fish->Spill(Boat, Spilled);
	}
}

void ULFFishCargoComponent::UpdatePile()
{
	const int32 Visible = FMath::Min(Count, FMath::Max(0, VisibleFishLimit));
	while (Pile.Num() < Visible)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
		Mesh->SetStaticMesh(FishMesh);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetupAttachment(GetOwner()->GetRootComponent());
		GetOwner()->AddInstanceComponent(Mesh);
		Mesh->RegisterComponent();
		const int32 I = Pile.Num();
		Mesh->SetRelativeLocation(FVector(85.f + (I % 2) * 22.f, -25.f + (I % 3) * 25.f, 12.f + (I / 3) * 10.f));
		Mesh->SetRelativeRotation(FRotator(0.f, I * 47.f, 90.f));
		Mesh->SetRelativeScale3D(FVector(0.6f));
		Pile.Add(Mesh);
	}
	for (int32 I = 0; I < Pile.Num(); ++I)
	{
		Pile[I]->SetVisibility(I < Visible);
	}
}
