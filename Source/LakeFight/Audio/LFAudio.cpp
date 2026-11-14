#include "Audio/LFAudio.h"

#include "Boat/LFBoat.h"
#include "Brawler/LFBrawler.h"
#include "Water/LFWater.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWave.h"

static TAutoConsoleVariable<int32> CVarAudioLog(TEXT("lf.audio.log"), 1, TEXT("1 = log every sound event fired ([audio])."));

namespace
{
	const TCHAR* AudioRoot = TEXT("/Game/LakeFight/Audio/");
	const TCHAR* CategoryName(ELFSoundCategory C)
	{
		switch (C)
		{
		case ELFSoundCategory::SFX: return TEXT("SFX");
		case ELFSoundCategory::Voice: return TEXT("Voice");
		case ELFSoundCategory::UI: return TEXT("UI");
		case ELFSoundCategory::Ambience: return TEXT("Ambience");
		case ELFSoundCategory::Music: return TEXT("Music");
		default: return TEXT("Master");
		}
	}

	// Ambience bed levels (before category volume).
	constexpr float AmbLakeVol = 0.8f;
	constexpr float AmbShoreVol = 0.9f;
	constexpr float AmbBirdsVol = 0.3f;
	constexpr float AmbMeadowVol = 0.25f;
	constexpr float ShoreNear = 300.f;   // full shore loop within 3 m of a bank
	constexpr float ShoreFar = 1500.f;   // silent beyond 15 m
	constexpr float RollCreakDegPerSec = 70.f;
	constexpr double RollCreakCooldown = 0.8;
	constexpr float BladeTipX = 140.f;   // blade centre along the oar mesh (same as the [oars] debug)
}

const ULFAudioSubsystem::FEventDef& ULFAudioSubsystem::Def(ELFSound E)
{
	// Prefix, category, volume, pitch +-, max instances
	static const FEventDef Defs[int32(ELFSound::Count)] = {
		{ TEXT("SFX_LF_PaddleWood"),    ELFSoundCategory::SFX,   0.9f,  0.06f, 4 },
		{ TEXT("SFX_LF_PaddleHit"),     ELFSoundCategory::SFX,   0.8f,  0.06f, 4 },
		{ TEXT("SFX_LF_PaddleCharged"), ELFSoundCategory::SFX,   1.0f,  0.06f, 4 },
		{ TEXT("SFX_LF_Stun"),          ELFSoundCategory::SFX,   0.7f,  0.06f, 3 },
		{ TEXT("SFX_LF_Overboard"),     ELFSoundCategory::SFX,   1.0f,  0.06f, 4 },
		{ TEXT("SFX_LF_Surface"),       ELFSoundCategory::SFX,   0.7f,  0.06f, 4 },
		{ TEXT("SFX_LF_BoatCreak"),     ELFSoundCategory::SFX,   0.7f,  0.06f, 4 },
		{ TEXT("SFX_LF_RamWood"),       ELFSoundCategory::SFX,   1.0f,  0.06f, 4 },
		{ TEXT("SFX_LF_RamBass"),       ELFSoundCategory::SFX,   0.8f,  0.06f, 3 },
		{ TEXT("SFX_LF_HitCreak"),      ELFSoundCategory::SFX,   0.8f,  0.06f, 3 },
		{ TEXT("SFX_LF_Surface"),       ELFSoundCategory::SFX,   0.22f, 0.12f, 4 }, // OarDip: soft, wider pitch spread
		{ TEXT("VO_LF_Attack"),         ELFSoundCategory::Voice, 0.9f,  0.03f, 3 },
		{ TEXT("VO_LF_Yell"),           ELFSoundCategory::Voice, 0.9f,  0.03f, 4 },
		{ TEXT("VO_LF_Pant"),           ELFSoundCategory::Voice, 0.55f, 0.03f, 4 },
		{ TEXT("VO_LF_Taunt"),          ELFSoundCategory::Voice, 1.0f,  0.03f, 2 },
		{ TEXT("VO_LF_Whistle"),        ELFSoundCategory::Voice, 0.8f,  0.03f, 2 },
	};
	return Defs[FMath::Clamp(int32(E), 0, int32(ELFSound::Count) - 1)];
}

