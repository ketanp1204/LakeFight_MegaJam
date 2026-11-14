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

/** Native B08 HUD and results. Fixed design canvas scales to the viewport, including gamepad menus. */
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
	UPROPERTY() TObjectPtr<UImage> GoldIcon;
	UPROPERTY() TArray<TObjectPtr<UWidget>> GameplayWidgets;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> Scores;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> Icons;
	UPROPERTY() TObjectPtr<UTextBlock> Timer;
	UPROPERTY() TObjectPtr<UTextBlock> Cargo;
	UPROPERTY() TObjectPtr<UTextBlock> Banner;
	UPROPERTY() TObjectPtr<UTextBlock> Direction;
	UPROPERTY() TObjectPtr<UBorder> Results;
	UPROPERTY() TObjectPtr<UButton> RestartButton;
	UPROPERTY() TObjectPtr<UButton> QuitButton;
	bool bResults = false;
	int32 Selection = 0;

	UTextBlock* Text(UCanvasPanel* Parent, const FString& Value, FVector2D At, FVector2D Size, int32 FontSize, FLinearColor Colour);
	UImage* Icon(const TCHAR* Name, FVector2D At, float Size);
	void SelectButton(int32 Index);
	UFUNCTION() void Restart();
	UFUNCTION() void Quit();
};
