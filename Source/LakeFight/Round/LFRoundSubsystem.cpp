#include "Round/LFRoundSubsystem.h"

#include "Round/LFGoldFish.h"
#include "Round/LFRoundWidget.h"
#include "Brawler/LFBrawler.h"
#include "Boat/LFBoat.h"
#include "Fish/LFFishSubsystem.h"
#include "Audio/LFAudio.h"
#include "Water/LFWater.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

static FAutoConsoleCommandWithWorldAndArgs CmdLFRoundTime(TEXT("lf.round.remaining"), TEXT("lf.round.remaining N - set remaining seconds for testing round milestones."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (World && Args.Num() == 1)
		{
			if (ULFRoundSubsystem* Round = World->GetSubsystem<ULFRoundSubsystem>()) Round->SetRemaining(FCString::Atof(*Args[0]));
		}
	}));

namespace
{
	const TCHAR* Teams[] = { TEXT("Red"), TEXT("Blue"), TEXT("Green"), TEXT("Yellow") };
}

ULFRoundSubsystem::ULFRoundSubsystem()
{
	static ConstructorHelpers::FObjectFinder<USoundWave> Gold(TEXT("/Game/LakeFight/Audio/UI_LF_GoldFish.UI_LF_GoldFish"));
	static ConstructorHelpers::FObjectFinder<USoundWave> Sparkle(TEXT("/Game/LakeFight/Audio/UI_LF_GoldFishSparkle.UI_LF_GoldFishSparkle"));
	static ConstructorHelpers::FObjectFinder<USoundWave> Final(TEXT("/Game/LakeFight/Audio/UI_LF_Final30_Loop.UI_LF_Final30_Loop"));
	static ConstructorHelpers::FObjectFinder<USoundWave> End(TEXT("/Game/LakeFight/Audio/UI_LF_EndCheer.UI_LF_EndCheer"));
	GoldSound = Gold.Object;
	SparkleSound = Sparkle.Object;
	FinalSound = Final.Object;
	EndSound = End.Object;
}

bool ULFRoundSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

TStatId ULFRoundSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULFRoundSubsystem, STATGROUP_Tickables);
}

void ULFRoundSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Remaining = RoundDuration;
	bStarted = true;
	if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this)) Music->SetMusicState(ELFMusicState::Main);
#if !UE_BUILD_SHIPPING
	bTest = FParse::Param(FCommandLine::Get(), TEXT("LFRoundTest"));
#endif
	UE_LOG(LogLF, Display, TEXT("[round] started %.0f s; gold at %.0f remaining, final at %.0f"), Remaining, GoldRemaining, FinalRemaining);
}

float ULFRoundSubsystem::UIGain() const
{
	const ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this);
	return Music ? Music->CategoryGain(ELFSoundCategory::UI) : 1.f;
}

void ULFRoundSubsystem::PlayUI(USoundWave* Sound)
{
	UGameplayStatics::PlaySound2D(this, Sound, UIGain());
}

void ULFRoundSubsystem::SetRemaining(float Seconds)
{
	if (!bEnded)
	{
		Remaining = FMath::Clamp(Seconds, 0.f, RoundDuration);
		UE_LOG(LogLF, Display, TEXT("[round] debug remaining %.2f"), Remaining);
		Advance();
	}
}

void ULFRoundSubsystem::Advance()
{
	if (Remaining <= 0.f)
	{
		EndRound();
		return;
	}
	if (!bGoldSpawned && Remaining <= GoldRemaining)
	{
		bGoldSpawned = true;
		// Centre is the clear water shared by the three existing fishing zones.
		GoldSpot = GetWorld()->SpawnActor<ALFGoldFish>(FVector(0.f, 0.f, LF::Water::BaseHeight), FRotator::ZeroRotator);
		GoldBanner = 6.f;
		PlayUI(GoldSound);
		UE_LOG(LogLF, Display, TEXT("[round] GOLD FISH at %.2f remaining"), Remaining);
	}
	if (!bFinal && Remaining <= FinalRemaining)
	{
		bFinal = true;
		if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this)) Music->SetMusicState(ELFMusicState::Final);
		if (FinalSound)
		{
			FinalSound->bLooping = true;
			FinalTick = UGameplayStatics::SpawnSound2D(this, FinalSound, UIGain(), 1.f, 0.f, nullptr, false, false);
		}
		UE_LOG(LogLF, Display, TEXT("[round] FINAL30 at %.2f remaining"), Remaining);
	}
}

