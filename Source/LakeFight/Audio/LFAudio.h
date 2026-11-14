#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
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

	float CategoryVolume[6] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
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
