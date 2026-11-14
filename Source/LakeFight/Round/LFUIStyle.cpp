#include "Round/LFUIStyle.h"

#include "Audio/LFAudio.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"

namespace LF::UI
{
	// Sampled from the B12a lockup: sign wood, cream lettering/outline, red ribbon.
	const FLinearColor Wood = FLinearColor(FColor(126, 92, 64)).CopyWithNewOpacity(0.95f);
	const FLinearColor WoodDark = FLinearColor(FColor(58, 38, 24));
	const FLinearColor WoodLight = FLinearColor(FColor(156, 118, 84)).CopyWithNewOpacity(0.95f);
	const FLinearColor Cream = FLinearColor(FColor(255, 244, 214));
	const FLinearColor Ribbon = FLinearColor(FColor(212, 66, 58));
	const FLinearColor Gold = FLinearColor(FColor(255, 204, 52));
	const FLinearColor Ink = FLinearColor(FColor(30, 18, 10)).CopyWithNewOpacity(0.9f);
	const FLinearColor TeamColours[4] = { FLinearColor(FColor(232, 72, 60)), FLinearColor(FColor(64, 140, 236)),
		FLinearColor(FColor(70, 196, 104)), FLinearColor(FColor(250, 206, 50)) };

	FSlateBrush RoundBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Outline), OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	void Place(UWidget* Widget, FVector2D At, FVector2D Size)
	{
		UCanvasPanelSlot* Slot = CastChecked<UCanvasPanelSlot>(Widget->Slot);
		Slot->SetPosition(At);
		Slot->SetSize(Size);
	}

	UBorder* Panel(UWidgetTree* Tree, UCanvasPanel* Parent, FVector2D At, FVector2D Size, const FLinearColor& Fill, float Radius,
		const FLinearColor& Outline, float OutlineWidth)
	{
		// Soft shadow: two stacked translucent rounded boxes, offset down.
		for (int32 I = 0; I < 2; ++I)
		{
			UBorder* Shadow = Tree->ConstructWidget<UBorder>();
			Parent->AddChild(Shadow);
			const float Grow = I == 0 ? 4.f : 1.f;
			Place(Shadow, At + FVector2D(-Grow, 5.f - Grow), Size + FVector2D(2.f * Grow));
			Shadow->SetBrush(RoundBrush(FLinearColor(0.f, 0.f, 0.f, I == 0 ? 0.16f : 0.28f), Radius + Grow, FLinearColor::Transparent, 0.f));
			Shadow->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		UBorder* Box = Tree->ConstructWidget<UBorder>();
		Parent->AddChild(Box);
		Place(Box, At, Size);
		Box->SetBrush(RoundBrush(Fill, Radius, Outline, OutlineWidth));
		Box->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Box;
	}

	void StyleText(UTextBlock* Text, int32 FontSize, const FLinearColor& Colour, bool bBold)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		Font.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(FontSize / 11.f));
		Font.OutlineSettings.OutlineColor = Ink;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Colour);
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Text->SetShadowOffset(FVector2D(0.f, 2.f));
	}

	UTextBlock* Label(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Value, FVector2D At, FVector2D Size, int32 FontSize,
		const FLinearColor& Colour, bool bBold, int32 Justify)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Parent->AddChild(Text);
		Place(Text, At, Size);
		Text->SetText(FText::FromString(Value));
		StyleText(Text, FontSize, Colour, bBold);
		Text->SetJustification(Justify == 1 ? ETextJustify::Center : Justify == 2 ? ETextJustify::Right : ETextJustify::Left);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	}

	static FButtonStyle MakeStyle(bool bSelected)
	{
		FButtonStyle Style;
		const FSlateBrush Idle = RoundBrush(Wood.CopyWithNewOpacity(1.f), 12.f, WoodDark, 3.f);
		const FSlateBrush Hot = RoundBrush(Ribbon, 12.f, Cream, 3.f);
		const FSlateBrush Down = RoundBrush(Ribbon * 0.8f, 12.f, Cream, 3.f);
		Style.SetNormal(bSelected ? Hot : Idle);
		Style.SetHovered(Hot);
		Style.SetPressed(Down);
		Style.SetDisabled(Idle);
		Style.SetNormalPadding(FMargin(8.f, 2.f));
		Style.SetPressedPadding(FMargin(8.f, 4.f, 8.f, 0.f));
		return Style;
	}

	UButton* Button(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Text, FVector2D At, FVector2D Size, int32 FontSize)
	{
		UButton* Result = Tree->ConstructWidget<UButton>();
		Parent->AddChild(Result);
		Place(Result, At, Size);
		Result->SetStyle(MakeStyle(false));
		Result->SetBackgroundColor(FLinearColor::White);
		UTextBlock* Caption = Tree->ConstructWidget<UTextBlock>();
		Caption->SetText(FText::FromString(Text));
		StyleText(Caption, FontSize, Cream, true);
		Result->AddChild(Caption);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Caption->Slot))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Result;
	}

	void SetSelected(UButton* Target, bool bSelected)
	{
		static const FButtonStyle IdleStyle = MakeStyle(false);
		static const FButtonStyle HotStyle = MakeStyle(true);
		if (Target) Target->SetStyle(bSelected ? HotStyle : IdleStyle);
	}

	void Sound(const UObject* WorldContext, const TCHAR* Name)
	{
		static TMap<FString, TWeakObjectPtr<USoundWave>> Cache;
		TWeakObjectPtr<USoundWave>& Entry = Cache.FindOrAdd(Name);
		if (!Entry.IsValid())
		{
			USoundWave* Loaded = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/LakeFight/Audio/UI_LF_%s.UI_LF_%s"), Name, Name));
			if (Loaded) Loaded->AddToRoot(); // a handful of tiny stings, kept for the session
			Entry = Loaded;
		}
		if (!Entry.IsValid() || !WorldContext) return;
		const ULFMusicSubsystem* Music = ULFMusicSubsystem::Get(WorldContext);
		UGameplayStatics::PlaySound2D(WorldContext, Entry.Get(), Music ? Music->CategoryGain(ELFSoundCategory::UI) : 1.f);
	}

	UTexture2D* LoadPng(UObject* Outer, const FString& RelativeEditorPath, const FString& FileName)
	{
		FString Path = FPaths::ProjectDir() / RelativeEditorPath / FileName;
		if (!FPaths::FileExists(Path)) Path = FString(FPlatformProcess::BaseDir()) / TEXT("LFUI") / FileName;
		return UKismetRenderingLibrary::ImportFileAsTexture2D(Outer, Path);
	}
}
