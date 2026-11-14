#include "Round/LFMenuWidget.h"

#include "Round/LFRoundSubsystem.h"
#include "Round/LFUIStyle.h"
#include "Audio/LFAudio.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Water/LFWater.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "UObject/UObjectIterator.h"

#if !UE_BUILD_SHIPPING
// lf.menu.page <0|1|2> [delay s]: show main / options / credits on the open menu (evidence screenshots; real-time delay).
static FAutoConsoleCommand CmdLFMenuPage(TEXT("lf.menu.page"), TEXT("lf.menu.page <0 main|1 options|2 credits> [delay s]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const int32 Index = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.01f;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Index](float)
		{
			for (TObjectIterator<ULFMenuWidget> It; It; ++It) if (It->IsInViewport()) It->DebugShowPage(Index);
			return false;
		}), FMath::Max(Delay, 0.01f));
	}));
// lf.menu.item <index> [delay s]: press a main-page item on the open menu / pause menu (real-time delay).
static FAutoConsoleCommand CmdLFMenuItem(TEXT("lf.menu.item"), TEXT("lf.menu.item <index> [delay s]"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const int32 Index = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.01f;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Index](float)
		{
			for (TObjectIterator<ULFMenuWidget> It; It; ++It) if (It->IsInViewport()) { It->DebugActivate(Index); break; }
			return false;
		}), FMath::Max(Delay, 0.01f));
	}));
#endif

namespace
{
	const TCHAR* MenuSection = TEXT("LakeFight.Audio");

	void MenuPlace(UWidget* Widget, FVector2D At, FVector2D Size)
	{
		UCanvasPanelSlot* Slot = CastChecked<UCanvasPanelSlot>(Widget->Slot);
		Slot->SetPosition(At);
		Slot->SetSize(Size);
	}

	// Credits: the director-approved list from Docs/ItchPage.md; the Uppbeat lines are verbatim from Docs/ASSETS.md.
	const TCHAR* MenuCreditsText =
		TEXT("Ketan Patel - design, direction, playtesting and final creative decisions.\n")
		TEXT("Claude Code - Unreal gameplay and editor implementation, plus code review.\n")
		TEXT("Codex - original scripted Blender art and icons, audio processing, documentation, T04 fish/round work.\n")
		TEXT("Claude in chat - production support, task briefs, reviews and scheduling.\n")
		TEXT("\n")
		TEXT("Epic Games - Unreal Engine and the UE5 Mannequin (Manny), used under the Unreal Engine EULA.\n")
		TEXT("Adobe Mixamo - animation clips retargeted to Manny (Adobe Mixamo terms).\n")
		TEXT("Sonniss GDC 2026 Game Audio Bundle - licensed sound effects, ambience and voices, edited for the game.\n")
		TEXT("\n")
		TEXT("Music: 'Funk Formula' by Nick Petrov on #Uppbeat https://uppbeat.io/c/nick-petrov\n")
		TEXT("License code: ILNEZRJIJP1DSTBI\n")
		TEXT("\n")
		TEXT("Music: 'Boardwalk Jaunt' by Sky Toes on #Uppbeat https://uppbeat.io/c/sky-toes\n")
		TEXT("License code: TBK6NUHLZIDMPKOK\n")
		TEXT("\n")
		TEXT("Made for the Epic Games MegaJam 2026 with Unreal Engine 5.8.");

	// N2 item 13: how-to-play card (pause menu). Controls match Docs/ItchPage.md.
	const TCHAR* HowToGoal =
		TEXT("Stop on a fish spot to catch fish, then bank them at your team's dock for points. Ram or whack other boats to ")
		TEXT("spill their catch - or knock the driver out and steal the boat. Most points after 4 minutes wins; at 2:00 a gold ")
		TEXT("fish spot appears and every gold fish banks for 5.");
	const TCHAR* HowToActions =
		TEXT("Row forward / reverse (driving)\nSteer (driving)\nMove, swim\nLook, aim the paddle\nTake an empty seat / stand up\n")
		TEXT("Grab a boat / let go (swimming)\nPaddle swing (hold to charge)\nPause");
	const TCHAR* HowToKeys = TEXT("W / S\nA / D\nWASD\nMouse\nE\nE\nLeft mouse\nEsc");
	const TCHAR* HowToPad = TEXT("RT / LT\nLeft stick\nLeft stick\nRight stick\nY\nY\nRT\nStart");
}

