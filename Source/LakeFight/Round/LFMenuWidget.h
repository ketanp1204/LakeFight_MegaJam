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
	/** Dev evidence (lf.menu.page): 0 main, 1 options, 2 credits. */
	void DebugShowPage(int32 Index) { ShowPage(Index == 1 ? EPage::Options : Index == 2 ? EPage::Credits : EPage::Main); }

private:
	enum class EPage : uint8 { Main, Options, Credits };
	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TObjectPtr<UCanvasPanel> MainPage;
	UPROPERTY() TObjectPtr<UCanvasPanel> OptionsPage;
	UPROPERTY() TObjectPtr<UCanvasPanel> CreditsPage;
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

	EPage Page = EPage::Main;
	int32 Selection = 0;

	UTextBlock* Text(UCanvasPanel* Parent, const FString& Value, FVector2D At, FVector2D Size, int32 FontSize, FLinearColor Colour);
	UButton* MakeButton(UCanvasPanel* Parent, const FString& Label, FVector2D At, FVector2D Size, int32 FontSize = 24);
	USlider* MakeSlider(UCanvasPanel* Parent, FVector2D At, FVector2D Size, float Value);
	void ShowPage(EPage NewPage);
	int32 ItemCount() const;
	void Select(int32 Index);
	void Activate();
	void Back();
	void Nudge(float Delta);
	void ApplyVolumes();

	UFUNCTION() void OnPlay();
	UFUNCTION() void OnOptions();
	UFUNCTION() void OnCredits();
	UFUNCTION() void OnQuit();
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
