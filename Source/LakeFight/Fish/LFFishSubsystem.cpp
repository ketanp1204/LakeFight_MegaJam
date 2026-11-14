#include "Fish/LFFishSubsystem.h"

#include "Fish/LFFishCargoComponent.h"
#include "Fish/LFFishRuntime.h" // T04 hook
#include "Fish/LFFishSpotVisualComponent.h" // T04 hook
#include "Round/LFRoundCargoComponent.h" // T04 hook
#include "Round/LFRoundSubsystem.h" // T04 hook
#include "Boat/LFBoat.h"
#include "Brawler/LFBrawler.h"
#include "Audio/LFAudio.h"
#include "Water/LFWater.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

static FAutoConsoleCommandWithWorldAndArgs CmdLFFishGive(TEXT("lf.fish.give"), TEXT("lf.fish.give N - add fish to the player's current boat (1-100)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC || Args.Num() != 1)
		{
			return;
		}
		ALFBoat* Boat = Cast<ALFBoat>(PC->GetPawn());
		if (ALFBrawler* B = Cast<ALFBrawler>(PC->GetPawn()))
		{
			Boat = B->GetBoat();
		}
		if (Boat)
		{
			if (ULFFishCargoComponent* Cargo = ULFFishCargoComponent::Get(Boat))
			{
				const int32 N = FMath::Clamp(FCString::Atoi(*Args[0]), 0, 100);
				Cargo->AddFish(N);
				UE_LOG(LogLF, Display, TEXT("[fish] debug give %d to %s (now %d)"), N, *Boat->GetName(), Cargo->GetCount());
			}
		}
	}));

namespace
{
	const TCHAR* TeamNames[] = { TEXT("Red"), TEXT("Blue"), TEXT("Green"), TEXT("Yellow") };
	// T04 hook: these array indices are the persisted crew enum values.
	static_assert(int32(ELFTeam::Red) == 0 && int32(ELFTeam::Blue) == 1 && int32(ELFTeam::Green) == 2 && int32(ELFTeam::Yellow) == 3);
	static_assert(UE_ARRAY_COUNT(TeamNames) == int32(ELFTeam::Yellow) + 1);

	bool Named(const AActor* Actor, const FString& Prefix)
	{
		if (Actor->GetName().StartsWith(Prefix))
		{
			return true;
		}
		for (const FName& Tag : Actor->Tags)
		{
			if (Tag.ToString().StartsWith(Prefix))
			{
				return true;
			}
		}
#if WITH_EDITOR
		return Actor->GetActorLabel().StartsWith(Prefix);
#else
		return false;
#endif
	}
}

ULFFishSubsystem::ULFFishSubsystem()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Spot(TEXT("/Game/LakeFight/Meshes/SM_LF_FishSpot.SM_LF_FishSpot"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Fish(TEXT("/Game/LakeFight/Meshes/SM_LF_Fish_A.SM_LF_Fish_A"));
	static ConstructorHelpers::FObjectFinder<USoundWave> Catch(TEXT("/Game/LakeFight/Audio/UI_LF_FishCaught.UI_LF_FishCaught"));
	static ConstructorHelpers::FObjectFinder<USoundWave> Bank(TEXT("/Game/LakeFight/Audio/UI_LF_FishBanked.UI_LF_FishBanked"));
	SpotMesh = Spot.Object;
	FishMesh = Fish.Object;
	CatchSound = Catch.Object;
	BankSound = Bank.Object;
}

bool ULFFishSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId ULFFishSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULFFishSubsystem, STATGROUP_Tickables);
}

void ULFFishSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (Named(*It, TEXT("FishZone_")) && (MaxSpots <= 0 || Spots.Num() < MaxSpots))
		{
			FSpot S;
			S.Centre = It->GetActorLocation();
			Spots.Add(S);
		}
		for (int32 Team = 0; Team < 4; ++Team)
		{
			if (Named(*It, FString::Printf(TEXT("BankZone_%s"), TeamNames[Team])))
			{
				FBank B;
				B.Zone = *It;
				B.Team = Team;
				Banks.Add(B);
			}
		}
	}
	for (FSpot& S : Spots)
	{
		Respawn(S);
	}
	UE_LOG(LogLF, Display, TEXT("[fish] ready: %d zones, %d banks; catch %.1f s below %.0f cm/s, radius %.0f cm"), Spots.Num(), Banks.Num(), CatchInterval, CatchMaxSpeed, CatchRadius);
	if (Spots.IsEmpty() || Banks.Num() != UE_ARRAY_COUNT(TeamNames))
	{
		UE_LOG(LogLF, Warning, TEXT("[fish] expected FishZone_* markers and BankZone_Red/Blue/Green/Yellow; add runtime Actor Tags matching these names (editor labels work only in PIE)"));
	}
	NextReport = InWorld.GetTimeSeconds() + 10.f;
	bStarted = true;
}

AStaticMeshActor* ULFFishSubsystem::MakeMesh(UStaticMesh* Mesh, const FVector& At, float Scale)
{
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(At, FRotator::ZeroRotator);
	if (Actor)
	{
		Actor->SetMobility(EComponentMobility::Movable);
		Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		Actor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Actor->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
		Actor->SetActorScale3D(FVector(Scale));
	}
	return Actor;
}

bool ULFFishSubsystem::IsOpenWater(const FVector& At) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LFFishWater), false);
	return !GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 150.f),
		FVector(At.X, At.Y, LF::Water::BaseHeight + 20.f), ECC_WorldStatic, Params);
}

FVector ULFFishSubsystem::PickPoint(const FVector& Centre) const
{
	for (int32 I = 0; I < 24; ++I)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::Sqrt(FMath::FRand()) * ZoneRadius;
		const FVector At = Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
		if (IsOpenWater(At))
		{
			return At;
		}
	}
	return Centre;
}

void ULFFishSubsystem::Respawn(FSpot& Spot)
{
	const FVector At = PickPoint(Spot.Centre);
	Spot.Mesh = MakeMesh(SpotMesh, At, 2.f);
	// T04 hook: failed spawns retry on the same cooldown, never every frame.
	Spot.RespawnAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(5.f, 8.f);
	if (!Spot.Mesh.IsValid()) return;
	ULFFishSpotVisualComponent::Add(Spot.Mesh.Get(), false);
	Spot.Goal = PickPoint(Spot.Centre);
	Spot.Remaining = FMath::RandRange(3, 5);
	Spot.Progress = 0.f;
	Spot.Catcher.Reset();
	UE_LOG(LogLF, Display, TEXT("[fish] spot spawned at %s with %d fish"), *At.ToCompactString(), Spot.Remaining);
}

void ULFFishSubsystem::PlaySound(USoundWave* Sound)
{
	float Gain = 1.f;
	if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this))
	{
		Gain = Music->CategoryGain(ELFSoundCategory::UI);
	}
	UGameplayStatics::PlaySound2D(this, Sound, Gain);
}

int32 ULFFishSubsystem::Spill(ALFBoat* Boat, int32 Amount)
{
	const float Now = GetWorld()->GetTimeSeconds();
	int32 Spawned = 0;
	for (int32 I = 0; I < Amount; ++I)
	{
		FVector At;
		// T04 hook: outside the complete hull, in open water; keep fish aboard on failure.
		if (!LF::Fish::SpillPoint(Boat, I, Amount, At)) continue;
		FPickup P;
		P.Mesh = MakeMesh(FishMesh, At, 0.8f);
		if (!P.Mesh.IsValid()) continue;
		P.Born = Now;
		P.SourceBoat = Boat;
		Pickups.Add(P);
		++Spawned;
	}
	if (Spawned < Amount) ULFFishCargoComponent::Get(Boat)->AddFish(Amount - Spawned);
	Spilled += Spawned;
	if (Spawned > 0) Boat->OnSplash(Boat->GetActorLocation(), 0.3f);
	return Spawned;
}