ULFAudioSubsystem* ULFAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULFAudioSubsystem>() : nullptr;
}

bool ULFAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && !IsRunningDedicatedServer();
}

TStatId ULFAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULFAudioSubsystem, STATGROUP_Tickables);
}

void ULFAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 3D attenuation for SFX + Voice: full volume inside 2 m, linear falloff to silence at 40 m.
	Attenuation3D = NewObject<USoundAttenuation>(this, TEXT("LF_Attenuation3D"));
	FSoundAttenuationSettings& A = Attenuation3D->Attenuation;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(200.f, 0.f, 0.f);
	A.FalloffDistance = 3800.f;
	A.DistanceAlgorithm = EAttenuationDistanceModel::Linear;

	int32 Loaded = 0;
	Concurrency.SetNum(int32(ELFSound::Count));
	for (int32 i = 0; i < int32(ELFSound::Count); ++i)
	{
		const FEventDef& D = Def(ELFSound(i));
		FEventState& S = Events[i];
		for (int32 n = 1; n <= 6; ++n)
		{
			const FString Name = FString::Printf(TEXT("%s_%02d"), D.Prefix, n);
			const FString Path = FString::Printf(TEXT("%s%s.%s"), AudioRoot, *Name, *Name);
			if (USoundWave* W = LoadObject<USoundWave>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				S.Waves.Add(W);
				LoadedWaves.AddUnique(W);
			}
		}
		Loaded += S.Waves.Num();
		if (S.Waves.Num() == 0)
		{
			UE_LOG(LogLF, Warning, TEXT("[audio] no waves found for %s_NN under %s"), D.Prefix, AudioRoot);
		}
		// Pile-up guard: at most MaxInstances of this event at once; a new one stops the oldest. Long tails (> 1.5 s, e.g. RamBass)
		// keep playing and the new one is skipped instead, capped at 2 (stacked bass tails were the loudest build-up in a pile-up).
		float LongestTail = 0.f;
		for (const USoundWave* W : S.Waves)
		{
			LongestTail = FMath::Max(LongestTail, W->Duration);
		}
		const bool bLongTail = LongestTail > 1.5f;
		USoundConcurrency* C = NewObject<USoundConcurrency>(this, *FString::Printf(TEXT("LF_Conc_%d"), i));
		C->Concurrency.MaxCount = bLongTail ? FMath::Min(D.MaxInstances, 2) : D.MaxInstances;
		C->Concurrency.bLimitToOwner = false;
		C->Concurrency.ResolutionRule = bLongTail ? EMaxConcurrentResolutionRule::PreventNew : EMaxConcurrentResolutionRule::StopOldest;
		Concurrency[i] = C;
	}
	UE_LOG(LogLF, Display, TEXT("[audio] ready: %d events, %d wave variations loaded"), int32(ELFSound::Count), Loaded);

	StartAmbience();
	bStarted = true;

	if (ULFMusicSubsystem* M = ULFMusicSubsystem::Get(this))
	{
		M->OnGameWorldStarted(&InWorld);
	}
}

void ULFAudioSubsystem::Deinitialize()
{
	for (UAudioComponent* C : { AmbLake.Get(), AmbShore.Get(), AmbBirds.Get(), AmbMeadow.Get() })
	{
		if (C)
		{
			C->Stop();
			C->DestroyComponent();
		}
	}
	bStarted = false;
	Super::Deinitialize();
}

float ULFAudioSubsystem::CategoryGain(ELFSoundCategory C) const
{
	const ULFMusicSubsystem* M = ULFMusicSubsystem::Get(this);
	return M ? M->CategoryGain(C) : 1.f;
}

void ULFAudioSubsystem::SetCategoryVolume(ELFSoundCategory Category, float Volume)
{
	if (ULFMusicSubsystem* M = ULFMusicSubsystem::Get(this))
	{
		M->SetCategoryVolume(Category, Volume);
	}
	UpdateAmbience(0.f);
}

