#include "UI/Widgets/PadelRowControl.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	constexpr float RowControlHeight = 72.0f;
	// The design's settings rows (board 06): 24 px each side, 32 on the left when focused.
	constexpr float RowControlPadding = 24.0f;
}

void UPadelRowControl::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	RefreshVisuals();
}

void UPadelRowControl::SetRowWidth(float InWidth)
{
	RowWidth = InWidth;
	RefreshVisuals();
}

void UPadelRowControl::SetDescription(const FText& InDescription)
{
	Description = InDescription;
	RefreshVisuals();
}

void UPadelRowControl::BuildTree()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = Column;

	RowRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	RowBox = MakeSized(RowRoot, 0.0f, RowControlHeight);
	Column->AddChildToVerticalBox(RowBox);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RowFill = MakeRoundedBorder(FLinearColor::Transparent, PadelUi::Theme().Layout.RadiusControl, FLinearColor::Transparent, 0.0f,
		FMargin(RowControlPadding, 0.0f), Row);
	RowFill->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* FillSlot = RowRoot->AddChildToOverlay(RowFill);
	FillSlot->SetHorizontalAlignment(HAlign_Fill);
	FillSlot->SetVerticalAlignment(VAlign_Fill);

	LabelText = MakeTokenText(Label, EUiType::Body, EUiColour::TextSecondary);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	BuildValue(Row);

	BarBox = MakeSized(MakeImage(PadelUiPaint::Solid(PadelUi::Colour(EUiColour::Accent))), PadelUi::Theme().Layout.FocusRowBar, 0.0f);
	UOverlaySlot* BarSlot = RowRoot->AddChildToOverlay(BarBox);
	BarSlot->SetHorizontalAlignment(HAlign_Left);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	DescriptionText = MakeTokenText(Description, EUiType::BodySmall, EUiColour::TextSecondary);
	DescriptionText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(DescriptionText)->SetPadding(FMargin(RowControlPadding, 8.0f, RowControlPadding, 0.0f));
	RefreshVisuals();
}

void UPadelRowControl::RefreshVisuals()
{
	if (!RowBox || !RowFill || !LabelText)
	{
		return;
	}
	const Padel::Ui::FUiLayout& Layout = PadelUi::Theme().Layout;
	const bool bFocused = IsElementFocused() && !IsDisabled();
	const bool bRowFocus = bFocused && ShowsRowFocus();
	const bool bPressed = IsPressedVisual() && !IsDisabled();
	if (RowWidth > 0.0f)
	{
		RowBox->SetWidthOverride(RowWidth);
	}
	else
	{
		RowBox->ClearWidthOverride();
	}
	RowFill->SetBrush(PadelUiPaint::Rounded(bRowFocus ? PadelUi::Colour(EUiColour::GlassFillRaised) : FLinearColor::Transparent,
		Layout.RadiusControl));
	RowFill->SetPadding(FMargin(RowControlPadding + (bRowFocus ? Layout.FocusRowSlide : 0.0f), 0.0f, RowRightPadding(), 0.0f));
	LabelText->SetText(Label);
	LabelText->SetColorAndOpacity(FSlateColor(PadelUi::Colour(
		IsDisabled() ? EUiColour::TextDisabled : (bFocused ? EUiColour::TextPrimary : EUiColour::TextSecondary))));
	if (BarBox)
	{
		BarBox->SetWidthOverride(bPressed ? Layout.FocusRowBarPressed : Layout.FocusRowBar);
		BarBox->SetVisibility(bRowFocus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DescriptionText)
	{
		const FText Shown = CurrentDescription();
		DescriptionText->SetText(Shown);
		DescriptionText->SetVisibility(bFocused && !Shown.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshValue();
}