void ULFFishSubsystem::Tick(float DeltaTime)
{
	// T04 hook: the round owns the scoring cutoff.
	const ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
	if (Round && Round->HasEnded()) return;
	if (!bStarted)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const TArray<FLFFishBoat>& Boats = GetWorld()->GetSubsystem<ULFFishBoatCache>()->GetBoats(); // T04 hook
	for (FSpot& S : Spots)
	{
		if (!S.Mesh.IsValid())
		{
			if (Now >= S.RespawnAt)
			{
				Respawn(S);
			}
			continue;
		}
		FVector At = S.Mesh->GetActorLocation();
		if (FVector::Dist2D(At, S.Goal) < 30.f)
		{
			S.Goal = PickPoint(S.Centre);
		}
		FVector Next = At + (S.Goal - At).GetSafeNormal2D() * FMath::Min(DriftSpeed * DeltaTime, float(FVector::Dist2D(At, S.Goal)));
		if (IsOpenWater(Next))
		{
			At = Next;
		}
		else
		{
			S.Goal = PickPoint(S.Centre);
		}
		At.Z = LF::Water::GetHeightAt(At, Now) + 18.f;
		S.Mesh->SetActorLocation(At);
		S.Mesh->SetActorScale3D(FVector(2.f + 0.08f * FMath::Sin(Now * 3.f)));
		ALFBoat* Catcher = LF::Fish::NearestCatcher(GetWorld(), At, CatchRadius, CatchMaxSpeed); // T04 hook
		if (!Catcher || S.Catcher.Get() != Catcher)
		{
			S.Progress = 0.f;
			S.Catcher = Catcher;
		}
		if (Catcher)
		{
			S.Progress += DeltaTime;
			if (S.Progress >= CatchInterval)
			{
				if (ULFFishCargoComponent* Cargo = ULFFishCargoComponent::Get(Catcher))
				{
					Cargo->AddFish(1);
					++Caught;
					--S.Remaining;
					S.Progress = 0.f;
					Catcher->OnSplash(At, 0.25f);
					PlaySound(CatchSound);
					UE_LOG(LogLF, Display, TEXT("[fish] catch %s cargo=%d spot=%d t=%.2f"), *Catcher->GetName(), Cargo->GetCount(), S.Remaining, Now);
					if (S.Remaining == 0)
					{
						S.Mesh->Destroy();
						S.Mesh.Reset();
						S.RespawnAt = Now + FMath::FRandRange(5.f, 8.f);
					}
				}
			}
		}
	}
	for (int32 I = Pickups.Num() - 1; I >= 0; --I)
	{
		FPickup& P = Pickups[I];
		bool bRemove = !P.Mesh.IsValid() || Now - P.Born >= PickupLifetime;
		if (!bRemove)
		{
			FVector At = P.Mesh->GetActorLocation();
			At.Z = LF::Water::GetHeightAt(At, Now) + 18.f;
			P.Mesh->SetActorLocation(At);
			for (const FLFFishBoat& Entry : Boats)
			{
				ALFBoat* Boat = Entry.Boat.Get();
				if (!Boat || !Entry.Cargo.IsValid()) continue;
				FVector2D Nearest;
				if ((Boat != P.SourceBoat.Get() || Now - P.Born >= 1.5f) && !Boat->IsCapsized() && Boat->HasDriver() && Boat->GunwaleDistance(At, Nearest) < 25.f)
				{
					if (ULFFishCargoComponent* Cargo = Entry.Cargo.Get())
					{
						Cargo->AddFish(1);
						++Scooped;
						bRemove = true;
						UE_LOG(LogLF, Display, TEXT("[fish] pickup %s cargo=%d"), *Boat->GetName(), Cargo->GetCount());
						break;
					}
				}
			}
		}
		if (bRemove)
		{
			if (P.Mesh.IsValid()) P.Mesh->Destroy();
			Pickups.RemoveAt(I);
		}
	}
	for (const FLFFishBoat& Entry : Boats)
	{
		ALFBoat* Boat = Entry.Boat.Get();
		ULFFishCargoComponent* Cargo = Entry.Cargo.Get();
		if (!Boat || !Cargo) continue;
		// T04 hook: the round HUD replaces temporary boat text.
		if (LF::Fish::DebugEnabled()) DrawDebugString(GetWorld(), Boat->GetActorLocation() + FVector(0.f, 0.f, 180.f), FString::Printf(TEXT("Fish: %d"), Cargo->GetCount()), nullptr, FColor::White, 0.f, true);
		ALFBrawler* Driver = Boat->GetDriver();
		if (!Driver || Boat->IsCapsized() || Cargo->GetCount() == 0) continue;
		for (const FBank& B : Banks)
		{
			if (!B.Zone.IsValid() || int32(Driver->Team) != B.Team) continue;
			// T04 hook: mesh-local bounds include authored size, offset, rotation and component scale.
			const UStaticMeshComponent* BankMesh = B.Zone->FindComponentByClass<UStaticMeshComponent>();
			if (!BankMesh || !BankMesh->GetStaticMesh()) continue;
			const FBox Bounds = BankMesh->GetStaticMesh()->GetBoundingBox();
			const FVector Local = BankMesh->GetComponentTransform().InverseTransformPosition(Boat->GetActorLocation());
			if (Local.X < Bounds.Min.X || Local.X > Bounds.Max.X || Local.Y < Bounds.Min.Y || Local.Y > Bounds.Max.Y
				|| FMath::Abs(Local.Z - Bounds.GetCenter().Z) > 150.f) continue;
			// T04 hook
			const int32 Points = ULFRoundCargoComponent::Get(Boat)->GetValue(Cargo->GetCount());
			const int32 N = Cargo->RemoveFish(Cargo->GetCount());
			Scores[B.Team] += Points;
			if (ULFRoundSubsystem* R = GetWorld()->GetSubsystem<ULFRoundSubsystem>()) R->RecordBank(B.Team, Points);
			Banked += N;
			PlaySound(BankSound);
			for (int32 I = 0; I < FMath::Min(N, 8); ++I)
			{
				FFlight F;
				F.Start = Boat->GetActorLocation() + FVector(0.f, I * 8.f, 40.f);
				F.End = B.Zone->GetActorLocation() + FVector(0.f, 0.f, 60.f);
				F.Born = Now;
				F.Mesh = MakeMesh(FishMesh, F.Start, 0.6f);
				Flights.Add(F);
			}
			if (LF::Fish::DebugEnabled() && GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, FString::Printf(TEXT("%s +%d points!"), TeamNames[B.Team], Points));
			UE_LOG(LogLF, Display, TEXT("[fish] bank %s team=%s fish=%d points=%d score=%d"), *Boat->GetName(), TeamNames[B.Team], N, Points, Scores[B.Team]);
			break; // T04 hook: one deposit even when same-team banks overlap.
		}
	}
	for (int32 I = Flights.Num() - 1; I >= 0; --I)
	{
		FFlight& F = Flights[I];
		const float T = (Now - F.Born) / 0.8f;
		if (!F.Mesh.IsValid() || T >= 1.f)
		{
			if (F.Mesh.IsValid()) F.Mesh->Destroy();
			Flights.RemoveAt(I);
			continue;
		}
		F.Mesh->SetActorLocation(FMath::Lerp(F.Start, F.End, T) + FVector(0.f, 0.f, 240.f * FMath::Sin(T * PI)));
	}
	// T04 hook: the round HUD replaces temporary scores.
	if (LF::Fish::DebugEnabled() && GEngine) GEngine->AddOnScreenDebugMessage(40401, 0.f, FColor::White,
		FString::Printf(TEXT("FISH  Red %d | Blue %d | Green %d | Yellow %d"), Scores[0], Scores[1], Scores[2], Scores[3]));
	if (Now >= NextReport)
	{
		UE_LOG(LogLF, Display, TEXT("[fish] t=%.1f caught=%d spilled=%d scooped=%d banked=%d scores=%d/%d/%d/%d pickups=%d"),
			Now, Caught, Spilled, Scooped, Banked, Scores[0], Scores[1], Scores[2], Scores[3], Pickups.Num());
		NextReport = Now + 10.f;
	}
}
