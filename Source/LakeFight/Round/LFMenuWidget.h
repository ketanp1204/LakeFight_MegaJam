#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LFMenuWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UButton;
class USlider;
class UTexture2D;

/**
 * N1 item 5: main menu over the paused lake - logo, Play, Options (music / SFX volume through LFAudio), Credits, Quit.
 * Native UMG on a fixed 1280x720 design canvas (like ULFRoundWidget). Mouse (click / hover / drag sliders), keyboard
 * (arrows, Enter, Esc) and gamepad (D-pad / left stick, A, B) all drive the same selection.
 * N2 item 12: the same widget in pause mode (SetPauseMode before AddToViewport) is the in-round pause menu - Resume,
 * Restart, Options (shared page), Quit to menu; dimmed backdrop, "PAUSED" title; Esc / Start / B on its main page resumes.
 */
UCLASS()
class LAKEFIGHT_API ULFMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Saved volumes (GameUserSettings [LakeFight.Audio]) -> LFAudio. Called once at startup by the round subsystem. */
	static void LoadAudioSettings(UObject* WorldContext);
	/** Dev evidence (lf.menu.page): 0 main, 1 options, 2 credits (menu only), 3 how to play (pause only). */
	void DebugShowPage(int32 Index)
	{
		ShowPage(Index == 1 ? EPage::Options : Index == 2 && !bPauseMode ? EPage::Credits : Index == 3 && bPauseMode ? EPage::HowTo : EPage::Main);
	}
	void SetPauseMode(bool bPause) { bPauseMode = bPause; }
	/** Dev evidence (lf.menu.item): press main-page item Index (pause: 0 resume, 1 restart, 2 options, 3 quit to menu). */
	void DebugActivate(int32 Index) { ActivateMain(Index); }
	bool IsPauseMode() const { return bPauseMode; }

private:
	enum class EPage : uint8 { Main, Options, Credits, HowTo };
	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TObjectPtr<UCanvasPanel> MainPage;
	UPROPERTY() TObjectPtr<UCanvasPanel> OptionsPage;
	UPROPERTY() TObjectPtr<UCanvasPanel> CreditsPage;
	UPROPERTY() TObjectPtr<UCanvasPanel> HowToPage; // N2 item 13 (pause only): goal + controls card
	UPROPERTY() TObjectPtr<UButton> HowToBack;
	UPROPERTY() TArray<TObjectPtr<UButton>> MainButtons;
	UPROPERTY() TObjectPtr<USlider> MusicSlider;
	UPROPERTY() TObjectPtr<USlider> SfxSlider;
	UPROPERTY() TObjectPtr<UButton> MusicRow;
	UPROPERTY() TObjectPtr<UButton> SfxRow;
	UPROPERTY() TObjectPtr<UButton> OptionsBack;
	UPROPERTY() TObjectPtr<UButton> CreditsBack;
	UPROPERTY() TObjectPtr<UTextBlock> MusicValue;
	UPROPERTY() TObjectPtr<UTextBlock> SfxValue;
	UPROPERTY() TObjectPtr<UTexture2D> LogoTexture;
	UPROPERTY() TObjectPtr<class UImage> LogoImage;

	bool bPauseMode = false;
	EPage Page = EPage::Main;
	int32 Selection = 0;

	UTextBlock* Text(UCanvasPanel* Parent, const FString& Value, FVector2D At, FVector2D Size, int32 FontSize, FLinearColor Colour);
	UButton* MakeButton(UCanvasPanel* Parent, const FString& Label, FVector2D At, FVector2D Size, int32 FontSize = 24);
	USlider* MakeSlider(UCanvasPanel* Parent, FVector2D At, FVector2D Size, float Value);
	void ShowPage(EPage NewPage);
	int32 ItemCount() const;
	void Select(int32 Index);
	void Activate();
	void ActivateMain(int32 Index);
	void Back();
	void Nudge(float Delta);
	void ApplyVolumes();

	UFUNCTION() void OnMain0();
	UFUNCTION() void OnMain1();
	UFUNCTION() void OnMain2();
	UFUNCTION() void OnMain3();
	UFUNCTION() void OnMain4();
	UFUNCTION() void OnHoverMain4();
	UFUNCTION() void OnBack();
	UFUNCTION() void OnSliderChanged(float Value);
	UFUNCTION() void OnHoverPlay();
	UFUNCTION() void OnHoverOptions();
	UFUNCTION() void OnHoverCredits();
	UFUNCTION() void OnHoverQuit();
	UFUNCTION() void OnHoverMusic();
	UFUNCTION() void OnHoverSfx();
	UFUNCTION() void OnHoverBack();
};
