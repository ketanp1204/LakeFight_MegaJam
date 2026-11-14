#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LFRoundSubsystem.generated.h"

class ALFBrawler;
class ALFGoldFish;
class ULFRoundWidget;
class USoundWave;
class UAudioComponent;

/** Single-player round authority. World lifetime = one round; restart reloads the map. */
UCLASS()
class LAKEFIGHT_API ULFRoundSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	ULFRoundSubsystem();
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual void Deinitialize() override;
	virtual TStatId GetStatId() const override;

	UPROPERTY(EditDefaultsOnly, Category="LF|Round")
	float RoundDuration = 240.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Round")
	float GoldRemaining = 120.f;
	UPROPERTY(EditDefaultsOnly, Category="LF|Round")
	float FinalRemaining = 30.f;

	float GetRemaining() const { return Remaining; }
	bool HasEnded() const { return bEnded; }
	bool GoldAlert() const { return GoldBanner > 0.f; }
	ALFGoldFish* GetGoldSpot() const { return GoldSpot.Get(); }
	void SetRemaining(float Seconds);
	void GoldCaught();
	void RecordOverboard(ALFBrawler* Brawler);
	void RecordSteal(ALFBrawler* Brawler);
	void RecordSpill(int32 Count);
	void RecordBank(int32 Team, int32 Value);
	FString GetWinner() const;
	FString GetStats() const;
	void Restart();
	void Quit();

private:
	float Remaining = 240.f;
	float GoldBanner = 0.f;
	bool bStarted = false;
	bool bGoldSpawned = false;
	bool bFinal = false;
	bool bEnded = false;
	int32 Overboards[4] = { 0, 0, 0, 0 };
	int32 Steals = 0;
	int32 Spills = 0;
	int32 BiggestBank = 0;
	int32 BiggestBankTeam = 0;
	float LongestSwim = 0.f;
	int32 LongestSwimTeam = 0;
	TMap<TWeakObjectPtr<ALFBrawler>, float> SwimTimes;
	TWeakObjectPtr<ALFGoldFish> GoldSpot;

	UPROPERTY(Transient)
	TObjectPtr<ULFRoundWidget> Widget;
	UPROPERTY()
	TObjectPtr<USoundWave> GoldSound;
	UPROPERTY()
	TObjectPtr<USoundWave> SparkleSound;
	UPROPERTY()
	TObjectPtr<USoundWave> FinalSound;
	UPROPERTY()
	TObjectPtr<USoundWave> EndSound;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FinalTick;

	void Advance();
	void EndRound();
	void PlayUI(USoundWave* Sound);
	float UIGain() const;
	bool bTest = false;
	float TestElapsed = 0.f;
	int32 TestStep = 0;
};
