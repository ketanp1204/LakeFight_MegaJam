#include "Round/LFMenuWidget.h"

#include "Round/LFRoundSubsystem.h"
#include "Audio/LFAudio.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
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
#endif

namespace
{
	const FLinearColor MenuCream(1.f, .95f, .8f, 1.f);
	const FLinearColor MenuGold(1.f, .85f, .25f, 1.f);
	const FLinearColor MenuInk(.02f, .04f, .05f, 1.f);
	const FLinearColor MenuPanel(.018f, .035f, .045f, .9f);
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
}

UTextBlock* ULFMenuWidget::Text(UCanvasPanel* Parent, const FString& Value, FVector2D At, FVector2D Size, int32 FontSize, FLinearColor Colour)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Parent->AddChild(Label);
	MenuPlace(Label, At, Size);
	Label->SetText(FText::FromString(Value));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = FontSize;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(Colour);
	Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Label->SetShadowOffset(FVector2D(1.f, 2.f));
	return Label;
}

UButton* ULFMenuWidget::MakeButton(UCanvasPanel* Parent, const FString& Label, FVector2D At, FVector2D Size, int32 FontSize)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Parent->AddChild(Button);
	MenuPlace(Button, At, Size);
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetText(FText::FromString(Label));
	Text->SetColorAndOpacity(MenuInk);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	Button->AddChild(Text);
	return Button;
}

USlider* ULFMenuWidget::MakeSlider(UCanvasPanel* Parent, FVector2D At, FVector2D Size, float Value)
{
	USlider* Slider = WidgetTree->ConstructWidget<USlider>();
	Parent->AddChild(Slider);
	MenuPlace(Slider, At, Size);
	Slider->SetValue(Value);
	Slider->SetStepSize(0.05f);
	Slider->SetSliderBarColor(MenuCream);
	Slider->SetSliderHandleColor(MenuGold);
	Slider->OnValueChanged.AddDynamic(this, &ULFMenuWidget::OnSliderChanged);
	return Slider;
}