UTextBlock* ULFMenuWidget::Text(UCanvasPanel* Parent, const FString& Value, FVector2D At, FVector2D Size, int32 FontSize, FLinearColor Colour)
{
	return LF::UI::Label(WidgetTree, Parent, Value, At, Size, FontSize, Colour, FontSize >= 20);
}

UButton* ULFMenuWidget::MakeButton(UCanvasPanel* Parent, const FString& Label, FVector2D At, FVector2D Size, int32 FontSize)
{
	return LF::UI::Button(WidgetTree, Parent, Label, At, Size, FontSize);
}

USlider* ULFMenuWidget::MakeSlider(UCanvasPanel* Parent, FVector2D At, FVector2D Size, float Value)
{
	USlider* Slider = WidgetTree->ConstructWidget<USlider>();
	Parent->AddChild(Slider);
	MenuPlace(Slider, At, Size);
	Slider->SetValue(Value);
	Slider->SetStepSize(0.05f);
	Slider->SetSliderBarColor(LF::UI::Cream);
	Slider->SetSliderHandleColor(LF::UI::Ribbon);
	Slider->OnValueChanged.AddDynamic(this, &ULFMenuWidget::OnSliderChanged);
	return Slider;
}

TSharedRef<SWidget> ULFMenuWidget::RebuildWidget()
{
	if (Canvas) return Super::RebuildWidget();
	SetIsFocusable(true);
	// Root: full-screen backdrop (dims the frozen round in pause mode; empty for the main menu) under the scaled design canvas.
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, bPauseMode ? 0.5f : 0.f));
	UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(Backdrop);
	BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
	BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	UOverlaySlot* ScaleSlot = Root->AddChildToOverlay(Scale);
	ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
	ScaleSlot->SetVerticalAlignment(VAlign_Fill);
	USizeBox* Design = WidgetTree->ConstructWidget<USizeBox>();
	Design->SetWidthOverride(1280.f);
	Design->SetHeightOverride(720.f);
	Scale->AddChild(Design);
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Design->AddChild(Canvas);

	if (bPauseMode)
	{
		// Pause: no logo (the round is behind it) - a red ribbon title between the HUD timer and the buttons.
		LF::UI::Panel(WidgetTree, Canvas, FVector2D(500.f, 196.f), FVector2D(280.f, 56.f), LF::UI::Ribbon, 10.f, LF::UI::Cream, 3.f);
		LF::UI::Label(WidgetTree, Canvas, TEXT("PAUSED"), FVector2D(500.f, 200.f), FVector2D(280.f, 48.f), 34, LF::UI::Cream, true, 1);
	}
	else
	{
	// Logo: B12a centred 1920x1080 title lockup over the whole design canvas (editor: Art/Export/KeyArt, packaged: LFUI next to
	// the executable - see LakeFight.Build.cs). The sign + ribbon fill the top ~40%; the buttons sit below it.
	LogoTexture = LF::UI::LoadPng(this, TEXT("Art/Export/KeyArt"), TEXT("LF_TitleLockup_1920x1080_Light.png"));
	UImage* Logo = WidgetTree->ConstructWidget<UImage>();
	LogoImage = Logo;
	Canvas->AddChild(Logo);
	MenuPlace(Logo, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
	if (LogoTexture) Logo->SetBrushFromTexture(LogoTexture);
	else Text(Canvas, TEXT("LAKE FIGHT"), FVector2D(380.f, 90.f), FVector2D(520.f, 90.f), 56, LF::UI::Cream);
	}

	// Main page.
	MainPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->AddChild(MainPage);
	MenuPlace(MainPage, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
	const TCHAR* MenuLabels[] = { TEXT("PLAY"), TEXT("OPTIONS"), TEXT("CREDITS"), TEXT("QUIT") };
	const TCHAR* PauseLabels[] = { TEXT("RESUME"), TEXT("HOW TO PLAY"), TEXT("RESTART"), TEXT("OPTIONS"), TEXT("QUIT TO MENU") };
	for (int32 I = 0; I < (bPauseMode ? 5 : 4); ++I)
	{
		MainButtons.Add(bPauseMode ? MakeButton(MainPage, PauseLabels[I], FVector2D(480.f, 266.f + I * 66.f), FVector2D(320.f, 56.f), 22)
			: MakeButton(MainPage, MenuLabels[I], FVector2D(480.f, 312.f + I * 74.f), FVector2D(320.f, 60.f)));
	}
	if (bPauseMode)
	{
		MainButtons[4]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnMain4);
		MainButtons[4]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverMain4);
	}
	MainButtons[0]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnMain0);
	MainButtons[1]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnMain1);
	MainButtons[2]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnMain2);
	MainButtons[3]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnMain3);
	MainButtons[0]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverPlay);
	MainButtons[1]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverOptions);
	MainButtons[2]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverCredits);
	MainButtons[3]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverQuit);
	Text(MainPage, bPauseMode ? TEXT("Mouse / arrows / D-pad to choose  -  Enter / A to confirm  -  Esc / Start / B resume")
		: TEXT("Mouse / arrows / D-pad to choose  -  Enter / A to confirm  -  Esc / B back"), bPauseMode ? FVector2D(340.f, 590.f) : FVector2D(280.f, 650.f), bPauseMode ? FVector2D(600.f, 22.f) : FVector2D(720.f, 30.f), bPauseMode ? 12 : 14, LF::UI::Cream)
		->SetJustification(ETextJustify::Center); // pause: under the buttons, clear of the HUD boat panel and gold pill

	// Options page: two rows (label + slider), Back.
	OptionsPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->AddChild(OptionsPage);
	MenuPlace(OptionsPage, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
	const float OY = bPauseMode ? 0.f : 26.f; // below the logo ribbon on the main menu; clear of the HUD boat panel in pause
	LF::UI::Panel(WidgetTree, OptionsPage, FVector2D(300.f, 270.f + OY), FVector2D(680.f, 330.f)); // rows 286 / 352 / 418, hint 482, Back 520
	const ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(this);
	const float Music = Audio ? Audio->GetCategoryVolume(ELFSoundCategory::Music) : 1.f;
	const float Sfx = Audio ? Audio->GetCategoryVolume(ELFSoundCategory::SFX) : 1.f;
	MusicRow = MakeButton(OptionsPage, TEXT("MUSIC"), FVector2D(330.f, 286.f + OY), FVector2D(200.f, 54.f), 22);
	SfxRow = MakeButton(OptionsPage, TEXT("SOUND FX"), FVector2D(330.f, 352.f + OY), FVector2D(200.f, 54.f), 22);
	GraphicsRow = MakeButton(OptionsPage, TEXT("GRAPHICS"), FVector2D(330.f, 418.f + OY), FVector2D(200.f, 54.f), 22);
	MusicSlider = MakeSlider(OptionsPage, FVector2D(560.f, 293.f + OY), FVector2D(300.f, 40.f), FMath::Clamp(Music, 0.f, 1.f));
	SfxSlider = MakeSlider(OptionsPage, FVector2D(560.f, 359.f + OY), FVector2D(300.f, 40.f), FMath::Clamp(Sfx, 0.f, 1.f));
	MusicValue = Text(OptionsPage, TEXT(""), FVector2D(880.f, 295.f + OY), FVector2D(90.f, 40.f), 20, LF::UI::Cream);
	SfxValue = Text(OptionsPage, TEXT(""), FVector2D(880.f, 361.f + OY), FVector2D(90.f, 40.f), 20, LF::UI::Cream);
	GraphicsValue = Text(OptionsPage, TEXT(""), FVector2D(560.f, 425.f + OY), FVector2D(400.f, 40.f), 22, LF::UI::Cream);
	GraphicsRow->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverGraphics);
	GraphicsRow->OnClicked.AddDynamic(this, &ULFMenuWidget::OnGraphicsClicked);
	RefreshGraphics();
	OptionsBack = MakeButton(OptionsPage, TEXT("BACK"), FVector2D(480.f, 520.f + OY), FVector2D(320.f, 60.f));
	MusicRow->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverMusic);
	SfxRow->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverSfx);
	OptionsBack->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverBack);
	OptionsBack->OnClicked.AddDynamic(this, &ULFMenuWidget::OnBack);
	Text(OptionsPage, TEXT("Left / right to change  -  click Graphics to cycle"), FVector2D(330.f, 482.f + OY), FVector2D(600.f, 30.f), 14, LF::UI::Cream);

	// Credits page.
	CreditsPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->AddChild(CreditsPage);
	MenuPlace(CreditsPage, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
	LF::UI::Panel(WidgetTree, CreditsPage, FVector2D(500.f, 40.f), FVector2D(280.f, 56.f), LF::UI::Ribbon, 10.f, LF::UI::Cream, 3.f);
	LF::UI::Label(WidgetTree, CreditsPage, TEXT("CREDITS"), FVector2D(500.f, 44.f), FVector2D(280.f, 48.f), 32, LF::UI::Cream, true, 1);
	LF::UI::Panel(WidgetTree, CreditsPage, FVector2D(90.f, 120.f), FVector2D(1100.f, 490.f));
	UTextBlock* Credits = Text(CreditsPage, MenuCreditsText, FVector2D(124.f, 136.f), FVector2D(1032.f, 466.f), 15, LF::UI::Cream);
	Credits->SetAutoWrapText(true);
	CreditsBack = MakeButton(CreditsPage, TEXT("BACK"), FVector2D(480.f, 636.f), FVector2D(320.f, 60.f));
	CreditsBack->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverBack);
	CreditsBack->OnClicked.AddDynamic(this, &ULFMenuWidget::OnBack);

	// How-to-play page (pause only): goal paragraph + a three-column controls table, Back.
	HowToPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->AddChild(HowToPage);
	MenuPlace(HowToPage, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
	LF::UI::Panel(WidgetTree, HowToPage, FVector2D(190.f, 266.f), FVector2D(900.f, 322.f));
	UTextBlock* Goal = Text(HowToPage, HowToGoal, FVector2D(215.f, 276.f), FVector2D(850.f, 70.f), 14, LF::UI::Cream);
	Goal->SetAutoWrapText(true);
	LF::UI::Panel(WidgetTree, HowToPage, FVector2D(205.f, 352.f), FVector2D(870.f, 26.f), LF::UI::Ribbon, 8.f, LF::UI::WoodDark, 2.f);
	LF::UI::Label(WidgetTree, HowToPage, TEXT("ACTION"), FVector2D(215.f, 354.f), FVector2D(380.f, 22.f), 14, LF::UI::Cream, true);
	LF::UI::Label(WidgetTree, HowToPage, TEXT("KEYBOARD + MOUSE"), FVector2D(615.f, 354.f), FVector2D(230.f, 22.f), 14, LF::UI::Cream, true);
	LF::UI::Label(WidgetTree, HowToPage, TEXT("GAMEPAD"), FVector2D(855.f, 354.f), FVector2D(220.f, 22.f), 14, LF::UI::Cream, true);
	Text(HowToPage, HowToActions, FVector2D(215.f, 378.f), FVector2D(390.f, 236.f), 15, LF::UI::Cream);
	Text(HowToPage, HowToKeys, FVector2D(615.f, 378.f), FVector2D(230.f, 236.f), 15, LF::UI::Cream);
	Text(HowToPage, HowToPad, FVector2D(855.f, 378.f), FVector2D(220.f, 236.f), 15, LF::UI::Cream);
	HowToBack = MakeButton(HowToPage, TEXT("BACK"), FVector2D(500.f, 604.f), FVector2D(280.f, 52.f), 22); // clear of the HUD boat panel
	HowToBack->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverBack);
	HowToBack->OnClicked.AddDynamic(this, &ULFMenuWidget::OnBack);

	ShowPage(EPage::Main);
	ApplyVolumes();
	return Super::RebuildWidget();
}

