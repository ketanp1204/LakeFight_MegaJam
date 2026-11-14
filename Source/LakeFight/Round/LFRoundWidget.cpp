#include "Round/LFRoundWidget.h"

#include "Round/LFRoundSubsystem.h"
#include "Round/LFRoundCargoComponent.h"
#include "Round/LFGoldFish.h"
#include "Round/LFUIStyle.h"
#include "Fish/LFFishSubsystem.h"
#include "Fish/LFBoatFishLabels.h" // T04 hook
#include "Fish/LFFishCargoComponent.h"
#include "Brawler/LFBrawler.h"
#include "Boat/LFBoat.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const TCHAR* TeamNames[] = { TEXT("RED"), TEXT("BLUE"), TEXT("GREEN"), TEXT("YELLOW") };
	const TCHAR* CrewIcons[] = { TEXT("Crew_Red"), TEXT("Crew_Blue"), TEXT("Crew_Green"), TEXT("Crew_Yellow") };

	// Layout on the 1280x720 design canvas (= 1920x1080 reference at 2/3).
	constexpr float ChipX = 36.f, ChipStep = 306.f, ChipY = 18.f, ChipW = 290.f, ChipH = 78.f;
	const FVector2D BoatPanelAt(36.f, 612.f), BoatPanelSize(410.f, 84.f);

	UCanvasPanel* Group(UWidgetTree* Tree, UCanvasPanel* Parent)
	{
		UCanvasPanel* Panel = Tree->ConstructWidget<UCanvasPanel>();
		Parent->AddChild(Panel);
		LF::UI::Place(Panel, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Panel;
	}

	float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T); }
}

UImage* ULFRoundWidget::Icon(UCanvasPanel* Parent, const FString& Name, FVector2D At, float Size)
{
	TObjectPtr<UTexture2D>& Texture = Icons.FindOrAdd(Name);
	if (!Texture) Texture = LF::UI::LoadPng(this, TEXT("Art/Export/UI"), FString(TEXT("ICON_LF_")) + Name + TEXT(".png"));
	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	Parent->AddChild(Image);
	LF::UI::Place(Image, At, FVector2D(Size));
	if (Texture) Image->SetBrushFromTexture(Texture);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Image;
}

FVector2D ULFRoundWidget::ChipScoreCentre(int32 Team) const
{
	return FVector2D(ChipX + Team * ChipStep + 120.f, ChipY + 50.f);
}

int32 ULFRoundWidget::PlayerTeam() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (const ALFBrawler* Brawler = Cast<ALFBrawler>(Pawn)) return int32(Brawler->Team);
	if (const ALFBoat* Boat = Cast<ALFBoat>(Pawn)) if (const ALFBrawler* Driver = Boat->GetDriver()) return int32(Driver->Team);
	return -1;
}