TSharedRef<SWidget> ULFMenuWidget::RebuildWidget()
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

	// Logo: B12a title lockup PNG (editor: Art/Export/KeyArt, packaged: LFUI next to the executable - see LakeFight.Build.cs).
	FString LogoPath = FPaths::ProjectDir() / TEXT("Art/Export/KeyArt/LF_TitleLockup_960x300_Light.png");
	if (!FPaths::FileExists(LogoPath)) LogoPath = FString(FPlatformProcess::BaseDir()) / TEXT("LFUI/LF_TitleLockup_960x300_Light.png");
	LogoTexture = UKismetRenderingLibrary::ImportFileAsTexture2D(this, LogoPath);
	UImage* Logo = WidgetTree->ConstructWidget<UImage>();
	LogoImage = Logo;
	Canvas->AddChild(Logo);
	// The lettering fills roughly the left 45% of the 960x300 lockup canvas: offset so it sits centred above the buttons.
	MenuPlace(Logo, FVector2D(395.f, -10.f), FVector2D(1000.f, 312.5f));
	if (LogoTexture) Logo->SetBrushFromTexture(LogoTexture);
	else Text(Canvas, TEXT("LAKE FIGHT"), FVector2D(380.f, 90.f), FVector2D(520.f, 90.f), 56, MenuCream);

	// Main page.
	MainPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->AddChild(MainPage);
	MenuPlace(MainPage, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
	const TCHAR* Labels[] = { TEXT("PLAY"), TEXT("OPTIONS"), TEXT("CREDITS"), TEXT("QUIT") };
	for (int32 I = 0; I < 4; ++I)
	{
		MainButtons.Add(MakeButton(MainPage, Labels[I], FVector2D(480.f, 290.f + I * 82.f), FVector2D(320.f, 64.f)));
	}
	MainButtons[0]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnPlay);
	MainButtons[1]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnOptions);
	MainButtons[2]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnCredits);
	MainButtons[3]->OnClicked.AddDynamic(this, &ULFMenuWidget::OnQuit);
	MainButtons[0]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverPlay);
	MainButtons[1]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverOptions);
	MainButtons[2]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverCredits);
	MainButtons[3]->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverQuit);
	Text(MainPage, TEXT("Mouse / arrows / D-pad to choose  -  Enter / A to confirm  -  Esc / B back"), FVector2D(340.f, 640.f), FVector2D(700.f, 30.f), 14, MenuCream);

	// Options page: two rows (label + slider), Back.
	OptionsPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->AddChild(OptionsPage);
	MenuPlace(OptionsPage, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
	UBorder* OptionsCard = WidgetTree->ConstructWidget<UBorder>();
	OptionsPage->AddChild(OptionsCard);
	MenuPlace(OptionsCard, FVector2D(300.f, 270.f), FVector2D(680.f, 330.f));
	OptionsCard->SetBrushColor(MenuPanel);
	const ULFAudioSubsystem* Audio = ULFAudioSubsystem::Get(this);
	const float Music = Audio ? Audio->GetCategoryVolume(ELFSoundCategory::Music) : 1.f;
	const float Sfx = Audio ? Audio->GetCategoryVolume(ELFSoundCategory::SFX) : 1.f;
	MusicRow = MakeButton(OptionsPage, TEXT("MUSIC"), FVector2D(330.f, 300.f), FVector2D(200.f, 60.f), 22);
	SfxRow = MakeButton(OptionsPage, TEXT("SOUND FX"), FVector2D(330.f, 380.f), FVector2D(200.f, 60.f), 22);
	MusicSlider = MakeSlider(OptionsPage, FVector2D(560.f, 310.f), FVector2D(300.f, 40.f), FMath::Clamp(Music, 0.f, 1.f));
	SfxSlider = MakeSlider(OptionsPage, FVector2D(560.f, 390.f), FVector2D(300.f, 40.f), FMath::Clamp(Sfx, 0.f, 1.f));
	MusicValue = Text(OptionsPage, TEXT(""), FVector2D(880.f, 312.f), FVector2D(90.f, 40.f), 20, MenuCream);
	SfxValue = Text(OptionsPage, TEXT(""), FVector2D(880.f, 392.f), FVector2D(90.f, 40.f), 20, MenuCream);
	OptionsBack = MakeButton(OptionsPage, TEXT("BACK"), FVector2D(480.f, 500.f), FVector2D(320.f, 64.f));
	MusicRow->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverMusic);
	SfxRow->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverSfx);
	OptionsBack->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverBack);
	OptionsBack->OnClicked.AddDynamic(this, &ULFMenuWidget::OnBack);
	Text(OptionsPage, TEXT("Left / right to change"), FVector2D(330.f, 450.f), FVector2D(600.f, 30.f), 14, MenuCream);

	// Credits page.
	CreditsPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->AddChild(CreditsPage);
	MenuPlace(CreditsPage, FVector2D::ZeroVector, FVector2D(1280.f, 720.f));
	UBorder* CreditsCard = WidgetTree->ConstructWidget<UBorder>();
	CreditsPage->AddChild(CreditsCard);
	MenuPlace(CreditsCard, FVector2D(90.f, 200.f), FVector2D(1100.f, 420.f));
	CreditsCard->SetBrushColor(MenuPanel);
	UTextBlock* Credits = Text(CreditsPage, MenuCreditsText, FVector2D(120.f, 214.f), FVector2D(1040.f, 400.f), 13, MenuCream);
	Credits->SetAutoWrapText(true);
	CreditsBack = MakeButton(CreditsPage, TEXT("BACK"), FVector2D(480.f, 636.f), FVector2D(320.f, 60.f));
	CreditsBack->OnHovered.AddDynamic(this, &ULFMenuWidget::OnHoverBack);
	CreditsBack->OnClicked.AddDynamic(this, &ULFMenuWidget::OnBack);

	ShowPage(EPage::Main);
	ApplyVolumes();
	return Super::RebuildWidget();
}

