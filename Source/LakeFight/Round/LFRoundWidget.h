#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LFRoundWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UButton;
class UBorder;
class UTexture2D;
class UImage;

/**
 * Native HUD and results card (B08 icons). N2 item 15 restyle (LF::UI): team score chips, timer pill (red from 0:30),
 * "your boat" panel, gold banner + gold arrow pill, round-start hints, banking pops (+N flies from your boat panel to your
 * team's chip; other teams pop at their chip), styled results card. Fixed 1280x720 design canvas scaled to the viewport.
 */
UCLASS()
class LAKEFIGHT_API ULFRoundWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	void ShowResults();
private:
	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TObjectPtr<UCanvasPanel> HudGroup;
	UPROPERTY() TObjectPtr<UCanvasPanel> GoldBannerGroup;
	UPROPERTY() TObjectPtr<UCanvasPanel> GoldArrowGroup;
	UPROPERTY() TObjectPtr<UCanvasPanel> HintGroup;
	UPROPERTY() TObjectPtr<UCanvasPanel> ResultsGroup;
	UPROPERTY() TMap<FString, TObjectPtr<UTexture2D>> Icons;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Scores;
	UPROPERTY() TObjectPtr<UBorder> TimerPill;
	UPROPERTY() TObjectPtr<UTextBlock> Timer;
	UPROPERTY() TObjectPtr<UTextBlock> Cargo;
	UPROPERTY() TObjectPtr<UTextBlock> Direction;
	UPROPERTY() TObjectPtr<UTextBlock> HintText;
	UPROPERTY() TObjectPtr<UButton> RestartButton;
	UPROPERTY() TObjectPtr<UButton> QuitButton;
	// Banking pops (parallel arrays; a handful at most).
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> PopTexts;
	TArray<FVector2D> PopFrom;
	TArray<FVector2D> PopTo;
	TArray<float> PopAge;
	TArray<float> PopDuration;
	TArray<int32> PopTeam;
	int32 LastScores[4] = { 0, 0, 0, 0 };
	float ChipBump[4] = { 0.f, 0.f, 0.f, 0.f };
	bool bScoresKnown = false;
	bool bTimerRed = false;
	bool bResults = false;
	float ResultsAge = 0.f;
	int32 Selection = 0;
	int32 HintShown = -1;

	UImage* Icon(UCanvasPanel* Parent, const FString& Name, FVector2D At, float Size);
	FVector2D ChipScoreCentre(int32 Team) const;
	int32 PlayerTeam() const;
	void AddPop(int32 Team, int32 Amount, bool bFromBoat);
	void TickPops(float DeltaTime);
	void SelectButton(int32 Index);
	UFUNCTION() void Restart();
	UFUNCTION() void Quit();
	UFUNCTION() void OnHoverRestart();
	UFUNCTION() void OnHoverQuit();
};