float ULFAudioSubsystem::GetCategoryVolume(ELFSoundCategory Category) const
{
	const ULFMusicSubsystem* M = ULFMusicSubsystem::Get(this);
	return M ? M->GetCategoryVolume(Category) : 1.f;
}

bool ULFAudioSubsystem::Play(ELFSound Event, const FVector& Location, float VolumeScale, float PitchScale, const TCHAR* Why)
{
	UWorld* World = GetWorld();
	const int32 Idx = int32(Event);
	if (!bStarted || !World || Idx < 0 || Idx >= int32(ELFSound::Count))
	{
		return false;
	}
	FEventState& S = Events[Idx];
	if (S.Waves.Num() == 0)
	{
		return false;
	}
	// Random variation, never the same one twice in a row.
	int32 Pick = 0;
	if (S.Waves.Num() > 1)
	{
		Pick = FMath::RandRange(0, S.Waves.Num() - 2);
		if (Pick >= S.LastIndex && S.LastIndex != INDEX_NONE)
		{
			++Pick;
		}
	}
	S.LastIndex = Pick;

	const FEventDef& D = Def(Event);
	// Burst ducking: beyond 3 one-shots (oar dips excluded) inside 0.25 s, each extra one plays 25 % quieter (pile-up headroom).
	const double Now = World->GetRealTimeSeconds();
	float Duck = 1.f;
	if (Event != ELFSound::OarDip)
	{
		RecentPlays.RemoveAll([Now](double T) { return Now - T > 0.25; });
		Duck = FMath::Pow(0.75f, float(FMath::Max(RecentPlays.Num() - 2, 0)));
		RecentPlays.Add(Now);
	}
	const float Volume = D.Volume * VolumeScale * Duck * CategoryGain(D.Category);
	const float Pitch = PitchScale * (1.f + FMath::FRandRange(-D.PitchVar, D.PitchVar));
	USoundWave* Wave = S.Waves[Pick];
	const bool b3D = D.Category == ELFSoundCategory::SFX || D.Category == ELFSoundCategory::Voice;
	if (b3D)
	{
		UGameplayStatics::SpawnSoundAtLocation(World, Wave, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, Attenuation3D, Concurrency[Idx]);
	}
	else
	{
		UGameplayStatics::SpawnSound2D(World, Wave, Volume, Pitch, 0.f, Concurrency[Idx]);
	}
	if (CVarAudioLog.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogLF, Display, TEXT("[audio] %-14s %s vol=%.2f pitch=%.2f at (%.0f, %.0f, %.0f)%s%s"),
			*StaticEnum<ELFSound>()->GetNameStringByValue(Idx), *Wave->GetName(), Volume, Pitch, Location.X, Location.Y, Location.Z,
			Why ? TEXT(" <- ") : TEXT(""), Why ? Why : TEXT(""));
	}
	return true;
}

// ---------------------------------------------------------------- Ambience

UAudioComponent* ULFAudioSubsystem::StartLoop(const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("%s%s.%s"), AudioRoot, Name, Name);
	USoundWave* W = LoadObject<USoundWave>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!W)
	{
		UE_LOG(LogLF, Warning, TEXT("[audio] ambience %s missing"), Name);
		return nullptr;
	}
	LoadedWaves.AddUnique(W);
	W->bLooping = true;
	UAudioComponent* C = UGameplayStatics::CreateSound2D(GetWorld(), W, 1.f, 1.f, 0.f, nullptr, false, false);
	if (C)
	{
		C->SetVolumeMultiplier(0.f);
		C->Play(FMath::FRandRange(0.f, FMath::Max(W->Duration - 1.f, 0.f))); // de-sync the loops
	}
	return C;
}

