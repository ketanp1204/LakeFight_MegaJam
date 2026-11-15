#include "Fish/LFFishSubsystem.h"

#include "Fish/LFFishCargoComponent.h"
#include "Fish/LFFishRuntime.h" // T04 hook
#include "Fish/LFFishSpotVisualComponent.h" // T04 hook
#include "Round/LFRoundCargoComponent.h" // T04 hook
#include "Round/LFRoundSubsystem.h" // T04 hook
#include "Round/LFGoldFish.h"
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
	if (FParse::Param(FCommandLine::Get(), TEXT("LFLegacyFish"))) bFairSpots = false; // N3: pre-N3 marker zones, for A/B test rounds
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (Named(*It, TEXT("FishZone_")))
		{
			++ZoneMarkers;
			if (!bFairSpots && (MaxSpots <= 0 || Spots.Num() < MaxSpots))
			{
				FSpot S;
				S.Centre = It->GetActorLocation();
				Spots.Add(S);
			}
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
	if (bFairSpots)
	{
		if (Banks.Num() == UE_ARRAY_COUNT(TeamNames))
		{
			FSpot Ring; Ring.Slot = 0; Spots.Add(Ring);
			FSpot Lane; Lane.Slot = 1; Spots.Add(Lane);
		}
		else
		{
			UE_LOG(LogLF, Warning, TEXT("[fish] fair spots need 4 banks (found %d): no spots"), Banks.Num());
		}
	}
	for (FSpot& S : Spots)
	{
		Respawn(S);
	}
	UE_LOG(LogLF, Display, TEXT("[fish] ready: %d zones, %d banks, %d live spots (%s); catch %.1f s below %.0f cm/s, radius %.0f cm"), ZoneMarkers, Banks.Num(),
		Spots.Num(), bFairSpots ? TEXT("fair: centre ring + rotating lane") : TEXT("marker zones"), CatchInterval, CatchMaxSpeed, CatchRadius);
	LF::Smoke(FString::Printf(TEXT("map %s: fish ready - %d zones, %d banks, %d live spots"), *InWorld.GetMapName(), ZoneMarkers, Banks.Num(), Spots.Num()));
	if (ZoneMarkers == 0 || Spots.IsEmpty() || Banks.Num() != UE_ARRAY_COUNT(TeamNames))
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

bool ULFFishSubsystem::PlaceFairSpot(FSpot& Spot)
{
	// N3 19: the gold spot (and the lake) is centred on the world origin.
	const FVector LakeCentre(0.f, 0.f, LF::Water::BaseHeight);
	if (Spot.Slot == 0)
	{
		for (int32 I = 0; I < 24; ++I)
		{
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			const FVector C = LakeCentre + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(CentreRingMin, CentreRingMax);
			if (IsOpenWater(C))
			{
				Spot.Centre = C;
				Spot.LaneTeam = -1;
				++RingSpawns;
				return true;
			}
		}
		return false;
	}
	if (LaneBag.IsEmpty())
	{
		LaneBag = { 0, 1, 2, 3 };
		for (int32 I = LaneBag.Num() - 1; I > 0; --I) LaneBag.Swap(I, FMath::RandRange(0, I));
	}
	const int32 Team = LaneBag.Pop(EAllowShrinking::No);
	FVector Bank;
	if (!GetBankLocation(ELFTeam(Team), Bank)) return false;
	Bank.Z = LakeCentre.Z;
	const FVector In = (LakeCentre - Bank).GetSafeNormal2D();
	// Same distance from this team's bank every time; swing off the centre line (both ways) only if it lands on land.
	for (const float Deg : { 0.f, 6.f, -6.f, 12.f, -12.f, 18.f, -18.f })
	{
		const FVector C = Bank + In.RotateAngleAxis(Deg, FVector::UpVector) * LaneDistFromBank;
		if (IsOpenWater(C))
		{
			Spot.Centre = C;
			Spot.LaneTeam = Team;
			++LaneSpawns[Team];
			return true;
		}
	}
	UE_LOG(LogLF, Warning, TEXT("[fish] no open water for the %s lane spot"), TeamNames[Team]);
	return false;
}

void ULFFishSubsystem::Respawn(FSpot& Spot)
{
	if (Spot.Slot >= 0 && !PlaceFairSpot(Spot))
	{
		Spot.RespawnAt = GetWorld()->GetTimeSeconds() + 2.f;
		return;
	}
	const FVector At = PickPoint(Spot.Centre);
	Spot.Mesh = MakeMesh(SpotMesh, At, 2.f);
	// T04 hook: failed spawns retry on the same cooldown, never every frame.
	Spot.RespawnAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(5.f, 8.f);
	if (!Spot.Mesh.IsValid()) return;
	ULFFishSpotVisualComponent::Add(Spot.Mesh.Get(), false);
	Spot.Goal = PickPoint(Spot.Centre);
	Spot.Remaining = bFairSpots ? FMath::RandRange(4, 6) : FMath::RandRange(3, 5); // N3 19: only 2 live spots now
	Spot.LastActive = GetWorld()->GetTimeSeconds();
	Spot.Progress = 0.f;
	Spot.Catcher.Reset();
	FString Dist;
	for (const FBank& B : Banks)
	{
		if (B.Zone.IsValid()) Dist += FString::Printf(TEXT(" %s=%.0fm"), TeamNames[B.Team], FVector::Dist2D(At, B.Zone->GetActorLocation()) / 100.f);
	}
	UE_LOG(LogLF, Display, TEXT("[fish] spot spawned at %s with %d fish (%s; to banks:%s)"), *At.ToCompactString(), Spot.Remaining,
		Spot.Slot == 0 ? TEXT("centre ring") : Spot.Slot == 1 ? *FString::Printf(TEXT("%s lane"), TeamNames[Spot.LaneTeam]) : TEXT("marker zone"), *Dist);
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
		if (S.Slot >= 0 && !S.Catcher.IsValid() && Now - S.LastActive > IdleSpotLifetime)
		{
			UE_LOG(LogLF, Display, TEXT("[fish] %s spot unfished for %.0f s - moving on"), S.Slot == 0 ? TEXT("centre") : TeamNames[S.LaneTeam], IdleSpotLifetime);
			S.Mesh->Destroy();
			S.Mesh.Reset();
			S.RespawnAt = Now + 1.f;
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
					S.LastActive = Now;
					Catcher->OnSplash(At, 0.25f);
					if (Catcher == LF::Fish::PlayerBoat(GetWorld())) PlaySound(CatchSound); // N1 #2: AI catches are not UI sounds
					UE_LOG(LogLF, Display, TEXT("[fish] catch %s cargo=%d spot=%d t=%.2f"), *Catcher->GetName(), Cargo->GetCount(), S.Remaining, Now);
					LF::Smoke(FString::Printf(TEXT("catch %s (driver %s) cargo=%d"), *Catcher->GetName(), Catcher->GetDriver() ? *Catcher->GetDriver()->GetName() : TEXT("-"), Cargo->GetCount()));
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
		UE_LOG(LogLF, Display, TEXT("[fish] t=%.1f caught=%d spilled=%d scooped=%d banked=%d scores=%d/%d/%d/%d pickups=%d spawns ring=%d lanes R/B/G/Y=%d/%d/%d/%d"),
			Now, Caught, Spilled, Scooped, Banked, Scores[0], Scores[1], Scores[2], Scores[3], Pickups.Num(), RingSpawns, LaneSpawns[0], LaneSpawns[1], LaneSpawns[2], LaneSpawns[3]);
		LF::Smoke(FString::Printf(TEXT("t=%.0f caught=%d spilled=%d scooped=%d banked=%d scores=%d/%d/%d/%d"), Now, Caught, Spilled, Scooped, Banked, Scores[0], Scores[1], Scores[2], Scores[3]));
		NextReport = Now + 10.f;
	}
}

// ---- AI query API (N1 #14)
bool ULFFishSubsystem::FindFishSpot(const ALFBoat* Boat, FVector& OutLocation, bool bPreferGold) const
{
	const FVector From = Boat ? Boat->GetActorLocation() : FVector::ZeroVector;
	if (bPreferGold)
	{
		const ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
		const ALFGoldFish* Gold = Round ? Round->GetGoldSpot() : nullptr;
		if (Gold && Gold->IsSpot())
		{
			OutLocation = Gold->GetActorLocation();
			return true;
		}
	}
	float Best = TNumericLimits<float>::Max();
	for (const FSpot& S : Spots)
	{
		if (!S.Mesh.IsValid() || S.Remaining <= 0) continue;
		const ALFBoat* Holder = S.Catcher.Get();
		if (Holder && Holder != Boat) continue;
		const FVector SpotAt = S.Mesh->GetActorLocation();
		const float D = FVector::DistSquared2D(From, SpotAt);
		if (D < Best)
		{
			Best = D;
			OutLocation = SpotAt;
		}
	}
	return Best < TNumericLimits<float>::Max();
}

bool ULFFishSubsystem::GetBankLocation(ELFTeam Team, FVector& OutLocation) const
{
	for (const FBank& B : Banks)
	{
		if (B.Team == int32(Team) && B.Zone.IsValid())
		{
			OutLocation = B.Zone->GetActorLocation();
			return true;
		}
	}
	return false;
}

bool ULFFishSubsystem::IsFishing(const ALFBoat* Boat) const
{
	if (!Boat) return false;
	for (const FSpot& S : Spots)
	{
		if (S.Mesh.IsValid() && S.Catcher.Get() == Boat) return true;
	}
	const ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
	const ALFGoldFish* Gold = Round ? Round->GetGoldSpot() : nullptr;
	return Gold && Gold->GetCatcher() == Boat;
}

int32 ULFFishSubsystem::GetCargoCount(const ALFBoat* Boat)
{
	const ULFFishCargoComponent* Cargo = Boat ? ULFFishCargoComponent::Get(Boat) : nullptr;
	return Cargo ? Cargo->GetCount() : 0;
}

int32 ULFFishSubsystem::GetCargoValue(const ALFBoat* Boat)
{
	const int32 Count = GetCargoCount(Boat);
	const ULFRoundCargoComponent* Round = Boat ? ULFRoundCargoComponent::Get(const_cast<ALFBoat*>(Boat)) : nullptr;
	return Round ? Round->GetValue(Count) : Count;
}

bool ULFFishSubsystem::FindPickup(const FVector& From, float Radius, FVector& OutLocation) const
{
	float Best = Radius * Radius;
	bool bFound = false;
	for (const FPickup& P : Pickups)
	{
		if (!P.Mesh.IsValid()) continue;
		const float D = FVector::DistSquared2D(From, P.Mesh->GetActorLocation());
		if (D < Best)
		{
			Best = D;
			OutLocation = P.Mesh->GetActorLocation();
			bFound = true;
		}
	}
	return bFound;
}

int32 ULFFishSubsystem::GetActiveSpotCount() const
{
	int32 N = 0;
	for (const FSpot& S : Spots) N += S.Mesh.IsValid() ? 1 : 0;
	return N;
}
