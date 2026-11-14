#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class UWidgetTree;
class UCanvasPanel;
class UBorder;
class UButton;
class UTextBlock;
class UWidget;

/**
 * N2 item 15: one UI style for HUD, menu, pause and results, matching the B12a logo: wood-brown rounded panels with a dark
 * outline and a soft drop shadow, cream outlined text (engine Roboto only - Bold for numbers/titles), red-ribbon accents for
 * selection / alerts, team colours only on team elements. All layouts use a 1280x720 design canvas inside a ScaleBox
 * (ScaleToFit), i.e. the 1920x1080 reference at 2/3 scale - every size scales uniformly to 720p / 1080p / 1440p.
 */
namespace LF::UI
{
	LAKEFIGHT_API extern const FLinearColor Wood;      // panel fill
	LAKEFIGHT_API extern const FLinearColor WoodDark;  // panel outline, button text shadow
	LAKEFIGHT_API extern const FLinearColor WoodLight; // inner rows
	LAKEFIGHT_API extern const FLinearColor Cream;     // text, selected outline
	LAKEFIGHT_API extern const FLinearColor Ribbon;    // selection, alerts, timer < 0:30
	LAKEFIGHT_API extern const FLinearColor Gold;      // gold fish only
	LAKEFIGHT_API extern const FLinearColor Ink;       // text outline
	LAKEFIGHT_API extern const FLinearColor TeamColours[4];

	/** Rounded box brush with an outline. */
	LAKEFIGHT_API FSlateBrush RoundBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth);
	/** Places a widget on a canvas slot (adds it first if Parent is set). */
	LAKEFIGHT_API void Place(UWidget* Widget, FVector2D At, FVector2D Size);
	/** Soft shadow + rounded outlined panel on Parent. Returns the panel; the shadow is added just before it. */
	LAKEFIGHT_API UBorder* Panel(UWidgetTree* Tree, UCanvasPanel* Parent, FVector2D At, FVector2D Size, const FLinearColor& Fill = Wood,
		float Radius = 14.f, const FLinearColor& Outline = WoodDark, float OutlineWidth = 3.f);
	/** Cream text with a dark outline + shadow so it reads over bright water. */
	LAKEFIGHT_API UTextBlock* Label(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Value, FVector2D At, FVector2D Size, int32 FontSize,
		const FLinearColor& Colour = Cream, bool bBold = true, int32 Justify = 0 /*0 left, 1 centre, 2 right*/);
	/** Restyles a text block (font size, typeface, outline). */
	LAKEFIGHT_API void StyleText(UTextBlock* Text, int32 FontSize, const FLinearColor& Colour, bool bBold = true);
	/** Menu button: wood when idle, red ribbon with a cream outline when selected (hover, keyboard or gamepad focus). */
	LAKEFIGHT_API UButton* Button(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Text, FVector2D At, FVector2D Size, int32 FontSize = 24);
	LAKEFIGHT_API void SetSelected(UButton* Button, bool bSelected);
	/** A01 UI sounds (UI category volume, audible while paused): MenuMove, MenuSelect, Deny, Win, RoundStart... */
	LAKEFIGHT_API void Sound(const UObject* WorldContext, const TCHAR* Name);
	/** UI PNG next to the editor project (Art/Export/...) or the packaged exe (LFUI/). */
	LAKEFIGHT_API class UTexture2D* LoadPng(UObject* Outer, const FString& RelativeEditorPath, const FString& FileName);
}
