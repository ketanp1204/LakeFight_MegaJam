#include "Round/LFRoundWidget.h"

#include "Round/LFRoundSubsystem.h"
#include "Round/LFRoundCargoComponent.h"
#include "Round/LFGoldFish.h"
#include "Fish/LFFishSubsystem.h"
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
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

namespace
{
	const TCHAR* Names[] = { TEXT("RED"), TEXT("BLUE"), TEXT("GREEN"), TEXT("YELLOW") };
	const FLinearColor Colours[] = { {1.f,.3f,.25f,1.f}, {.3f,.65f,1.f,1.f}, {.35f,.9f,.55f,1.f}, {1.f,.85f,.25f,1.f} };
	const FLinearColor Cream(1.f, .95f, .8f, 1.f);
	void Place(UWidget* Widget, FVector2D At, FVector2D Size)
	{
		UCanvasPanelSlot* Slot = CastChecked<UCanvasPanelSlot>(Widget->Slot);
		Slot->SetPosition(At);
		Slot->SetSize(Size);
	}
}

UTextBlock* ULFRoundWidget::Text(UCanvasPanel* Parent, const FString& Value, FVector2D At, FVector2D Size, int32 FontSize, FLinearColor Colour)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Parent->AddChild(Label);
	Place(Label, At, Size);
	Label->SetText(FText::FromString(Value));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(Colour);
	Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Label->SetShadowOffset(FVector2D(1.f, 2.f));
	return Label;
}

UImage* ULFRoundWidget::Icon(const TCHAR* Name, FVector2D At, float Size)
{
	FString Path = FPaths::ProjectDir() / TEXT("Art/Export/UI") / (FString(Name) + TEXT(".png"));
	if (!FPaths::FileExists(Path)) Path = FString(FPlatformProcess::BaseDir()) / TEXT("LFUI") / (FString(Name) + TEXT(".png"));
	UTexture2D* Texture = UKismetRenderingLibrary::ImportFileAsTexture2D(this, Path);
	Icons.Add(Texture);
	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	Canvas->AddChild(Image);
	Place(Image, At, FVector2D(Size));
	Image->SetBrushFromTexture(Texture);
	return Image;
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
	for (int32 I = 0; I < 4; ++I)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		Canvas->AddChild(Card);
		Place(Card, FVector2D(36.f + I * 306.f, 24.f), FVector2D(290.f, 84.f));
		Card->SetBrushColor(FLinearColor(.025f, .055f, .065f, .92f));
		const TCHAR* IconNames[] = { TEXT("ICON_LF_Crew_Red"), TEXT("ICON_LF_Crew_Blue"), TEXT("ICON_LF_Crew_Green"), TEXT("ICON_LF_Crew_Yellow") };
		Icon(IconNames[I], FVector2D(42.f + I * 306.f, 30.f), 72.f);
		Text(Canvas, Names[I], FVector2D(122.f + I * 306.f, 32.f), FVector2D(175.f, 25.f), 16, Colours[I]);
		Scores.Add(Text(Canvas, TEXT("0"), FVector2D(122.f + I * 306.f, 54.f), FVector2D(175.f, 48.f), 30, Cream));
	}
	const int32 ScoreChildren = Canvas->GetChildrenCount();
	UBorder* TimerCard = WidgetTree->ConstructWidget<UBorder>();
	Canvas->AddChild(TimerCard);
	Place(TimerCard, FVector2D(530.f, 120.f), FVector2D(220.f, 72.f));
	TimerCard->SetBrushColor(FLinearColor(.025f, .055f, .065f, .92f));
	Icon(TEXT("ICON_LF_Clock"), FVector2D(537.f, 126.f), 60.f);
	Timer = Text(Canvas, TEXT("4:00"), FVector2D(602.f, 130.f), FVector2D(140.f, 55.f), 32, Cream);
	Banner = Text(Canvas, TEXT(""), FVector2D(290.f, 204.f), FVector2D(700.f, 55.f), 26, Colours[3]);
	Banner->SetJustification(ETextJustify::Center);
	UBorder* CargoCard = WidgetTree->ConstructWidget<UBorder>();
	Canvas->AddChild(CargoCard);
	Place(CargoCard, FVector2D(36.f, 608.f), FVector2D(390.f, 86.f));
	CargoCard->SetBrushColor(FLinearColor(.025f, .055f, .065f, .92f));
	Icon(TEXT("ICON_LF_Fish"), FVector2D(44.f, 616.f), 68.f);
	Text(Canvas, TEXT("YOUR BOAT"), FVector2D(122.f, 616.f), FVector2D(280.f, 26.f), 15, Cream);
	Cargo = Text(Canvas, TEXT("0 fish"), FVector2D(122.f, 642.f), FVector2D(280.f, 42.f), 23, Cream);
	GoldIcon = Icon(TEXT("ICON_LF_FishGold"), FVector2D(1020.f, 616.f), 60.f);
	Direction = Text(Canvas, TEXT(""), FVector2D(830.f, 644.f), FVector2D(410.f, 50.f), 20, Colours[3]);
	Direction->SetJustification(ETextJustify::Right);
	for (int32 I = ScoreChildren; I < Canvas->GetChildrenCount(); ++I) GameplayWidgets.Add(Canvas->GetChildAt(I));
	return Super::RebuildWidget();
}

void ULFRoundWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
	ULFFishSubsystem* Fish = GetWorld()->GetSubsystem<ULFFishSubsystem>();
	if (!Round || !Fish || !Timer) return;
	for (int32 I = 0; I < 4; ++I) Scores[I]->SetText(FText::AsNumber(Fish->GetScore(I)));
	const int32 Seconds = FMath::CeilToInt(Round->GetRemaining());
	Timer->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60)));
	Timer->SetColorAndOpacity(Seconds <= 30 ? Colours[0] : Cream);
	Banner->SetText(FText::FromString(Round->GoldAlert() && !bResults ? TEXT("GOLD FISH!  Each fish banks for 5") : TEXT("")));
	APlayerController* PC = GetOwningPlayer();
	ALFBoat* Boat = PC ? Cast<ALFBoat>(PC->GetPawn()) : nullptr;
	if (PC) if (ALFBrawler* Brawler = Cast<ALFBrawler>(PC->GetPawn())) Boat = Brawler->GetBoat();
	ULFFishCargoComponent* Hold = Boat ? Boat->FindComponentByClass<ULFFishCargoComponent>() : nullptr;
	const int32 Count = Hold ? Hold->GetCount() : 0;
	const ULFRoundCargoComponent* Gold = Boat ? Boat->FindComponentByClass<ULFRoundCargoComponent>() : nullptr;
	Cargo->SetText(FText::FromString(Boat ? FString::Printf(TEXT("%d fish  /  %d points"), Count, Gold ? Gold->GetValue(Count) : Count) : TEXT("Swimming - find a boat")));
	FString Arrow;
	if (ALFGoldFish* Spot = Round->GetGoldSpot())
	{
		FVector View;
		FRotator Rotation;
		PC->GetPlayerViewPoint(View, Rotation);
		const FVector Delta = Spot->GetActorLocation() - View;
		const float Angle = FMath::FindDeltaAngleDegrees(float(Rotation.Yaw), float(Delta.Rotation().Yaw));
		const TCHAR* Mark = FMath::Abs(Angle) < 30.f ? TEXT("\u2191") : (FMath::Abs(Angle) > 150.f ? TEXT("\u2193") : (Angle > 0.f ? TEXT("\u2192") : TEXT("\u2190")));
		Arrow = FString::Printf(TEXT("GOLD %s  %.0f m"), Mark, Delta.Size2D() / 100.f);
	}
	Direction->SetText(FText::FromString(bResults ? TEXT("") : Arrow));
	GoldIcon->SetVisibility(!bResults && Round->GetGoldSpot() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void ULFRoundWidget::ShowResults()
{
	if (bResults) return;
	bResults = true;
	for (UWidget* Gameplay : GameplayWidgets) Gameplay->SetVisibility(ESlateVisibility::Hidden);
	ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>();
	Results = WidgetTree->ConstructWidget<UBorder>();
	Canvas->AddChild(Results);
	Place(Results, FVector2D(270.f, 116.f), FVector2D(740.f, 588.f));
	Results->SetBrushColor(FLinearColor(.018f, .035f, .045f, .98f));
	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>();
	Results->AddChild(Panel);
	Text(Panel, TEXT("LINES UP. TIME'S UP."), FVector2D(36.f, 22.f), FVector2D(670.f, 40.f), 19, Cream);
	UTextBlock* Winner = Text(Panel, Round->GetWinner(), FVector2D(36.f, 68.f), FVector2D(670.f, 85.f), 28, Colours[3]);
	Winner->SetAutoWrapText(true);
	Text(Panel, TEXT("FINAL SCORES ABOVE"), FVector2D(36.f, 151.f), FVector2D(670.f, 28.f), 14, Cream);
	Text(Panel, Round->GetStats(), FVector2D(36.f, 195.f), FVector2D(670.f, 220.f), 22, Cream);
	RestartButton = WidgetTree->ConstructWidget<UButton>();
	QuitButton = WidgetTree->ConstructWidget<UButton>();
	for (int32 I = 0; I < 2; ++I)
	{
		UButton* Button = I == 0 ? RestartButton.Get() : QuitButton.Get();
		Panel->AddChild(Button);
		Place(Button, FVector2D(36.f + I * 346.f, 459.f), FVector2D(320.f, 65.f));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(I == 0 ? TEXT("RESTART") : TEXT("QUIT")));
		Label->SetColorAndOpacity(FLinearColor(.02f, .04f, .05f, 1.f));
		FSlateFontInfo Font = Label->GetFont(); Font.Size = 24; Label->SetFont(Font);
		Button->AddChild(Label);
	}
	RestartButton->OnClicked.AddDynamic(this, &ULFRoundWidget::Restart);
	QuitButton->OnClicked.AddDynamic(this, &ULFRoundWidget::Quit);
	Text(Panel, TEXT("Mouse / arrows / D-pad to choose  -  Enter / A to confirm"), FVector2D(36.f, 542.f), FVector2D(670.f, 30.f), 14, Cream);
	SelectButton(0);
}

void ULFRoundWidget::SelectButton(int32 Index)
{
	Selection = Index;
	RestartButton->SetBackgroundColor(Index == 0 ? Colours[3] : Cream);
	QuitButton->SetBackgroundColor(Index == 1 ? Colours[3] : Cream);
}

FReply ULFRoundWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bResults)
	{
		const FName Key = InKeyEvent.GetKey().GetFName();
		if (Key == TEXT("Left") || Key == TEXT("Right") || Key == TEXT("Tab") || Key == TEXT("Gamepad_DPad_Left") || Key == TEXT("Gamepad_DPad_Right"))
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

void ULFRoundWidget::Restart() { GetWorld()->GetSubsystem<ULFRoundSubsystem>()->Restart(); }
void ULFRoundWidget::Quit() { GetWorld()->GetSubsystem<ULFRoundSubsystem>()->Quit(); }
