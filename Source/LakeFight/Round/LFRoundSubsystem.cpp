#include "Round/LFRoundSubsystem.h"

#include "Round/LFGoldFish.h"
#include "Round/LFRoundWidget.h"
#include "Round/LFMenuWidget.h"
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
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Containers/Ticker.h"
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

bool LF::SmokeEnabled()
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("LFSmoke"));
	return bOn;
}

void LF::Smoke(const FString& Line)
{
	if (!SmokeEnabled()) return;
	const FString Path = FPaths::ProjectSavedDir() / TEXT("LFSmoke.txt");
	FFileHelper::SaveStringToFile(FDateTime::Now().ToString(TEXT("%H:%M:%S ")) + Line + LINE_TERMINATOR, *Path,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

bool ULFRoundSubsystem::bMenuDone = false;

void ULFRoundSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Remaining = RoundDuration;
	bStarted = true;
	if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this)) Music->SetMusicState(ELFMusicState::Main);
#if !UE_BUILD_SHIPPING
	bTest = FParse::Param(FCommandLine::Get(), TEXT("LFRoundTest"));
#endif
	// N1 item 5: menu on the first load of a real game session (not in editor PIE, not for the test flag).
	static bool bAudioLoaded = false;
	if (!bAudioLoaded)
	{
		bAudioLoaded = true;
		ULFMenuWidget::LoadAudioSettings(this);
	}
	bInMenu = !bMenuDone && !bTest && !InWorld.IsPlayInEditor() && !FParse::Param(FCommandLine::Get(), TEXT("LFNoMenu"));
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

void ULFRoundSubsystem::StartFromMenu()
{
	if (!bInMenu) return;
	bInMenu = false;
	bMenuDone = true;
	if (Menu) Menu->RemoveFromParent();
	Menu = nullptr;
	UGameplayStatics::SetGamePaused(this, false);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this)) Music->SetMusicState(ELFMusicState::Main);
	UE_LOG(LogLF, Display, TEXT("[round] menu -> Play"));
	LF::Smoke(TEXT("menu -> Play: round starts"));
}

void ULFRoundSubsystem::Tick(float DeltaTime)
{
	if (!bStarted || bEnded) return;
	if (bInMenu)
	{
		// Menu over the lake; the world pauses after MenuSettleTime so the camera / boats have settled (pausing on the first
		// frame froze a motion-blurred camera jump). The subsystem doesn't tick while paused; Play unpauses.
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		MenuTime += DeltaTime;
		if (Menu && PC && MenuTime >= 1.5f && !PC->IsPaused())
		{
			PC->SetPause(true);
		}
		if (!Menu && PC)
		{
			Menu = CreateWidget<ULFMenuWidget>(PC, ULFMenuWidget::StaticClass());
			Menu->AddToViewport(60);
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Menu->TakeWidget());
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
			if (ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(this)) Music->SetMusicState(ELFMusicState::Menu);
			UE_LOG(LogLF, Display, TEXT("[round] main menu shown"));
			LF::Smoke(TEXT("main menu shown"));
			if (LF::SmokeEnabled())
			{
				// Smoke runs press Play themselves after 3 s (real time - the world is paused).
				TWeakObjectPtr<ULFRoundSubsystem> Self(this);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float) { if (Self.IsValid()) Self->StartFromMenu(); return false; }), 3.f);
			}
		}
		return;
	}
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
	if (LF::SmokeEnabled())
	{
		const ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
		LF::Smoke(FString::Printf(TEXT("round END - results screen %s; winner %s; scores R%d B%d G%d Y%d | %s"), Widget ? TEXT("shown") : TEXT("MISSING"),
			*GetWinner(), Fish ? Fish->GetScore(0) : -1, Fish ? Fish->GetScore(1) : -1, Fish ? Fish->GetScore(2) : -1, Fish ? Fish->GetScore(3) : -1, *GetStats()));
		// Smoke runs end themselves 5 s after the results appear (real time; the world is paused).
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { LF::Smoke(TEXT("quit")); FPlatformMisc::RequestExit(false); return false; }), 5.f);
	}
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
