#include "UI/Widgets/PadelHintBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/Model/PadelUiControls.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelButtonGlyph.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	constexpr float HintGlyph = 40.0f;
	constexpr float HintGlyphGap = 8.0f;
	constexpr float HintItemGap = 32.0f;
	constexpr float HintBarPaddingX = 24.0f;
	constexpr float HintPauseStripAlpha = 0.8f;
}

void UPadelHintBar::SetHints(const TArray<FPadelHint>& InHints)
{
	Hints = InHints;
	Rebuild();
}

void UPadelHintBar::SetContext(const FText& InContext)
{
	Context = InContext;
	if (ContextText)
	{
		ContextText->SetText(Context);
	}
}

void UPadelHintBar::SetFamily(Padel::Ui::EPadFamily InFamily)
{
	if (Family != InFamily)
	{
		Family = InFamily;
		for (UPadelButtonGlyph* Glyph : ItemGlyphs)
		{
			if (Glyph)
			{
				Glyph->SetFamily(Family);
			}
		}
	}
}

void UPadelHintBar::SetPauseStrip(bool bInStrip)
{
	bPauseStrip = bInStrip;
	Rebuild();
}

void UPadelHintBar::FlashConfirm()
{
	for (int32 Index = 0; Index < Shown.Num(); ++Index)
	{
		if (Shown[Index] == EUiInput::Confirm && ItemGlyphs.IsValidIndex(Index) && ItemGlyphs[Index])
		{
			ItemGlyphs[Index]->Flash();
		}
	}
}

void UPadelHintBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Visible);
}

void UPadelHintBar::BuildTree()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Strip = MakeRoundedBorder(FLinearColor::Transparent, 0.0f, FLinearColor::Transparent, 0.0f, FMargin(HintBarPaddingX, 0.0f), Row);
	Strip->SetVerticalAlignment(VAlign_Center);
	SizeRoot = MakeSized(Strip, 0.0f, PadelUi::Theme().Layout.HintBarHeight);
	WidgetTree->RootWidget = SizeRoot;

	ContextText = MakeTokenText(Context, EUiType::BodySmall, EUiColour::TextSecondary);
	UHorizontalBoxSlot* ContextSlot = Row->AddChildToHorizontalBox(ContextText);
	ContextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ContextSlot->SetVerticalAlignment(VAlign_Center);

	Items = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->AddChildToHorizontalBox(Items)->SetVerticalAlignment(VAlign_Center);
	Rebuild();
}

void UPadelHintBar::Rebuild()
{
	if (!Items || !Strip || !SizeRoot)
	{
		return;
	}
	const Padel::Ui::FUiLayout& Layout = PadelUi::Theme().Layout;
	SizeRoot->SetHeightOverride(bPauseStrip ? Layout.HintBarPauseStrip : Layout.HintBarHeight);
	Strip->SetBrush(PadelUiPaint::Solid(
		bPauseStrip ? PadelUi::Colour(EUiColour::SurfaceCharcoal, HintPauseStripAlpha) : FLinearColor::Transparent));

	// Order through the model, so every screen gets the same A ... B order (spec 6.1).
	std::vector<Padel::Ui::FHint> ModelHints;
	TMap<EUiInput, FText> Labels;
	for (const FPadelHint& Hint : Hints)
	{
		ModelHints.push_back({Hint.Input, std::string()});
		Labels.Add(Hint.Input, Hint.Label);
	}
	const std::vector<Padel::Ui::FHint> Ordered = Padel::Ui::OrderHints(ModelHints);

	Items->ClearChildren();
	ItemBoxes.Reset();
	ItemGlyphs.Reset();
	Shown.Reset();
	for (const Padel::Ui::FHint& Hint : Ordered)
	{
		UHorizontalBox* Item = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UPadelButtonGlyph* Glyph = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
		Glyph->SetInput(Hint.Input);
		Glyph->SetFamily(Family);
		Glyph->SetDiameter(HintGlyph);
		Item->AddChildToHorizontalBox(Glyph)->SetVerticalAlignment(VAlign_Center);
		ItemGlyphs.Add(Glyph);

		// The tab pair shows both shoulder glyphs ("LB RB Change tab").
		if (Hint.Input == EUiInput::TabLeft)
		{
			UPadelButtonGlyph* Second = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
			Second->SetInput(EUiInput::TabRight);
			Second->SetFamily(Family);
			Second->SetDiameter(HintGlyph);
			UHorizontalBoxSlot* SecondSlot = Item->AddChildToHorizontalBox(Second);
			SecondSlot->SetVerticalAlignment(VAlign_Center);
			SecondSlot->SetPadding(FMargin(HintGlyphGap * 0.5f, 0.0f, 0.0f, 0.0f));
		}

		const FText* Label = Labels.Find(Hint.Input);
		if (!Label && Hint.Input == EUiInput::TabLeft)
		{
			Label = Labels.Find(EUiInput::TabRight);
		}
		UTextBlock* LabelText = MakeTokenText(Label ? *Label : FText::GetEmpty(), EUiType::BodySmall, EUiColour::TextPrimary);
		UHorizontalBoxSlot* LabelSlot = Item->AddChildToHorizontalBox(LabelText);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(HintGlyphGap, 0.0f, 0.0f, 0.0f));

		UHorizontalBoxSlot* ItemSlot = Items->AddChildToHorizontalBox(Item);
		ItemSlot->SetVerticalAlignment(VAlign_Center);
		ItemSlot->SetPadding(FMargin(Shown.Num() == 0 ? 0.0f : HintItemGap, 0.0f, 0.0f, 0.0f));
		ItemBoxes.Add(Item);
		Shown.Add(Hint.Input);
	}
}

EUiInput UPadelHintBar::HitTest(const FVector2D& ScreenPosition) const
{
	for (int32 Index = 0; Index < ItemBoxes.Num(); ++Index)
	{
		if (ItemBoxes[Index] && ItemBoxes[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return Shown[Index];
		}
	}
	return EUiInput::None;
}

FReply UPadelHintBar::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		PressedItem = HitTest(InMouseEvent.GetScreenSpacePosition());
		if (PressedItem != EUiInput::None)
		{
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPadelHintBar::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && PressedItem != EUiInput::None)
	{
		const EUiInput Released = HitTest(InMouseEvent.GetScreenSpacePosition());
		const EUiInput Pressed = PressedItem;
		PressedItem = EUiInput::None;
		if (Released == Pressed)
		{
			// The tab pair: its LB glyph goes left; the RB glyph or the label goes right.
			EUiInput Clicked = Pressed;
			if (Pressed == EUiInput::TabLeft)
			{
				const int32 Index = Shown.IndexOfByKey(EUiInput::TabLeft);
				const bool bOnLb = ItemGlyphs.IsValidIndex(Index) && ItemGlyphs[Index]
					&& ItemGlyphs[Index]->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
				Clicked = bOnLb ? EUiInput::TabLeft : EUiInput::TabRight;
			}
			OnHintClicked.Broadcast(Clicked);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}
