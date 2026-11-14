#include "Round/LFRoundCargoComponent.h"

#include "Round/LFGoldFish.h"
#include "Fish/LFFishCargoComponent.h"
#include "Fish/LFFishRuntime.h" // T04 hook
#include "Brawler/LFBrawler.h"
#include "Boat/LFBoat.h"
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
	if (ULFFishCargoComponent* Cargo = ULFFishCargoComponent::Get(GetOwner()))
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
	ALFBoat* Boat = Cast<ALFBoat>(GetOwner());
	int32 Spawned = 0;
	for (int32 I = 0; I < RemovedGold; ++I)
	{
		// T04 hook: gold uses the same hull clearance/water check and source immunity.
		FVector At;
		if (!Boat || !LF::Fish::SpillPoint(Boat, I, RemovedGold, At)) continue;
		if (ALFGoldFish* Fish = GetWorld()->SpawnActor<ALFGoldFish>(At, FRotator::ZeroRotator))
		{
			Fish->MakePickup(Boat);
			++Spawned;
		}
	}
	const int32 Failed = RemovedGold - Spawned;
	RemovedGold = 0;
	for (int32 I = 0; I < Failed; ++I) AddGold();
	return Spawned;
}