void ULFAudioSubsystem::StartAmbience()
{
	AmbLake = StartLoop(TEXT("AMB_LF_Lake_Loop"));
	AmbShore = StartLoop(TEXT("AMB_LF_Shore_Loop"));
	AmbBirds = StartLoop(TEXT("AMB_LF_Birds_Loop"));
	AmbMeadow = StartLoop(TEXT("AMB_LF_Meadow_Loop"));
	ShoreProbeTimer = 0.f;
	UpdateAmbience(0.f);
	UE_LOG(LogLF, Display, TEXT("[audio] ambience started: Lake + Shore (crossfade < %.0f m from a bank) + Birds + Meadow"), ShoreFar / 100.f);
}

float ULFAudioSubsystem::ProbeShoreDistance(const FVector& At) const
{
	// Land = static ground above the water within ShoreFar: rings of downward traces around the listener.
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LFShoreProbe), false);
	static const float Radii[] = { 0.f, 300.f, 600.f, 1000.f, 1500.f };
	for (float R : Radii)
	{
		const int32 Dirs = R <= 0.f ? 1 : 12;
		for (int32 i = 0; i < Dirs; ++i)
		{
			const float Ang = 2.f * PI * float(i) / float(Dirs);
			const FVector P(At.X + R * FMath::Cos(Ang), At.Y + R * FMath::Sin(Ang), 0.f);
			const float Water = LF::Water::GetHeightAt(P, Now);
			FHitResult Hit;
			if (World->LineTraceSingleByObjectType(Hit, FVector(P.X, P.Y, Water + 3000.f), FVector(P.X, P.Y, Water - 100.f), Obj, Params)
				&& Hit.ImpactPoint.Z > Water + 30.f)
			{
				return R;
			}
		}
	}
	return BIG_NUMBER;
}

void ULFAudioSubsystem::UpdateAmbience(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (DeltaTime > 0.f)
	{
		ShoreProbeTimer -= DeltaTime;
		if (ShoreProbeTimer <= 0.f)
		{
			ShoreProbeTimer = 0.5f;
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				FVector Eye;
				FRotator Rot;
				PC->GetPlayerViewPoint(Eye, Rot);
				const float Dist = ProbeShoreDistance(Eye);
				const float NewTarget = 1.f - FMath::Clamp((Dist - ShoreNear) / (ShoreFar - ShoreNear), 0.f, 1.f);
				if (FMath::Abs(NewTarget - ShoreTarget) > 0.25f && CVarAudioLog.GetValueOnGameThread() != 0)
				{
					UE_LOG(LogLF, Display, TEXT("[audio] Ambience      shore weight -> %.2f (nearest bank %s)"), NewTarget,
						Dist >= BIG_NUMBER ? TEXT("> 15 m") : *FString::Printf(TEXT("~%.0f m"), Dist / 100.f));
				}
				ShoreTarget = NewTarget;
			}
		}
		ShoreWeight = FMath::FInterpConstantTo(ShoreWeight, ShoreTarget, DeltaTime, 0.5f); // ~2 s crossfade
	}
	const float G = CategoryGain(ELFSoundCategory::Ambience);
	if (AmbLake) { AmbLake->SetVolumeMultiplier(G * AmbLakeVol * (1.f - 0.4f * ShoreWeight)); }
	if (AmbShore) { AmbShore->SetVolumeMultiplier(FMath::Max(G * AmbShoreVol * ShoreWeight, 0.001f)); }
	if (AmbBirds) { AmbBirds->SetVolumeMultiplier(G * AmbBirdsVol); }
	if (AmbMeadow) { AmbMeadow->SetVolumeMultiplier(G * AmbMeadowVol); }
}

// ---------------------------------------------------------------- Boats: roll-rate creaks, oar dips

