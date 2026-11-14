#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LFBoatFishLabels.generated.h"

class ALFBoat;
class UCanvasPanel;
class UTextBlock;
class UImage;
class UTexture2D;

/** Projected gameplay cargo labels; neither debug text nor off-screen indicators. */
UCLASS()
class LAKEFIGHT_API ULFBoatFishLabels : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	struct FLabel
	{
		TWeakObjectPtr<ALFBoat> Boat;
		TWeakObjectPtr<UCanvasPanel> Panel;
		TWeakObjectPtr<UTextBlock> Count;
		int32 Team = INDEX_NONE;
	};
	TArray<FLabel> Labels;
	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TObjectPtr<UTexture2D> FishIcon;
};