TSharedRef<SWidget> ULFRoundWidget::RebuildWidget()
{
	if (Canvas) return Super::RebuildWidget();
	SetIsFocusable(true);
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	WidgetTree->RootWidget = Scale;
	USizeBox* Design = WidgetTree->ConstructWidget<USizeBox>();
	Design->SetWidthOverride(1280.f);
	Design->SetHeightOverride(720.f);
	Scale->AddChild(Design);
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Design->AddChild(Canvas);

	HudGroup = Group(WidgetTree, Canvas);
	// T04 hook: projected per-boat cargo counts, behind the fixed HUD.
	ULFBoatFishLabels* BoatLabels = CreateWidget<ULFBoatFishLabels>(GetOwningPlayer(), ULFBoatFishLabels::StaticClass());
	HudGroup->AddChild(BoatLabels);
	LF::UI::Place(BoatLabels, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));

	// Team score chips: wood chip, team-colour badge with the crew icon, team name in its colour, big bold score.
	for (int32 I = 0; I < 4; ++I)
	{
		const float X = ChipX + I * ChipStep;
		LF::UI::Panel(WidgetTree, HudGroup, FVector2D(X, ChipY), FVector2D(ChipW, ChipH));
		LF::UI::Panel(WidgetTree, HudGroup, FVector2D(X + 8.f, ChipY + 8.f), FVector2D(62.f, 62.f), LF::UI::TeamColours[I], 10.f, LF::UI::WoodDark, 2.f);
		Icon(HudGroup, CrewIcons[I], FVector2D(X + 9.f, ChipY + 9.f), 60.f);
		LF::UI::Label(WidgetTree, HudGroup, TeamNames[I], FVector2D(X + 82.f, ChipY + 6.f), FVector2D(200.f, 24.f), 15, LF::UI::TeamColours[I], true);
		UTextBlock* Score = LF::UI::Label(WidgetTree, HudGroup, TEXT("0"), FVector2D(X + 82.f, ChipY + 24.f), FVector2D(200.f, 52.f), 36, LF::UI::Cream, true);
		Score->SetRenderTransformPivot(FVector2D(0.1f, 0.5f));
		Scores.Add(Score);
	}

	// Timer pill.
	TimerPill = LF::UI::Panel(WidgetTree, HudGroup, FVector2D(550.f, 108.f), FVector2D(180.f, 56.f), LF::UI::Wood, 28.f);
	Icon(HudGroup, TEXT("Clock"), FVector2D(560.f, 112.f), 48.f);
	Timer = LF::UI::Label(WidgetTree, HudGroup, TEXT("4:00"), FVector2D(608.f, 113.f), FVector2D(112.f, 46.f), 32, LF::UI::Cream, true, 1);

	// Gold banner (red ribbon, gold lettering) under the timer while the gold alert runs.
	GoldBannerGroup = Group(WidgetTree, HudGroup);
	LF::UI::Panel(WidgetTree, GoldBannerGroup, FVector2D(370.f, 178.f), FVector2D(540.f, 52.f), LF::UI::Ribbon, 12.f, LF::UI::Cream, 3.f);
	Icon(GoldBannerGroup, TEXT("FishGold"), FVector2D(382.f, 180.f), 48.f);
	LF::UI::Label(WidgetTree, GoldBannerGroup, TEXT("GOLD FISH!  Each fish banks for 5"), FVector2D(436.f, 188.f), FVector2D(464.f, 34.f), 21, LF::UI::Gold, true, 1);

	// Your boat panel.
	LF::UI::Panel(WidgetTree, HudGroup, BoatPanelAt, BoatPanelSize);
	Icon(HudGroup, TEXT("Fish"), BoatPanelAt + FVector2D(10.f, 9.f), 66.f);
	LF::UI::Label(WidgetTree, HudGroup, TEXT("YOUR BOAT"), BoatPanelAt + FVector2D(88.f, 6.f), FVector2D(300.f, 24.f), 15, LF::UI::Cream, true);
	Cargo = LF::UI::Label(WidgetTree, HudGroup, TEXT("0 fish"), BoatPanelAt + FVector2D(88.f, 28.f), FVector2D(316.f, 46.f), 28, LF::UI::Cream, true);

	// Gold arrow pill (bottom right) while a gold spot exists.
	GoldArrowGroup = Group(WidgetTree, HudGroup);
	LF::UI::Panel(WidgetTree, GoldArrowGroup, FVector2D(904.f, 628.f), FVector2D(340.f, 64.f), LF::UI::Wood, 32.f, LF::UI::Gold, 3.f);
	Icon(GoldArrowGroup, TEXT("FishGold"), FVector2D(916.f, 630.f), 60.f);
	Direction = LF::UI::Label(WidgetTree, GoldArrowGroup, TEXT(""), FVector2D(980.f, 638.f), FVector2D(250.f, 44.f), 26, LF::UI::Gold, true);

	// N2 item 13: round-start hints, low centre, non-blocking.
	HintGroup = Group(WidgetTree, HudGroup);
	LF::UI::Panel(WidgetTree, HintGroup, FVector2D(390.f, 532.f), FVector2D(500.f, 56.f));
	HintText = LF::UI::Label(WidgetTree, HintGroup, TEXT(""), FVector2D(390.f, 542.f), FVector2D(500.f, 38.f), 22, LF::UI::Cream, true, 1);
	HintGroup->SetRenderOpacity(0.f);
	HintGroup->SetVisibility(ESlateVisibility::Collapsed);
	return Super::RebuildWidget();
}

void ULFRoundWidget::AddPop(int32 Team, int32 Amount, bool bFromBoat)
{
	UTextBlock* Pop = LF::UI::Label(WidgetTree, HudGroup, FString::Printf(TEXT("+%d"), Amount), FVector2D::ZeroVector, FVector2D(160.f, 56.f),
		bFromBoat ? 40 : 30, LF::UI::TeamColours[Team], true, 1);
	Pop->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PopTexts.Add(Pop);
	const FVector2D To = ChipScoreCentre(Team);
	PopFrom.Add(bFromBoat ? BoatPanelAt + FVector2D(200.f, 20.f) : To + FVector2D(0.f, 70.f));
	PopTo.Add(To);
	PopAge.Add(0.f);
	PopDuration.Add(bFromBoat ? 0.9f : 0.7f);
	PopTeam.Add(Team);
}

