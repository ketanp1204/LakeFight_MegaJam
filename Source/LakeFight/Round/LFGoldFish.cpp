#include "Round/LFGoldFish.h"

#include "Round/LFRoundCargoComponent.h"
#include "Fish/LFFishRuntime.h" // T04 hook
#include "Fish/LFFishSpotVisualComponent.h" // T04 hook
#include "Round/LFRoundSubsystem.h"
#include "Brawler/LFBrawler.h"
#include "Boat/LFBoat.h"
#include "Water/LFWater.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ALFGoldFish::ALFGoldFish()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Spot(TEXT("/Game/LakeFight/Meshes/SM_LF_FishSpot.SM_LF_FishSpot"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Gold(TEXT("/Game/LakeFight/Meshes/SM_LF_Fish_Gold.SM_LF_Fish_Gold"));
	GoldMesh = Gold.Object;
	Mesh->SetStaticMesh(Spot.Object);
	Mesh->SetRelativeScale3D(FVector(2.f));
	for (int32 I = 0; I < 6; ++I)
	{
		UStaticMeshComponent* Sparkle = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Sparkle%d"), I));
		Sparkle->SetupAttachment(Mesh);
		Sparkle->SetStaticMesh(GoldMesh);
		Sparkle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sparkle->SetCanEverAffectNavigation(false);
		Sparkles.Add(Sparkle);
	}
}

void ALFGoldFish::BeginPlay()
{
	Super::BeginPlay();
	Centre = GetActorLocation();
	Born = GetWorld()->GetTimeSeconds();
	Remaining = FMath::RandRange(1, 3);
	UE_LOG(LogLF, Display, TEXT("[round] gold spot: %d fish, value 5 each"), Remaining);
}

void ALFGoldFish::MakePickup(ALFBoat* SourceBoat)
{
	bPickup = true;
	SpillSource = SourceBoat; // T04 hook
	Mesh->SetStaticMesh(GoldMesh);
	Mesh->SetRelativeScale3D(FVector(0.8f));
	for (UStaticMeshComponent* Sparkle : Sparkles) Sparkle->SetVisibility(false);
	SetLifeSpan(10.f);
}

void ALFGoldFish::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
	if (!Round || Round->HasEnded()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	FVector At = Centre;
	if (!bPickup) At += FVector(FMath::Sin((Now - Born) * 0.5f), FMath::Cos((Now - Born) * 0.5f), 0.f) * 100.f;
	At.Z = LF::Water::GetHeightAt(At, Now) + 20.f;
	SetActorLocation(At);
	if (!bPickup)
	{
		// T04 hook: defer until the first tick so pickups never create spot visuals.
		if (!bVisualStarted) { ULFFishSpotVisualComponent::Add(this, true); bVisualStarted = true; }
		for (int32 I = 0; I < Sparkles.Num(); ++I)
		{
			const float A = Now * 1.5f + I * PI / 3.f;
			Sparkles[I]->SetRelativeLocation(FVector(FMath::Cos(A) * 95.f, FMath::Sin(A) * 95.f, 25.f + 12.f * FMath::Sin(A * 2.f)));
			Sparkles[I]->SetRelativeRotation(FRotator(0.f, Now * 90.f + I * 60.f, 0.f));
			Sparkles[I]->SetRelativeScale3D(FVector(0.12f + 0.08f * FMath::Abs(FMath::Sin(A * 3.f))));
		}
	}
	// T04 hook: use the shared cache and identical spot arbitration; pickups allow every team.
	ALFBoat* Eligible = bPickup ? nullptr : LF::Fish::NearestCatcher(GetWorld(), At, CatchRadius, CatchMaxSpeed);
	if (bPickup)
	{
		for (const FLFFishBoat& Entry : GetWorld()->GetSubsystem<ULFFishBoatCache>()->GetBoats())
		{
			ALFBoat* Boat = Entry.Boat.Get();
			if (!Boat || !Entry.Cargo.IsValid() || Boat->IsCapsized() || !Boat->HasDriver()) continue;
			FVector2D Nearest;
			if ((Boat != SpillSource.Get() || Now - Born >= 1.5f) && Boat->GunwaleDistance(At, Nearest) < 25.f)
			{
				Eligible = Boat;
				break;
			}
		}
	}
	if (!Eligible || Catcher.Get() != Eligible) Progress = 0.f;
	Catcher = Eligible;
	if (!Eligible) return;
	Progress += DeltaSeconds;
	if (bPickup || Progress >= CatchInterval)
	{
		ULFRoundCargoComponent::Get(Eligible)->AddGold();
		Eligible->OnSplash(At, 0.4f);
		Round->GoldCaught();
		UE_LOG(LogLF, Display, TEXT("[round] gold %s by %s, value=5"), bPickup ? TEXT("pickup") : TEXT("catch"), *Eligible->GetName());
		Progress = 0.f;
		if (bPickup || --Remaining <= 0) Destroy();
	}
}
