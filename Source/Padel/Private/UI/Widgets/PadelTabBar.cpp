#include "UI/Widgets/PadelTabBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelButtonGlyph.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	constexpr float TabGap = 48.0f;
	constexpr float TabGlyph = 32.0f;
	constexpr float TabGlyphGap = 24.0f;
	constexpr float TabUnderline = 4.0f;
	constexpr float TabUnderlineGap = 8.0f;
}

void UPadelTabBar::SetTabs(const TArray<FText>& InTabs)
{
	Tabs = InTabs;
	Model.Count = Tabs.Num();
	Model.Index = FMath::Clamp(Model.Index, 0, FMath::Max(0, Model.Count - 1));
	Rebuild();
}

void UPadelTabBar::SetIndex(int32 InIndex)
{
	Model.Index = FMath::Clamp(InIndex, 0, FMath::Max(0, Model.Count - 1));
	Refresh();
}

void UPadelTabBar::SetFamily(Padel::Ui::EPadFamily InFamily)
{
	Family = InFamily;
	if (LeftGlyph && RightGlyph)
	{
		LeftGlyph->SetFamily(Family);
		RightGlyph->SetFamily(Family);
	}
}

bool UPadelTabBar::HandleInput(EUiInput Input)
{
	if (Input != EUiInput::TabLeft && Input != EUiInput::TabRight)
	{
		return false;
	}
	if (!Model.Step(Input == EUiInput::TabRight ? 1 : -1))
	{
		return false;
	}
	Refresh();
	OnTabChanged.Broadcast(Model.Index);
	return true;
}

void UPadelTabBar::Select(int32 InIndex)
{
	if (InIndex != Model.Index && InIndex >= 0 && InIndex < Model.Count)
	{
		Model.Index = InIndex;
		Refresh();
		OnTabChanged.Broadcast(Model.Index);
	}
}

void UPadelTabBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Visible);
}

void UPadelTabBar::BuildTree()
{
	Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WidgetTree->RootWidget = Row;

	LeftGlyph = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
	LeftGlyph->SetInput(EUiInput::TabLeft);
	LeftGlyph->SetDiameter(TabGlyph);
	LeftGlyph->SetFamily(Family);
	UHorizontalBoxSlot* LeftSlot = Row->AddChildToHorizontalBox(LeftGlyph);
	LeftSlot->SetVerticalAlignment(VAlign_Center);
	LeftSlot->SetPadding(FMargin(0.0f, 0.0f, TabGlyphGap, 0.0f));

	TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->AddChildToHorizontalBox(TabRow)->SetVerticalAlignment(VAlign_Center);

	RightGlyph = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
	RightGlyph->SetInput(EUiInput::TabRight);
	RightGlyph->SetDiameter(TabGlyph);
	RightGlyph->SetFamily(Family);
	UHorizontalBoxSlot* RightSlot = Row->AddChildToHorizontalBox(RightGlyph);
	RightSlot->SetVerticalAlignment(VAlign_Center);
	RightSlot->SetPadding(FMargin(TabGlyphGap, 0.0f, 0.0f, 0.0f));
	Rebuild();
}

void UPadelTabBar::Rebuild()
{
	if (!TabRow)
	{
		return;
	}
	TabRow->ClearChildren();
	TabLabels.Reset();
	Underlines.Reset();
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		UVerticalBox* Tab = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* LabelText = MakeTokenText(Tabs[Index], EUiType::Heading, EUiColour::TextSecondary);
		Tab->AddChildToVerticalBox(LabelText);
		// The underline spans the label's full width (spec 6.11).
		UImage* Underline = MakeImage(PadelUiPaint::Solid(PadelUi::Colour(EUiColour::Accent)));
		UVerticalBoxSlot* LineSlot = Tab->AddChildToVerticalBox(MakeSized(Underline, 0.0f, TabUnderline));
		LineSlot->SetHorizontalAlignment(HAlign_Fill);
		LineSlot->SetPadding(FMargin(0.0f, TabUnderlineGap, 0.0f, 0.0f));
		UHorizontalBoxSlot* TabSlot = TabRow->AddChildToHorizontalBox(Tab);
		TabSlot->SetPadding(FMargin(Index == 0 ? 0.0f : TabGap, 0.0f, 0.0f, 0.0f));
		TabLabels.Add(LabelText);
		Underlines.Add(Underline);
	}
	Refresh();
}

void UPadelTabBar::Refresh()
{
	for (int32 Index = 0; Index < TabLabels.Num(); ++Index)
	{
		const bool bSelected = Index == Model.Index;
		if (TabLabels[Index])
		{
			TabLabels[Index]->SetColorAndOpacity(FSlateColor(PadelUi::Colour(bSelected ? EUiColour::TextPrimary : EUiColour::TextSecondary)));
		}
		if (Underlines.IsValidIndex(Index) && Underlines[Index])
		{
			// Hidden, not collapsed, so the row doesn't jump when the selection moves.
			Underlines[Index]->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	}
	// No wrap: a glyph with nowhere to go dims.
	if (LeftGlyph && RightGlyph)
	{
		LeftGlyph->SetRenderOpacity(Model.Index > 0 ? 1.0f : 0.4f);
		RightGlyph->SetRenderOpacity(Model.Index < Model.Count - 1 ? 1.0f : 0.4f);
	}
}

FReply UPadelTabBar::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D At = InMouseEvent.GetScreenSpacePosition();
		for (int32 Index = 0; Index < TabLabels.Num(); ++Index)
		{
			if (TabLabels[Index] && TabLabels[Index]->GetCachedGeometry().IsUnderLocation(At))
			{
				Select(Index);
				return FReply::Handled();
			}
		}
		if (LeftGlyph && LeftGlyph->GetCachedGeometry().IsUnderLocation(At))
		{
			HandleInput(EUiInput::TabLeft);
			return FReply::Handled();
		}
		if (RightGlyph && RightGlyph->GetCachedGeometry().IsUnderLocation(At))
		{
			HandleInput(EUiInput::TabRight);
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
