#include "Fish/LFBoatFishLabels.h"

#include "Fish/LFFishRuntime.h"
#include "Fish/LFFishCargoComponent.h"
#include "Brawler/LFBrawler.h"
#include "Boat/LFBoat.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/Paths.h"

TSharedRef<SWidget> ULFBoatFishLabels::RebuildWidget()
{
	if (!Canvas)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Canvas;
		SetVisibility(ESlateVisibility::HitTestInvisible);
		FString Path = FPaths::ProjectDir() / TEXT("Art/Export/UI/ICON_LF_Fish.png");
		if (!FPaths::FileExists(Path)) Path = FString(FPlatformProcess::BaseDir()) / TEXT("LFUI/ICON_LF_Fish.png");
		FishIcon = UKismetRenderingLibrary::ImportFileAsTexture2D(this, Path);
	}
	return Super::RebuildWidget();
}

void ULFBoatFishLabels::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !Canvas) return;
	const FLinearColor Colours[] = { {1.f,.3f,.25f,1.f}, {.3f,.65f,1.f,1.f}, {.35f,.9f,.55f,1.f}, {1.f,.85f,.25f,1.f} };
	static_assert(UE_ARRAY_COUNT(Colours) == int32(ELFTeam::Yellow) + 1);
	for (int32 I = Labels.Num() - 1; I >= 0; --I)
	{
		if (!Labels[I].Boat.IsValid())
		{
			if (Labels[I].Panel.IsValid()) Labels[I].Panel->RemoveFromParent();
			Labels.RemoveAt(I);
		}
	}
	for (const FLFFishBoat& Entry : GetWorld()->GetSubsystem<ULFFishBoatCache>()->GetBoats())
	{
		ALFBoat* Boat = Entry.Boat.Get();
		if (!Boat || !Entry.Cargo.IsValid()) continue;
		FLabel* Label = Labels.FindByPredicate([Boat](const FLabel& L) { return L.Boat.Get() == Boat; });
		if (!Label)
		{
			FLabel New;
			New.Boat = Boat;
			UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>();
			Canvas->AddChild(Panel);
			CastChecked<UCanvasPanelSlot>(Panel->Slot)->SetSize(FVector2D(130.f, 48.f));
			New.Panel = Panel;
			UImage* Image = WidgetTree->ConstructWidget<UImage>();
			Panel->AddChild(Image);
			CastChecked<UCanvasPanelSlot>(Image->Slot)->SetSize(FVector2D(48.f));
			Image->SetBrushFromTexture(FishIcon);
			UTextBlock* Count = WidgetTree->ConstructWidget<UTextBlock>();
			Panel->AddChild(Count);
			UCanvasPanelSlot* CountSlot = CastChecked<UCanvasPanelSlot>(Count->Slot);
			CountSlot->SetPosition(FVector2D(51.f, 4.f)); CountSlot->SetSize(FVector2D(79.f, 44.f));
			FSlateFontInfo Font = Count->GetFont(); Font.Size = 28; Font.OutlineSettings.OutlineSize = 2;
			Count->SetFont(Font);
			Count->SetShadowColorAndOpacity(FLinearColor::Black); Count->SetShadowOffset(FVector2D(1.f, 2.f));
			New.Count = Count;
			Label = &Labels.Add_GetRef(New);
		}
		if (const ALFBrawler* Driver = Boat->GetDriver()) Label->Team = int32(Driver->Team);
		// An empty boat retains its last driver's colour. Before anyone drives, infer its authored hull colour.
		if (Label->Team == INDEX_NONE)
		{
			const UStaticMesh* Hull = Boat->GetHull()->GetStaticMesh();
			const FString Name = Hull ? Hull->GetName() : FString();
			Label->Team = Name.Contains(TEXT("Blue")) ? 1 : Name.Contains(TEXT("Green")) ? 2 : Name.Contains(TEXT("Yellow")) ? 3 : 0;
		}
		FVector2D Projected;
		const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Boat->GetActorLocation() + FVector(0.f, 0.f, 160.f), Projected, false);
		const FGeometry Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
		const FVector2D At = MyGeometry.AbsoluteToLocal(Viewport.LocalToAbsolute(Projected)) - FVector2D(65.f, 48.f);
		const FVector2D Size = MyGeometry.GetLocalSize();
		const bool bVisible = Entry.Cargo->GetCount() > 0 && bProjected && At.X >= 0.f && At.Y >= 0.f && At.X + 130.f <= Size.X && At.Y + 48.f <= Size.Y;
		Label->Panel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (bVisible)
		{
			CastChecked<UCanvasPanelSlot>(Label->Panel->Slot)->SetPosition(At);
			Label->Count->SetText(FText::AsNumber(Entry.Cargo->GetCount()));
			Label->Count->SetColorAndOpacity(Colours[FMath::Clamp(Label->Team, 0, 3)]);
		}
	}
}
