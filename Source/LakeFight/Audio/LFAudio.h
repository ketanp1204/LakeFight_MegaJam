#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "LFAudio.generated.h"

class USoundWave;
class USoundAttenuation;
class USoundConcurrency;
class UAudioComponent;
class ALFBoat;

/** Mix categories (A02). Master scales everything; each sound plays at Master x its category. */
UENUM(BlueprintType)
enum class ELFSoundCategory : uint8
{
	Master, SFX, Voice, UI, Ambience, Music
};

/** One gameplay sound event = a set of variations (_01.._06) with its own randomisation / concurrency rules. */
UENUM(BlueprintType)
enum class ELFSound : uint8
{
	PaddleWood, PaddleHit, PaddleCharged, Stun, Overboard, Surface, BoatCreak, RamWood, RamBass, HitCreak, OarDip,
	VO_Attack, VO_Yell, VO_Pant, VO_Taunt, VO_Whistle,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ELFMusicState : uint8
{
	Off, Menu, Main, Final
};

/**
 * A02.1 music player + category volumes, one per game session (game instance), so music never restarts on respawn, overboard,
 * capsize or a level change. One 2D audio component per track (persistent across level transitions); state changes crossfade
 * equal-power over one bar of the incoming track (BPM from AUDIO_REPORT.md), each track starting from its loop start (0 s - the
 * WAVs are pre-looped). Ducking: Duck() = -4 dB, attack 0.05 s, hold 0.4 s, release 0.6 s; overlapping ducks extend the hold.
 * Debug: `lf.music <menu|main|final|off>`; [music] log for state changes and loop wraps.
 */
UCLASS()
class LAKEFIGHT_API ULFMusicSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static ULFMusicSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category="LF|Audio")
	void SetMusicState(ELFMusicState NewState);

	UFUNCTION(BlueprintPure, Category="LF|Audio")
	ELFMusicState GetMusicState() const { return State; }

	/** Called when a game world begins play: the first one starts Main (PIE default until T04 drives it). */
	void OnGameWorldStarted(UWorld* World);

	/** Ram-tier impact / capsize: duck the music. */
	void Duck(const TCHAR* Why);

	/** Category volumes live here so they survive level changes (ULFAudioSubsystem forwards to these). */
	void SetCategoryVolume(ELFSoundCategory Category, float Volume);
	float GetCategoryVolume(ELFSoundCategory Category) const { return CategoryVolume[int32(Category)]; }
	float CategoryGain(ELFSoundCategory C) const;

	// USubsystem / FTickableGameObject
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always; }
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FTrack
	{
		const TCHAR* Asset;
		float Bpm;
	};
	static const FTrack& TrackDef(ELFMusicState S);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Players; // indexed by ELFMusicState (Off unused)

	UAudioComponent* GetPlayer(ELFMusicState S);

	UFUNCTION()
	void OnPlaybackPercent(const USoundWave* PlayingWave, const float PlaybackPercent);

	void ApplyVolume();

	ELFMusicState State = ELFMusicState::Off;
	bool bEverStarted = false;
	TWeakObjectPtr<UWorld> MusicWorld;
	float LastPercent[4] = { 0.f, 0.f, 0.f, 0.f };
	int32 Wraps[4] = { 0, 0, 0, 0 };
	double StateStart = 0.0;

	float CategoryVolume[6] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };

	// Ducking (real time, so hitstop doesn't stretch it)
	double DuckStart = -100.0;    // start of the current attack
	double DuckHoldUntil = -100.0;
	float DuckDb = 0.f;           // current attenuation (0 .. -4)
	double LastTickTime = 0.0;
};

/**
 * A02 audio: plays the imported WAVs (/Game/LakeFight/Audio) for gameplay events, runs the ambience bed, and keeps per-category
 * volumes for the options menu. Per event: random variation (never the same one twice in a row), random pitch (SFX +-6 %,
 * voices +-3 %), a concurrency limit (MaxInstances, oldest stopped), 3D attenuation for SFX + Voice (2-40 m), 2D for UI/Ambience.
 * Also polls boats for hull roll-rate creaks and oar blade entries. `lf.audio.log 1` (default) logs every sound fired.
 */
UCLASS()
class LAKEFIGHT_API ULFAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULFAudioSubsystem* Get(const UObject* WorldContext);

	/** Play one event at a location. VolumeScale/PitchScale multiply the event's own settings. Returns false if nothing played. */
	bool Play(ELFSound Event, const FVector& Location, float VolumeScale = 1.f, float PitchScale = 1.f, const TCHAR* Why = nullptr);

	UFUNCTION(BlueprintCallable, Category="LF|Audio")
	void PlaySound(ELFSound Event, FVector Location, float VolumeScale = 1.f) { Play(Event, Location, VolumeScale, 1.f, TEXT("BP")); }

	/** Options menu: 0..1 (or more) per category. Live ambience loops follow immediately. */
	UFUNCTION(BlueprintCallable, Category="LF|Audio")
	void SetCategoryVolume(ELFSoundCategory Category, float Volume);

	UFUNCTION(BlueprintPure, Category="LF|Audio")
	float GetCategoryVolume(ELFSoundCategory Category) const;

	// UTickableWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	struct FEventDef
	{
		const TCHAR* Prefix;    // asset name without _NN
		ELFSoundCategory Category;
		float Volume;
		float PitchVar;         // +- fraction
		int32 MaxInstances;
	};
	static const FEventDef& Def(ELFSound E);

	struct FEventState
	{
		TArray<TObjectPtr<USoundWave>> Waves;
		int32 LastIndex = INDEX_NONE;
	};
	FEventState Events[int32(ELFSound::Count)];

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundWave>> LoadedWaves; // GC roots

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundConcurrency>> Concurrency; // one per event

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation3D;

	TArray<double> RecentPlays; // real time of recent one-shots (burst ducking)
	float CategoryGain(ELFSoundCategory C) const;

	// Ambience bed
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbLake;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbShore;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbBirds;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbMeadow;
	float ShoreWeight = 0.f;       // smoothed 0..1
	float ShoreTarget = 0.f;
	float ShoreProbeTimer = 0.f;
	void StartAmbience();
	void UpdateAmbience(float DeltaTime);
	float ProbeShoreDistance(const FVector& At) const;
	UAudioComponent* StartLoop(const TCHAR* Name);

	// Boat polling (roll-rate creaks, oar dips)
	struct FBoatAudio
	{
		double LastCreak = -100.0;
		bool bBladeIn[2] = { false, false };
	};
	TMap<TWeakObjectPtr<ALFBoat>, FBoatAudio> BoatState;
	void UpdateBoats();

	bool bStarted = false;
};