void ULFRoundWidget::TickPops(float DeltaTime)
{
	for (int32 I = PopTexts.Num() - 1; I >= 0; --I)
	{
		PopAge[I] += DeltaTime;
		const float T = PopAge[I] / PopDuration[I];
		if (T >= 1.f)
		{
			ChipBump[PopTeam[I]] = 0.3f;
			PopTexts[I]->RemoveFromParent();
			PopTexts.RemoveAt(I); PopFrom.RemoveAt(I); PopTo.RemoveAt(I); PopAge.RemoveAt(I); PopDuration.RemoveAt(I); PopTeam.RemoveAt(I);
			continue;
		}
		// Ease towards the chip on a shallow arc; grow in, shrink as it lands.
		const FVector2D P = FMath::Lerp(PopFrom[I], PopTo[I], EaseOut(T)) + FVector2D(0.f, -50.f * FMath::Sin(PI * T));
		LF::UI::Place(PopTexts[I], P - FVector2D(80.f, 28.f), FVector2D(160.f, 56.f));
		const float Scale = T < 0.15f ? FMath::Lerp(0.5f, 1.3f, T / 0.15f) : FMath::Lerp(1.3f, 0.8f, (T - 0.15f) / 0.85f);
		PopTexts[I]->SetRenderScale(FVector2D(Scale));
		PopTexts[I]->SetRenderOpacity(T > 0.8f ? (1.f - T) / 0.2f : 1.f);
	}
	for (int32 I = 0; I < 4; ++I)
	{
		ChipBump[I] = FMath::Max(0.f, ChipBump[I] - DeltaTime);
		Scores[I]->SetRenderScale(FVector2D(1.f + ChipBump[I] / 0.3f * 0.35f));
	}
}

void ULFRoundWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
	ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
	if (!Round || !Fish || !Timer) return;
	if (bResults)
	{
		ResultsAge += InDeltaTime;
		if (ResultsGroup) ResultsGroup->SetRenderOpacity(FMath::Clamp(ResultsAge / 0.35f, 0.f, 1.f));
		return;
	}

	// Scores + banking pops.
	const int32 Mine = PlayerTeam();
	for (int32 I = 0; I < 4; ++I)
	{
		const int32 Score = Fish->GetScore(I);
		if (bScoresKnown && Score > LastScores[I]) AddPop(I, Score - LastScores[I], I == Mine);
		LastScores[I] = Score;
		Scores[I]->SetText(FText::AsNumber(Score));
	}
	bScoresKnown = true;
	TickPops(InDeltaTime);

	// Timer pill: red ribbon from 0:30.
	const int32 Seconds = FMath::CeilToInt(Round->GetRemaining());
	Timer->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60)));
	if ((Seconds <= 30) != bTimerRed)
	{
		bTimerRed = Seconds <= 30;
		TimerPill->SetBrush(LF::UI::RoundBrush(bTimerRed ? LF::UI::Ribbon : LF::UI::Wood, 28.f, bTimerRed ? LF::UI::Cream : LF::UI::WoodDark, 3.f));
	}
	APlayerController* PC = GetOwningPlayer();
	// Transient elements give way to the pause menu (scores, timer and boat panel stay).
	const bool bPaused = PC && PC->IsPaused();
	GoldBannerGroup->SetVisibility(Round->GoldAlert() && !bPaused ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ALFBoat* Boat = PC ? Cast<ALFBoat>(PC->GetPawn()) : nullptr;
	if (PC) if (ALFBrawler* Brawler = Cast<ALFBrawler>(PC->GetPawn())) Boat = Brawler->GetBoat();
	ULFFishCargoComponent* Hold = ULFFishCargoComponent::Get(Boat);
	const int32 Count = Hold ? Hold->GetCount() : 0;
	const ULFRoundCargoComponent* Gold = Boat ? Boat->FindComponentByClass<ULFRoundCargoComponent>() : nullptr;
	Cargo->SetText(FText::FromString(Boat ? FString::Printf(TEXT("%d fish  /  %d pts"), Count, Gold ? Gold->GetValue(Count) : Count) : TEXT("Swimming - find a boat")));

	ALFGoldFish* Spot = Round->GetGoldSpot();
	GoldArrowGroup->SetVisibility(Spot && PC && !bPaused ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (Spot && PC)
	{
		FVector View;
		FRotator Rotation;
		PC->GetPlayerViewPoint(View, Rotation);
		const FVector Delta = Spot->GetActorLocation() - View;
		const float Angle = FMath::FindDeltaAngleDegrees(float(Rotation.Yaw), float(Delta.Rotation().Yaw));
		const TCHAR* Mark = FMath::Abs(Angle) < 30.f ? TEXT("↑") : (FMath::Abs(Angle) > 150.f ? TEXT("↓") : (Angle > 0.f ? TEXT("→") : TEXT("←")));
		Direction->SetText(FText::FromString(FString::Printf(TEXT("GOLD %s  %.0f m"), Mark, Delta.Size2D() / 100.f)));
	}

	// N2 item 13: three hints, 5 s each from 0.5 s into the round, fading in/out over 0.4 s (round time - freezes in pause).
	static const TCHAR* Hints[] = { TEXT("Stop on a fish spot to catch"), TEXT("Bank at your dock"), TEXT("Ram boats to spill their fish") };
	const float HintTime = Round->GetRoundTime() - 0.5f;
	const int32 HintIndex = HintTime >= 0.f ? FMath::FloorToInt(HintTime / 5.f) : -1;
	float HintAlpha = 0.f;
	if (HintIndex >= 0 && HintIndex < 3)
	{
		const float Local = HintTime - HintIndex * 5.f;
		HintAlpha = FMath::Clamp(FMath::Min(Local, 5.f - Local) / 0.4f, 0.f, 1.f);
		if (HintIndex != HintShown)
		{
			HintShown = HintIndex;
			HintText->SetText(FText::FromString(Hints[HintIndex]));
			LF::Smoke(FString::Printf(TEXT("hint %d: %s"), HintIndex + 1, Hints[HintIndex]));
		}
	}
	HintAlpha = bPaused ? 0.f : HintAlpha;
	HintGroup->SetRenderOpacity(HintAlpha);
	// Rounded-box outlines ignore inherited render opacity: collapse the card when it's fully faded.
	HintGroup->SetVisibility(HintAlpha > 0.01f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void ULFRoundWidget::ShowResults()
{
	if (bResults) return;
	bResults = true;
	HudGroup->SetVisibility(ESlateVisibility::Collapsed);
	ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
	ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
	ResultsGroup = Group(WidgetTree, Canvas);
	ResultsGroup->SetRenderOpacity(0.f);
	UCanvasPanel* R = ResultsGroup;

	// Dim the frozen lake a little, then the wood card.
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	R->AddChild(Dim);
	LF::UI::Place(Dim, FVector2D(-400.f, -300.f), FVector2D(2080.f, 1320.f));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
	LF::UI::Panel(WidgetTree, R, FVector2D(290.f, 70.f), FVector2D(700.f, 640.f), LF::UI::Wood, 18.f);
	LF::UI::Label(WidgetTree, R, TEXT("TIME'S UP!"), FVector2D(290.f, 82.f), FVector2D(700.f, 30.f), 18, LF::UI::Cream, true, 1);

	// Winner banner in the winner's team colour (red ribbon on a draw).
	const int32 Winner = Round->GetWinnerTeam();
	LF::UI::Panel(WidgetTree, R, FVector2D(330.f, 114.f), FVector2D(620.f, 78.f), Winner >= 0 ? LF::UI::TeamColours[Winner] : LF::UI::Ribbon, 14.f, LF::UI::Cream, 3.f);
	if (Winner >= 0)
	{
		Icon(R, CrewIcons[Winner], FVector2D(342.f, 118.f), 70.f);
		Icon(R, CrewIcons[Winner], FVector2D(868.f, 118.f), 70.f);
	}
	FString WinnerText = Round->GetWinner().ToUpper();
	LF::UI::Label(WidgetTree, R, WinnerText, FVector2D(410.f, 124.f), FVector2D(460.f, 60.f), Winner >= 0 ? 40 : 24, LF::UI::Cream, true, 1)->SetAutoWrapText(true);

	// Final scores: four team chips.
	for (int32 I = 0; I < 4; ++I)
	{
		const float X = 330.f + I * 160.f;
		LF::UI::Panel(WidgetTree, R, FVector2D(X, 206.f), FVector2D(140.f, 56.f), LF::UI::WoodLight, 10.f, LF::UI::TeamColours[I], 3.f);
		Icon(R, CrewIcons[I], FVector2D(X + 6.f, 210.f), 48.f);
		LF::UI::Label(WidgetTree, R, FString::FromInt(Fish ? Fish->GetScore(I) : 0), FVector2D(X + 56.f, 208.f), FVector2D(80.f, 50.f), 30, LF::UI::Cream, true, 1);
	}

	// Stat rows: icon, label, value (team colour when it belongs to a team).
	const TArray<FLFStatRow> Rows = Round->GetStatRows();
	for (int32 I = 0; I < Rows.Num(); ++I)
	{
		const float Y = 278.f + I * 50.f;
		LF::UI::Panel(WidgetTree, R, FVector2D(330.f, Y), FVector2D(620.f, 42.f), LF::UI::WoodLight, 10.f, LF::UI::WoodDark, 2.f);
		UImage* RowIcon = Icon(R, Rows[I].Icon, FVector2D(338.f, Y + 2.f), 38.f);
		if (Rows[I].Team < 0 && Rows[I].Icon.StartsWith(TEXT("Crew"))) RowIcon->SetRenderOpacity(0.3f); // "Nobody": greyed crew
		LF::UI::Label(WidgetTree, R, Rows[I].Label, FVector2D(388.f, Y + 8.f), FVector2D(300.f, 28.f), 18, LF::UI::Cream, false);
		LF::UI::Label(WidgetTree, R, Rows[I].Value, FVector2D(640.f, Y + 7.f), FVector2D(296.f, 30.f), 20,
			Rows[I].Team >= 0 ? LF::UI::TeamColours[Rows[I].Team] : LF::UI::Cream, true, 2);
	}

	RestartButton = LF::UI::Button(WidgetTree, R, TEXT("RESTART"), FVector2D(330.f, 548.f), FVector2D(300.f, 60.f), 24);
	QuitButton = LF::UI::Button(WidgetTree, R, TEXT("QUIT"), FVector2D(650.f, 548.f), FVector2D(300.f, 60.f), 24);
	RestartButton->OnClicked.AddDynamic(this, &ULFRoundWidget::Restart);
	QuitButton->OnClicked.AddDynamic(this, &ULFRoundWidget::Quit);
	RestartButton->OnHovered.AddDynamic(this, &ULFRoundWidget::OnHoverRestart);
	QuitButton->OnHovered.AddDynamic(this, &ULFRoundWidget::OnHoverQuit);
	LF::UI::Label(WidgetTree, R, TEXT("Mouse / arrows / D-pad to choose  -  Enter / A to confirm"), FVector2D(330.f, 626.f), FVector2D(620.f, 26.f), 14, LF::UI::Cream, false, 1);
	Selection = 0;
	LF::UI::SetSelected(RestartButton, true);
	LF::UI::SetSelected(QuitButton, false);
}

void ULFRoundWidget::SelectButton(int32 Index)
{
	if (Index != Selection) LF::UI::Sound(this, TEXT("MenuMove"));
	Selection = Index;
	LF::UI::SetSelected(RestartButton, Index == 0);
	LF::UI::SetSelected(QuitButton, Index == 1);
}

FReply ULFRoundWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bResults)
	{
		const FName Key = InKeyEvent.GetKey().GetFName();
		if (Key == TEXT("Left") || Key == TEXT("Right") || Key == TEXT("Tab") || Key == TEXT("Gamepad_DPad_Left") || Key == TEXT("Gamepad_DPad_Right")
			|| Key == TEXT("Gamepad_LeftStick_Left") || Key == TEXT("Gamepad_LeftStick_Right") || Key == TEXT("A") || Key == TEXT("D"))
		{
			SelectButton(1 - Selection);
			return FReply::Handled();
		}
		if (Key == TEXT("Enter") || Key == TEXT("SpaceBar") || Key == TEXT("Gamepad_FaceButton_Bottom"))
		{
			if (Selection == 0) Restart(); else Quit();
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void ULFRoundWidget::Restart() { LF::UI::Sound(this, TEXT("MenuSelect")); GetWorld()->GetSubsystem<ULFRoundSubsystem>()->Restart(); }
void ULFRoundWidget::Quit() { LF::UI::Sound(this, TEXT("MenuSelect")); GetWorld()->GetSubsystem<ULFRoundSubsystem>()->Quit(); }
void ULFRoundWidget::OnHoverRestart() { SelectButton(0); }
void ULFRoundWidget::OnHoverQuit() { SelectButton(1); }
