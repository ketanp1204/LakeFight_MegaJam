#include "Round/LFRoundCargoComponent.h"

#include "Round/LFGoldFish.h"
#include "Fish/LFFishCargoComponent.h"
#include "Engine/World.h"

ULFRoundCargoComponent* ULFRoundCargoComponent::Get(AActor* Boat)
{
	if (!Boat) return nullptr;
	ULFRoundCargoComponent* Cargo = Boat->FindComponentByClass<ULFRoundCargoComponent>();
	if (!Cargo)
	{
		Cargo = NewObject<ULFRoundCargoComponent>(Boat);
		Boat->AddInstanceComponent(Cargo);
		Cargo->RegisterComponent();
	}
	return Cargo;
}

void ULFRoundCargoComponent::AddGold()
{
	if (ULFFishCargoComponent* Cargo = GetOwner()->FindComponentByClass<ULFFishCargoComponent>())
	{
		++Gold;
		Cargo->AddFish(1);
	}
}

void ULFRoundCargoComponent::Removing(int32 Amount, int32 CountBefore)
{
	// Ordinary fish spill first; gold retains its value until it is actually removed.
	RemovedGold = FMath::Clamp(Amount - (CountBefore - Gold), 0, Gold);
	Gold -= RemovedGold;
}

int32 ULFRoundCargoComponent::SpillGold()
{
	for (int32 I = 0; I < RemovedGold; ++I)
	{
		const float A = I * 2.4f;
		const FVector At = GetOwner()->GetActorLocation() + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 220.f;
		if (ALFGoldFish* Fish = GetWorld()->SpawnActor<ALFGoldFish>(At, FRotator::ZeroRotator))
		{
			Fish->MakePickup();
		}
	}
	const int32 N = RemovedGold;
	RemovedGold = 0;
	return N;
}