void ULFMenuWidget::ShowPage(EPage NewPage)
{
	Page = NewPage;
	MainPage->SetVisibility(Page == EPage::Main ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	OptionsPage->SetVisibility(Page == EPage::Options ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	CreditsPage->SetVisibility(Page == EPage::Credits ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (LogoImage) LogoImage->SetVisibility(Page == EPage::Credits ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Select(0);
	LF::Smoke(FString::Printf(TEXT("menu page %s"), Page == EPage::Main ? TEXT("main") : Page == EPage::Options ? TEXT("options") : TEXT("credits")));
}

int32 ULFMenuWidget::ItemCount() const
{
	return Page == EPage::Main ? 4 : Page == EPage::Options ? 3 : 1;
}

void ULFMenuWidget::Select(int32 Index)
{
	Selection = (Index + ItemCount()) % ItemCount();
	for (int32 I = 0; I < MainButtons.Num(); ++I) MainButtons[I]->SetBackgroundColor(Page == EPage::Main && I == Selection ? MenuGold : MenuCream);
	MusicRow->SetBackgroundColor(Page == EPage::Options && Selection == 0 ? MenuGold : MenuCream);
	SfxRow->SetBackgroundColor(Page == EPage::Options && Selection == 1 ? MenuGold : MenuCream);
	OptionsBack->SetBackgroundColor(Page == EPage::Options && Selection == 2 ? MenuGold : MenuCream);
	CreditsBack->SetBackgroundColor(Page == EPage::Credits ? MenuGold : MenuCream);
}

void ULFMenuWidget::Activate()
{
	if (Page == EPage::Main)
	{
		switch (Selection)
		{
		case 0: OnPlay(); break;
		case 1: OnOptions(); break;
		case 2: OnCredits(); break;
		default: OnQuit(); break;
		}
	}
	else if (Page == EPage::Options && Selection < 2)
	{
		Select(Selection + 1);
	}
	else
	{
		Back();
	}
}

void ULFMenuWidget::Back()
{
	if (Page != EPage::Main) ShowPage(EPage::Main);
}

void ULFMenuWidget::Nudge(float Delta)
{
	if (Page != EPage::Options || Selection > 1) return;
	USlider* Slider = Selection == 0 ? MusicSlider.Get() : SfxSlider.Get();
	Slider->SetValue(FMath::Clamp(Slider->GetValue() + Delta, 0.f, 1.f));
	ApplyVolumes();
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
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void ULFMenuWidget::OnPlay()
{
	if (ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>()) Round->StartFromMenu();
}
void ULFMenuWidget::OnOptions() { ShowPage(EPage::Options); }
void ULFMenuWidget::OnCredits() { ShowPage(EPage::Credits); }
void ULFMenuWidget::OnQuit()
{
	if (ULFRoundSubsystem* Round = GetWorld()->GetSubsystem<ULFRoundSubsystem>()) Round->Quit();
}
void ULFMenuWidget::OnBack() { Back(); }
void ULFMenuWidget::OnSliderChanged(float) { ApplyVolumes(); }
void ULFMenuWidget::OnHoverPlay() { if (Page == EPage::Main) Select(0); }
void ULFMenuWidget::OnHoverOptions() { if (Page == EPage::Main) Select(1); }
void ULFMenuWidget::OnHoverCredits() { if (Page == EPage::Main) Select(2); }
void ULFMenuWidget::OnHoverQuit() { if (Page == EPage::Main) Select(3); }
void ULFMenuWidget::OnHoverMusic() { if (Page == EPage::Options) Select(0); }
void ULFMenuWidget::OnHoverSfx() { if (Page == EPage::Options) Select(1); }
void ULFMenuWidget::OnHoverBack() { if (Page == EPage::Options) Select(2); else if (Page == EPage::Credits) Select(0); }
