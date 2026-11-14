#include "Fish/LFFishRuntime.h"

#include "Fish/LFFishCargoComponent.h"
#include "Brawler/LFBrawler.h"
#include "Boat/LFBoat.h"
#include "Water/LFWater.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarLFFishDebug(TEXT("lf.fish.debug"), 0, TEXT("1 = fish debug text, scores and bank messages."));

bool ULFFishBoatCache::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void ULFFishBoatCache::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (TActorIterator<ALFBoat> It(&InWorld); It; ++It) AddBoat(*It);
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ULFFishBoatCache::AddBoat));
}

void ULFFishBoatCache::AddBoat(AActor* Actor)
{
	ALFBoat* Boat = Cast<ALFBoat>(Actor);
	if (!Boat) return;
	Boats.RemoveAll([](const FLFFishBoat& Entry) { return !Entry.Boat.IsValid(); });
	for (const FLFFishBoat& Entry : Boats) if (Entry.Boat == Boat) return;
	FLFFishBoat Entry;
	Entry.Boat = Boat;
	Entry.Cargo = ULFFishCargoComponent::Get(Boat);
	Boats.Add(Entry);
}

void ULFFishBoatCache::Deinitialize()
{
	GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
	Boats.Reset();
	Super::Deinitialize();
}

bool LF::Fish::DebugEnabled() { return CVarLFFishDebug.GetValueOnGameThread() != 0; }

ALFBoat* LF::Fish::PlayerBoat(UWorld* World)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) return nullptr;
	if (ALFBrawler* Brawler = Cast<ALFBrawler>(PC->GetPawn())) return Brawler->GetBoat();
	return Cast<ALFBoat>(PC->GetPawn());
}

bool LF::Fish::IsOpenWater(UWorld* World, const FVector& At)
{
	if (!World) return false;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LFFishSpillWater), false);
	const float Water = LF::Water::GetHeightAt(At, World->GetTimeSeconds());
	return !World->LineTraceSingleByChannel(Hit, FVector(At.X, At.Y, Water + 1000.f),
		FVector(At.X, At.Y, Water - 5.f), ECC_WorldStatic, Params);
}

bool LF::Fish::SpillPoint(ALFBoat* Boat, int32 Index, int32 Count, FVector& Out)
{
	// World XY bounding circle stays outside a rotated/scaled/capsized hull, with pickup clearance.
	const FVector Extent = Boat->GetHull()->Bounds.BoxExtent;
	const FVector Centre = Boat->GetHull()->Bounds.Origin;
	const float Radius = FVector2D(Extent.X, Extent.Y).Size() + 90.f;
	for (int32 Try = 0; Try < 32; ++Try)
	{
		const float Angle = 2.f * PI * Index / FMath::Max(Count, 1) + Try * 2.39996f;
		Out = Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (Radius + (Try / 8) * 100.f);
		Out.Z = LF::Water::GetHeightAt(Out, Boat->GetWorld()->GetTimeSeconds()) + 18.f;
		if (IsOpenWater(Boat->GetWorld(), Out)) return true;
	}
	return false;
}

ALFBoat* LF::Fish::NearestCatcher(UWorld* World, const FVector& At, float Radius, float MaxSpeed)
{
	ALFBoat* Best = nullptr;
	float BestDistance = Radius;
	float SecondDistance = TNumericLimits<float>::Max();
	for (const FLFFishBoat& Entry : World->GetSubsystem<ULFFishBoatCache>()->GetBoats())
	{
		ALFBoat* Boat = Entry.Boat.Get();
		if (!Boat || !Entry.Cargo.IsValid() || Boat->IsCapsized() || !Boat->HasDriver()
			|| Boat->GetHull()->GetPhysicsLinearVelocity().Size2D() >= MaxSpeed) continue;
		const float Distance = FVector::Dist2D(Boat->GetActorLocation(), At);
		if (Distance >= Radius) continue;
		if (Distance < BestDistance)
		{
			SecondDistance = Best ? BestDistance : TNumericLimits<float>::Max();
			Best = Boat;
			BestDistance = Distance;
		}
		else SecondDistance = FMath::Min(SecondDistance, Distance);
	}
	// Stage 2 remains player-only. A closer AI occupies the spot but gains no fishing behaviour.
	return SecondDistance - BestDistance > 1.f && Best && Best->IsPlayerControlled() ? Best : nullptr;
}

static FAutoConsoleCommandWithWorldAndArgs CmdLFFishGiveAll(TEXT("lf.fish.giveall"), TEXT("lf.fish.giveall N - add 0-100 fish to every boat."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() != 1) return;
		const int32 N = FMath::Clamp(FCString::Atoi(*Args[0]), 0, 100);
		for (const FLFFishBoat& Entry : World->GetSubsystem<ULFFishBoatCache>()->GetBoats())
		{
			if (Entry.Cargo.IsValid() && Entry.Boat.IsValid())
			{
				Entry.Cargo->AddFish(N);
				UE_LOG(LogLF, Display, TEXT("[fish] giveall %d to %s (now %d)"), N, *Entry.Boat->GetName(), Entry.Cargo->GetCount());
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdLFBoatCapsize(TEXT("lf.boat.capsize"), TEXT("lf.boat.capsize - roll the player's current boat over."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ALFBoat* Boat = LF::Fish::PlayerBoat(World)) Boat->DebugCapsizeKick();
	}));