void ULFAudioSubsystem::UpdateBoats()
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	const float WaterT = float(Now);
	for (TActorIterator<ALFBoat> It(World); It; ++It)
	{
		ALFBoat* B = *It;
		UStaticMeshComponent* Hull = B->GetHull();
		if (!Hull || !Hull->IsSimulatingPhysics())
		{
			continue;
		}
		FBoatAudio& S = BoatState.FindOrAdd(B);
		const FTransform& H = Hull->GetComponentTransform();

		// Hull rolling fast = creak (quieter just above the threshold).
		const float RollRate = FMath::Abs(float(FVector::DotProduct(Hull->GetPhysicsAngularVelocityInDegrees(), H.GetUnitAxis(EAxis::X))));
		if (RollRate > RollCreakDegPerSec && Now - S.LastCreak > RollCreakCooldown)
		{
			S.LastCreak = Now;
			Play(ELFSound::BoatCreak, H.GetLocation(), FMath::GetMappedRangeValueClamped(FVector2f(RollCreakDegPerSec, 200.f), FVector2f(0.5f, 1.f), RollRate),
				1.f, *FString::Printf(TEXT("%s roll %.0f deg/s"), *B->GetName(), RollRate));
		}

		// Oar blades entering the water while rowing.
		const bool bRowing = !B->IsCapsized() && FMath::Abs(B->GetThrottleOutput()) > 0.05f;
		const UStaticMeshComponent* Oars[2] = { B->OarL.Get(), B->OarR.Get() };
		for (int32 i = 0; i < 2; ++i)
		{
			if (!Oars[i])
			{
				continue;
			}
			const FVector Blade = Oars[i]->GetComponentTransform().TransformPosition(FVector(BladeTipX, 0.f, 0.f));
			const float Depth = LF::Water::GetHeightAt(Blade, WaterT) - float(Blade.Z);
			if (!S.bBladeIn[i] && Depth > 2.f)
			{
				S.bBladeIn[i] = true;
				if (bRowing)
				{
					Play(ELFSound::OarDip, Blade, 1.f, 1.15f, *FString::Printf(TEXT("%s oar %s"), *B->GetName(), i == 0 ? TEXT("L") : TEXT("R")));
				}
			}
			else if (S.bBladeIn[i] && Depth < -3.f)
			{
				S.bBladeIn[i] = false;
			}
		}
	}
}

void ULFAudioSubsystem::Tick(float DeltaTime)
{
	if (!bStarted)
	{
		return;
	}
	UpdateAmbience(DeltaTime);
	UpdateBoats();
}

// ---------------------------------------------------------------- Music (A02.1)

namespace
{
	constexpr float DuckDepthDb = -4.f;
	constexpr double DuckAttack = 0.05;
	constexpr double DuckHold = 0.4;
	constexpr double DuckRelease = 0.6;
	constexpr float FallbackBarSeconds = 2.f;

	const TCHAR* MusicStateName(ELFMusicState S)
	{
		switch (S)
		{
		case ELFMusicState::Menu: return TEXT("Menu");
		case ELFMusicState::Main: return TEXT("Main");
		case ELFMusicState::Final: return TEXT("Final");
		default: return TEXT("Off");
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GLFMusicCmd(
	TEXT("lf.music"), TEXT("lf.music <menu|main|final|off> - switch the music state (crossfades over one bar)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULFMusicSubsystem* M = ULFMusicSubsystem::Get(World);
		if (!M || Args.Num() == 0)
		{
			UE_LOG(LogLF, Display, TEXT("[music] usage: lf.music <menu|main|final|off>%s"), M ? TEXT("") : TEXT(" (no game instance)"));
			return;
		}
		const FString A = Args[0].ToLower();
		const ELFMusicState S = A == TEXT("menu") ? ELFMusicState::Menu : A == TEXT("main") ? ELFMusicState::Main
			: A == TEXT("final") ? ELFMusicState::Final : ELFMusicState::Off;
		M->SetMusicState(S);
	}));

const ULFMusicSubsystem::FTrack& ULFMusicSubsystem::TrackDef(ELFMusicState S)
{
	// BPM from Audio/AUDIO_REPORT.md (A03); 0 = unknown -> FallbackBarSeconds.
	static const FTrack Tracks[4] = {
		{ nullptr, 0.f },
		{ TEXT("MUS_LF_Menu"), 106.893f },
		{ TEXT("MUS_LF_Main_Loop"), 106.893f },
		{ TEXT("MUS_LF_Final_Loop"), 110.f },
	};
	return Tracks[FMath::Clamp(int32(S), 0, 3)];
}

ULFMusicSubsystem* ULFMusicSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULFMusicSubsystem>() : nullptr;
}

TStatId ULFMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULFMusicSubsystem, STATGROUP_Tickables);
}