void ULFMenuWidget::ShowPage(EPage NewPage)
{
	Page = NewPage;
	PageAge = 0.f;
	MainPage->SetVisibility(Page == EPage::Main ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	OptionsPage->SetVisibility(Page == EPage::Options ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	CreditsPage->SetVisibility(Page == EPage::Credits ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	HowToPage->SetVisibility(Page == EPage::HowTo ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (LogoImage) LogoImage->SetVisibility(Page == EPage::Credits ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	bSounds = false;
	Selection = 0;
	Select(0);
	bSounds = true;
	LF::Smoke(FString::Printf(TEXT("%s page %s"), bPauseMode ? TEXT("pause") : TEXT("menu"),
		Page == EPage::Main ? TEXT("main") : Page == EPage::Options ? TEXT("options") : Page == EPage::HowTo ? TEXT("how to play") : TEXT("credits")));
}

int32 ULFMenuWidget::ItemCount() const
{
	return Page == EPage::Main ? MainButtons.Num() : Page == EPage::Options ? 4 : 1;
}

void ULFMenuWidget::Select(int32 Index)
{
	const int32 Previous = Selection;
	Selection = (Index + ItemCount()) % ItemCount();
	if (bSounds && Selection != Previous) LF::UI::Sound(this, TEXT("MenuMove"));
	for (int32 I = 0; I < MainButtons.Num(); ++I) LF::UI::SetSelected(MainButtons[I], Page == EPage::Main && I == Selection);
	LF::UI::SetSelected(MusicRow, Page == EPage::Options && Selection == 0);
	LF::UI::SetSelected(SfxRow, Page == EPage::Options && Selection == 1);
	LF::UI::SetSelected(GraphicsRow, Page == EPage::Options && Selection == 2);
	LF::UI::SetSelected(OptionsBack, Page == EPage::Options && Selection == 3);
	LF::UI::SetSelected(CreditsBack, Page == EPage::Credits);
	LF::UI::SetSelected(HowToBack, Page == EPage::HowTo);
}

void ULFMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Short fades: the whole menu over 0.25 s when it opens, each page over 0.15 s when it changes (real time; works paused).
	Age += InDeltaTime;
	PageAge += InDeltaTime;
	SetRenderOpacity(FMath::Clamp(Age / 0.25f, 0.f, 1.f));
	UCanvasPanel* Current = Page == EPage::Main ? MainPage.Get() : Page == EPage::Options ? OptionsPage.Get() : Page == EPage::Credits ? CreditsPage.Get() : HowToPage.Get();
	if (Current) Current->SetRenderOpacity(FMath::Clamp(PageAge / 0.15f, 0.f, 1.f));
}

void ULFMenuWidget::Activate()
{
	if (Page == EPage::Main)
	{
		ActivateMain(Selection);
	}
	else if (Page == EPage::Options && Selection < 2)
	{
		Select(Selection + 1);
	}
	else if (Page == EPage::Options && Selection == 2)
	{
		CycleGraphics(1);
	}
	else
	{
		Back();
	}
}

void ULFMenuWidget::ActivateMain(int32 Index)
{
	LF::UI::Sound(this, TEXT("MenuSelect"));
	ULFRoundSubsystem* Round = GetWorld() ? GetWorld()->GetSubsystem<ULFRoundSubsystem>() : nullptr;
	if (bPauseMode)
	{
		switch (Index)
		{
		case 0: if (Round) Round->ResumeFromPause(); break;
		case 1: ShowPage(EPage::HowTo); break;
		case 2: if (Round) { LF::Smoke(TEXT("pause -> Restart")); Round->Restart(); } break;
		case 3: ShowPage(EPage::Options); break;
		default: if (Round) Round->QuitToMenu(); break;
		}
		return;
	}
	switch (Index)
	{
	case 0: if (Round) Round->StartFromMenu(); break;
	case 1: ShowPage(EPage::Options); break;
	case 2: ShowPage(EPage::Credits); break;
	default: if (Round) Round->Quit(); break;
	}
}

void ULFMenuWidget::Back()
{
	if (Page != EPage::Main) LF::UI::Sound(this, TEXT("Deny"));
	if (Page != EPage::Main) ShowPage(EPage::Main);
	else if (bPauseMode) ActivateMain(0); // Esc / Start / B on the pause menu's main page resumes
}

void ULFMenuWidget::Nudge(float Delta)
{
	if (Page != EPage::Options || Selection > 2) return;
	if (Selection == 2)
	{
		CycleGraphics(Delta > 0.f ? 1 : -1);
		return;
	}
	USlider* Slider = Selection == 0 ? MusicSlider.Get() : SfxSlider.Get();
	Slider->SetValue(FMath::Clamp(Slider->GetValue() + Delta, 0.f, 1.f));
	ApplyVolumes();
}

int32 ULFMenuWidget::GetGraphicsQuality()
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const int32 Level = Settings ? Settings->GetOverallScalabilityLevel() : 3; // -1 = mixed groups -> treat as High
	return Level < 0 ? 2 : FMath::Clamp(Level - 1, 0, 2);
}

void ULFMenuWidget::SetGraphicsQuality(int32 Quality)
{
	Quality = FMath::Clamp(Quality, 0, 2);
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(Quality + 1);
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}
	static const TCHAR* Names[] = { TEXT("Low"), TEXT("Medium"), TEXT("High") };
	UE_LOG(LogLF, Display, TEXT("[options] graphics %s (scalability %d)"), Names[Quality], Quality + 1);
	LF::Smoke(FString::Printf(TEXT("graphics quality %s (scalability %d)"), Names[Quality], Quality + 1));
}

void ULFMenuWidget::CycleGraphics(int32 Step)
{
	LF::UI::Sound(this, TEXT("MenuMove"));
	SetGraphicsQuality((GetGraphicsQuality() + Step + 3) % 3);
	RefreshGraphics();
}

void ULFMenuWidget::RefreshGraphics()
{
	static const TCHAR* Labels[] = { TEXT("<  LOW  >"), TEXT("<  MEDIUM  >"), TEXT("<  HIGH  >") };
	GraphicsValue->SetText(FText::FromString(Labels[GetGraphicsQuality()]));
}

void ULFMenuWidget::ApplyVolumes()
{
	const float Music = MusicSlider->GetValue();
	const float Sfx = SfxSlider->GetValue();
	if (ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(this))
	{
		Audio->SetCategoryVolume(ELFSoundCategory::Music, Music);
		// "Sound FX" = every non-music category the player hears: effects, voices, ambience and UI stings.
		for (ELFSoundCategory C : { ELFSoundCategory::SFX, ELFSoundCategory::Voice, ELFSoundCategory::Ambience, ELFSoundCategory::UI })
		{
			Audio->SetCategoryVolume(C, Sfx);
		}
	}
	MusicValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Music * 100.f))));
	SfxValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Sfx * 100.f))));
	if (GConfig)
	{
		GConfig->SetFloat(MenuSection, TEXT("Music"), Music, GGameUserSettingsIni);
		GConfig->SetFloat(MenuSection, TEXT("SFX"), Sfx, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void ULFMenuWidget::LoadAudioSettings(UObject* WorldContext)
{
	float Music = 1.f, Sfx = 1.f;
	if (!GConfig || !GConfig->GetFloat(MenuSection, TEXT("Music"), Music, GGameUserSettingsIni)) return;
	GConfig->GetFloat(MenuSection, TEXT("SFX"), Sfx, GGameUserSettingsIni);
	if (ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(WorldContext))
	{
		Audio->SetCategoryVolume(ELFSoundCategory::Music, FMath::Clamp(Music, 0.f, 1.f));
		for (ELFSoundCategory C : { ELFSoundCategory::SFX, ELFSoundCategory::Voice, ELFSoundCategory::Ambience, ELFSoundCategory::UI })
		{
			Audio->SetCategoryVolume(C, FMath::Clamp(Sfx, 0.f, 1.f));
		}
	}
}

FReply ULFMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FName Key = InKeyEvent.GetKey().GetFName();
	const bool bUp = Key == TEXT("Up") || Key == TEXT("Gamepad_DPad_Up") || Key == TEXT("Gamepad_LeftStick_Up") || Key == TEXT("W");
	const bool bDown = Key == TEXT("Down") || Key == TEXT("Gamepad_DPad_Down") || Key == TEXT("Gamepad_LeftStick_Down") || Key == TEXT("S") || Key == TEXT("Tab");
	const bool bLeft = Key == TEXT("Left") || Key == TEXT("Gamepad_DPad_Left") || Key == TEXT("Gamepad_LeftStick_Left") || Key == TEXT("A");
	const bool bRight = Key == TEXT("Right") || Key == TEXT("Gamepad_DPad_Right") || Key == TEXT("Gamepad_LeftStick_Right") || Key == TEXT("D");
	if (bUp) { Select(Selection - 1); return FReply::Handled(); }
	if (bDown) { Select(Selection + 1); return FReply::Handled(); }
	if (bLeft || bRight) { Nudge(bRight ? 0.05f : -0.05f); return FReply::Handled(); }
	if (Key == TEXT("Enter") || Key == TEXT("SpaceBar") || Key == TEXT("Gamepad_FaceButton_Bottom")) { Activate(); return FReply::Handled(); }
	if (Key == TEXT("Escape") || Key == TEXT("BackSpace") || Key == TEXT("Gamepad_FaceButton_Right")) { Back(); return FReply::Handled(); }
	if (bPauseMode && (Key == TEXT("Gamepad_Special_Right") || Key == TEXT("P")))
	{
		// The pause keys close the pause menu from any page.
		ActivateMain(0);
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void ULFMenuWidget::OnMain0() { ActivateMain(0); }
void ULFMenuWidget::OnMain1() { ActivateMain(1); }
void ULFMenuWidget::OnMain2() { ActivateMain(2); }
void ULFMenuWidget::OnMain3() { ActivateMain(3); }
void ULFMenuWidget::OnMain4() { ActivateMain(4); }
void ULFMenuWidget::OnHoverMain4() { if (Page == EPage::Main) Select(4); }
void ULFMenuWidget::OnBack() { Back(); }
void ULFMenuWidget::OnSliderChanged(float) { ApplyVolumes(); }
void ULFMenuWidget::OnHoverPlay() { if (Page == EPage::Main) Select(0); }
void ULFMenuWidget::OnHoverOptions() { if (Page == EPage::Main) Select(1); }
void ULFMenuWidget::OnHoverCredits() { if (Page == EPage::Main) Select(2); }
void ULFMenuWidget::OnHoverQuit() { if (Page == EPage::Main) Select(3); }
void ULFMenuWidget::OnHoverMusic() { if (Page == EPage::Options) Select(0); }
void ULFMenuWidget::OnHoverSfx() { if (Page == EPage::Options) Select(1); }
void ULFMenuWidget::OnHoverGraphics() { if (Page == EPage::Options) Select(2); }
void ULFMenuWidget::OnGraphicsClicked() { Select(2); CycleGraphics(1); }
void ULFMenuWidget::OnHoverBack() { if (Page == EPage::Options) Select(3); else if (Page != EPage::Main) Select(0); }