void ULFRoundSubsystem::Tick(float DeltaTime)
{
	if (!bStarted || bEnded) return;
	if (!Widget)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			Widget = CreateWidget<ULFRoundWidget>(PC, ULFRoundWidget::StaticClass());
			Widget->AddToViewport(50);
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}
	GoldBanner = FMath::Max(0.f, GoldBanner - DeltaTime);
	Remaining = FMath::Max(0.f, Remaining - DeltaTime);
	for (TActorIterator<ALFBrawler> It(GetWorld()); It; ++It)
	{
		float& Swim = SwimTimes.FindOrAdd(*It);
		if (It->GetBrawlerState() == ELFBrawlerState::Swimming || It->GetBrawlerState() == ELFBrawlerState::Climbing)
		{
			Swim += DeltaTime;
			if (Swim > LongestSwim)
			{
				LongestSwim = Swim;
				LongestSwimTeam = int32(It->Team);
			}
		}
		else Swim = 0.f;
	}
	if (FinalTick) FinalTick->SetVolumeMultiplier(UIGain());
	// Explicit test flag only: drive actual milestone code and save rendered UI evidence.
	if (bTest)
	{
		TestElapsed += DeltaTime;
		if (TestStep == 0 && TestElapsed > 2.f)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/T04_HUD.png"), true, false);
			++TestStep;
		}
		else if (TestStep == 1 && TestElapsed > 4.f) { SetRemaining(121.f); ++TestStep; }
		else if (TestStep == 2 && TestElapsed > 7.f)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/T04_Gold.png"), true, false);
			++TestStep;
		}
		else if (TestStep == 3 && TestElapsed > 9.f) { SetRemaining(31.f); ++TestStep; }
		else if (TestStep == 4 && TestElapsed > 12.f) { SetRemaining(1.f); ++TestStep; }
	}
	Advance();
}

void ULFRoundSubsystem::EndRound()
{
	if (bEnded) return;
	bEnded = true;
	Remaining = 0.f;
	if (FinalTick) FinalTick->Stop();
	PlayUI(EndSound);
	if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this)) Music->SetMusicState(ELFMusicState::Menu);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (Widget)
		{
			Widget->ShowResults();
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Widget->TakeWidget());
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
		}
		PC->SetPause(true); // freezes physics, AI, input and scoring; Slate stays interactive
	}
	UE_LOG(LogLF, Display, TEXT("[round] END %s | %s"), *GetWinner(), *GetStats());
	if (bTest) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/T04_Results.png"), true, false);
}

void ULFRoundSubsystem::GoldCaught() { PlayUI(SparkleSound); }
void ULFRoundSubsystem::RecordOverboard(ALFBrawler* Brawler) { if (!bEnded && Brawler) ++Overboards[int32(Brawler->Team)]; }
void ULFRoundSubsystem::RecordSteal(ALFBrawler* Brawler) { if (!bEnded && Brawler) ++Steals; }
void ULFRoundSubsystem::RecordSpill(int32 Count) { if (!bEnded) Spills += Count; }
void ULFRoundSubsystem::RecordBank(int32 Team, int32 Value)
{
	if (!bEnded && Value > BiggestBank) { BiggestBank = Value; BiggestBankTeam = Team; }
}

FString ULFRoundSubsystem::GetWinner() const
{
	const ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
	int32 Best = -1;
	FString Winners;
	int32 Count = 0;
	for (int32 I = 0; I < 4; ++I)
	{
		const int32 Score = Fish ? Fish->GetScore(I) : 0;
		if (Score > Best) { Best = Score; Winners = Teams[I]; Count = 1; }
		else if (Score == Best) { Winners += FString(TEXT(" / ")) + Teams[I]; ++Count; }
	}
	return Count > 1 ? FString(TEXT("DRAW - ")) + Winners : Winners + TEXT(" WINS!");
}

FString ULFRoundSubsystem::GetStats() const
{
	int32 Most = 0;
	for (int32 I = 1; I < 4; ++I) if (Overboards[I] > Overboards[Most]) Most = I;
	return FString::Printf(TEXT("Most overboards: %s (%d)\nLongest swim: %s (%.1f s)\nBoats stolen: %d\nFish spilled: %d\nBiggest single bank: %s (%d points)"),
		Overboards[Most] ? Teams[Most] : TEXT("Nobody"), Overboards[Most], LongestSwim > 0.f ? Teams[LongestSwimTeam] : TEXT("Nobody"),
		LongestSwim, Steals, Spills, BiggestBank ? Teams[BiggestBankTeam] : TEXT("Nobody"), BiggestBank);
}

void ULFRoundSubsystem::Restart()
{
	UGameplayStatics::SetGamePaused(this, false);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this)) Music->SetMusicState(ELFMusicState::Main);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void ULFRoundSubsystem::Quit()
{
	UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}

void ULFRoundSubsystem::Deinitialize()
{
	if (FinalTick) { FinalTick->Stop(); FinalTick->DestroyComponent(); }
	if (Widget) Widget->RemoveFromParent();
	bStarted = false;
	Super::Deinitialize();
}