float ULFMusicSubsystem::CategoryGain(ELFSoundCategory C) const
{
	return CategoryVolume[int32(ELFSoundCategory::Master)] * (C == ELFSoundCategory::Master ? 1.f : CategoryVolume[int32(C)]);
}

void ULFMusicSubsystem::SetCategoryVolume(ELFSoundCategory Category, float Volume)
{
	CategoryVolume[int32(Category)] = FMath::Max(Volume, 0.f);
	UE_LOG(LogLF, Display, TEXT("[audio] %s volume = %.2f"), CategoryName(Category), CategoryVolume[int32(Category)]);
	ApplyVolume();
}

UAudioComponent* ULFMusicSubsystem::GetPlayer(ELFMusicState S)
{
	if (S == ELFMusicState::Off)
	{
		return nullptr;
	}
	Players.SetNum(4);
	if (UAudioComponent* Existing = Players[int32(S)])
	{
		return Existing;
	}
	UWorld* World = MusicWorld.Get();
	if (!World)
	{
		return nullptr;
	}
	const TCHAR* Name = TrackDef(S).Asset;
	const FString Path = FString::Printf(TEXT("%s%s.%s"), AudioRoot, Name, Name);
	USoundWave* W = LoadObject<USoundWave>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!W)
	{
		UE_LOG(LogLF, Warning, TEXT("[music] %s missing"), Name);
		return nullptr;
	}
	W->bLooping = true; // pre-looped WAV: the whole file is the loop
	// 2D, survives level transitions, never auto-destroyed: one instance per track for the whole session.
	UAudioComponent* C = UGameplayStatics::CreateSound2D(World, W, 1.f, 1.f, 0.f, nullptr, /*bPersistAcrossLevelTransition*/ true, /*bAutoDestroy*/ false);
	if (C)
	{
		C->bIsUISound = true; // keeps playing while the game is paused
		C->OnAudioPlaybackPercent.AddDynamic(this, &ULFMusicSubsystem::OnPlaybackPercent);
		Players[int32(S)] = C;
	}
	return C;
}

void ULFMusicSubsystem::OnGameWorldStarted(UWorld* World)
{
	if (!MusicWorld.IsValid())
	{
		MusicWorld = World;
	}
	if (!bEverStarted)
	{
		bEverStarted = true;
		SetMusicState(ELFMusicState::Main); // PIE default until T04 / the menu drive it
	}
}

void ULFMusicSubsystem::SetMusicState(ELFMusicState NewState)
{
	if (NewState == State)
	{
		return;
	}
	const ELFMusicState Old = State;
	const float Bpm = TrackDef(NewState == ELFMusicState::Off ? Old : NewState).Bpm;
	const float Fade = Bpm > 0.f ? 240.f / Bpm : FallbackBarSeconds; // one 4/4 bar
	if (UAudioComponent* Out = Old != ELFMusicState::Off ? GetPlayer(Old) : nullptr)
	{
		Out->FadeOut(Fade, 0.f, EAudioFaderCurve::Sin); // equal-power pair with the Sin fade-in
	}
	if (UAudioComponent* In = GetPlayer(NewState))
	{
		LastPercent[int32(NewState)] = 0.f;
		Wraps[int32(NewState)] = 0;
		In->FadeIn(Fade, 1.f, 0.f, EAudioFaderCurve::Sin); // from the loop start
	}
	State = NewState;
	StateStart = FPlatformTime::Seconds();
	ApplyVolume();
	UE_LOG(LogLF, Display, TEXT("[music] %s -> %s (equal-power crossfade %.2f s = 1 bar at %.1f BPM)"),
		MusicStateName(Old), MusicStateName(NewState), Fade, Bpm);
}

