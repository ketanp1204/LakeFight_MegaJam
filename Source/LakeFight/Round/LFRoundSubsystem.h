#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LFRoundSubsystem.generated.h"

class ALFBrawler;
class ALFGoldFish;
class ULFRoundWidget;
class ULFMenuWidget;
class USoundWave;
class UAudioComponent;

/** N2 item 15: one results-card row (icon = ICON_LF_<Icon>.png; Team -1 = no team). */
struct FLFStatRow
{
	FString Label;
	FString Value;
	FString Icon;
	int32 Team = -1;
};

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
	/** Seconds of play since the round started (pauses and the menu excluded). */
	float GetRoundTime() const { return RoundTime; }
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
	/** -1 on a draw. */
	int32 GetWinnerTeam() const;
	TArray<FLFStatRow> GetStatRows() const;
	void Restart();
	void Quit();
	/** N1 item 5: the main menu shows over the paused lake on the first load only; Play starts the round. */
	bool IsInMenu() const { return bInMenu; }
	void StartFromMenu();
	/** N2 item 12: pause menu (Esc / P / gamepad Start during a running round). The world pauses; the widget stays live. */
	bool CanPause() const;
	bool IsPauseMenuOpen() const { return PauseMenu != nullptr; }
	void OpenPauseMenu();
	void ResumeFromPause();
	/** Reloads the map with the main menu shown again (also in PIE). */
	void QuitToMenu();

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
	TObjectPtr<ULFMenuWidget> Menu;
	bool bInMenu = false;
	float MenuTime = 0.f;
	static bool bMenuDone; // process lifetime: Restart (map reload) goes straight back into a round
	static bool bForceMenu; // set by QuitToMenu for the next load
	UPROPERTY()
	TObjectPtr<ULFMenuWidget> PauseMenu;
	TSharedPtr<class IInputProcessor> PauseKeys;
	bool bSmokePauseDone = false;
	// N2 item 14: -LFBench [-LFQuality=0|1|2] stages a 4-boat pile-up at the centre and logs frame times (works in Shipping).
	bool bBench = false;
	int32 BenchStaged = 0;
	bool bBenchDone = false;
	bool bBenchShot = false;
	TArray<float> BenchFrame, BenchGame, BenchRender, BenchGPU;
	void StagePileup();
	void FinishBench();
	float RoundTime = 0.f;
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

namespace LF
{
	/** N1 smoke evidence: with -LFSmoke on the command line (works in Shipping, which has no log), append a timestamped line
	 *  to <ProjectSavedDir>/LFSmoke.txt. No-op otherwise. */
	LAKEFIGHT_API void Smoke(const FString& Line);
	LAKEFIGHT_API bool SmokeEnabled();
}