void ULFMusicSubsystem::OnPlaybackPercent(const USoundWave* PlayingWave, const float PlaybackPercent)
{
	for (int32 i = 1; i < 4; ++i)
	{
		const UAudioComponent* C = Players.IsValidIndex(i) ? Players[i].Get() : nullptr;
		if (!C || C->Sound.Get() != PlayingWave)
		{
			continue;
		}
		static double LastDiag = 0.0;
		const double NowDiag = FPlatformTime::Seconds();
		if (NowDiag - LastDiag > 2.0)
		{
			LastDiag = NowDiag;
			UE_LOG(LogLF, Verbose, TEXT("[music] diag %s percent=%.4f (last %.4f), %.2f s after the state started"),
				*PlayingWave->GetName(), PlaybackPercent, LastPercent[i], NowDiag - StateStart);
		}
		// A wrap = the percent crosses a whole cycle: for a looping wave UE keeps counting past 1.0 (measured in PIE: 1.0072 at
		// 18.13 s on the 17.962 s Main loop); the jump-back test covers a restart from 0.
		const bool bWrapped = PlaybackPercent + 0.5f < LastPercent[i] || FMath::FloorToInt(PlaybackPercent) > FMath::FloorToInt(LastPercent[i]);
		if (bWrapped && ELFMusicState(i) == State && NowDiag - StateStart > 1.0)
		{
			++Wraps[i];
			UE_LOG(LogLF, Display, TEXT("[music] %s looped (wrap %d, %.2f s after the state started, wave %.3f s)"),
				MusicStateName(ELFMusicState(i)), Wraps[i], FPlatformTime::Seconds() - StateStart, PlayingWave->Duration);
		}
		LastPercent[i] = PlaybackPercent;
	}
}

void ULFMusicSubsystem::Duck(const TCHAR* Why)
{
	const double Now = FPlatformTime::Seconds();
	const bool bAlreadyDucked = Now < DuckHoldUntil + DuckRelease && DuckDb < -0.01f;
	if (!bAlreadyDucked)
	{
		DuckStart = Now;
	}
	// Overlapping ducks extend the hold, never go deeper.
	DuckHoldUntil = FMath::Max(DuckHoldUntil, (bAlreadyDucked ? Now : DuckStart + DuckAttack) + DuckHold);
	UE_LOG(LogLF, Display, TEXT("[music] duck %.0f dB (%s)%s, hold until +%.2f s"), DuckDepthDb, Why,
		bAlreadyDucked ? TEXT(" - extends the current duck") : TEXT(""), DuckHoldUntil - Now);
}

void ULFMusicSubsystem::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	float Target = 0.f;
	if (Now < DuckHoldUntil)
	{
		Target = DuckDepthDb;
	}
	const float PrevDb = DuckDb;
	const double Dt = LastTickTime > 0.0 ? FMath::Min(Now - LastTickTime, 0.1) : 0.0;
	LastTickTime = Now;
	if (Target < DuckDb)
	{
		DuckDb = FMath::Max(Target, DuckDb + float(DuckDepthDb * Dt / DuckAttack)); // attack: 0 -> -4 dB in 0.05 s
	}
	else if (Target > DuckDb)
	{
		DuckDb = FMath::Min(Target, DuckDb - float(DuckDepthDb * Dt / DuckRelease)); // release: -4 -> 0 dB in 0.6 s
		if (DuckDb > -0.01f && PrevDb <= -0.01f)
		{
			DuckDb = 0.f;
			UE_LOG(LogLF, Display, TEXT("[music] duck released (%.2f s after it started)"), Now - DuckStart);
		}
	}
	if (DuckDb != PrevDb)
	{
		ApplyVolume();
	}
}

void ULFMusicSubsystem::ApplyVolume()
{
	const float Gain = CategoryGain(ELFSoundCategory::Music) * FMath::Pow(10.f, DuckDb / 20.f);
	for (UAudioComponent* C : Players)
	{
		if (C)
		{
			C->SetVolumeMultiplier(FMath::Max(Gain, 0.001f));
		}
	}
}

void ULFMusicSubsystem::Deinitialize()
{
	for (UAudioComponent* C : Players)
	{
		if (C)
		{
			C->Stop();
			C->DestroyComponent();
		}
	}
	Players.Reset();
	Super::Deinitialize();
}
