#include "UI/Widgets/PadelButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelButtonGlyph.h"
#include "UI/Widgets/PadelIcon.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	// Spec 6.2: Accent (or Danger) at 0.18 over Glass.Fill, composited into one colour.
	FLinearColor ButtonTintOverGlass(EUiColour Tint)
	{
		const FLinearColor Glass = PadelUi::Colour(EUiColour::GlassFill);
		const FLinearColor Over = PadelUi::Colour(Tint);
		constexpr float Amount = 0.18f;
		FLinearColor Mixed = FMath::Lerp(Glass, Over, Amount);
		Mixed.A = 1.0f - (1.0f - Glass.A) * (1.0f - Amount);
		return Mixed;
	}

	FVector2D DefaultButtonSize(EPadelButtonKind Kind)
	{
		switch (Kind)
		{
		case EPadelButtonKind::Primary:
			return FVector2D(448.0f, 80.0f);
		case EPadelButtonKind::Secondary:
			return FVector2D(240.0f, 64.0f);
		case EPadelButtonKind::Danger:
			return FVector2D(320.0f, 72.0f);
		case EPadelButtonKind::List:
			return FVector2D(0.0f, 72.0f);
		}
		return FVector2D::ZeroVector;
	}
}

void UPadelButton::SetKind(EPadelButtonKind InKind)
{
	Kind = InKind;
	RefreshVisuals();
}

void UPadelButton::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	RefreshVisuals();
}

void UPadelButton::SetSize(float InWidth, float InHeight)
{
	Width = InWidth;
	Height = InHeight;
	RefreshVisuals();
}

void UPadelButton::SetShowGlyph(bool bShow, Padel::Ui::EPadFamily InFamily)
{
	bShowGlyph = bShow;
	GlyphFamily = InFamily;
	RefreshVisuals();
}

void UPadelButton::SetTrailing(const FText& InValue, bool bChevron)
{
	Trailing = InValue;
	bTrailingChevron = bChevron;
	RefreshVisuals();
}

float UPadelButton::FocusedScale() const
{
	// List rows are too close for a scale; they slide instead (spec 4.3).
	return Kind == EPadelButtonKind::List ? 1.0f : Super::FocusedScale();
}

void UPadelButton::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeRoot = MakeSized(Root, 0.0f, 0.0f);
	WidgetTree->RootWidget = SizeRoot;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Fill = MakeRoundedBorder(FLinearColor::Transparent, PadelUi::Theme().Layout.RadiusControl, FLinearColor::Transparent, 0.0f,
		FMargin(0.0f), Row);
	Fill->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* FillSlot = Root->AddChildToOverlay(Fill);
	FillSlot->SetHorizontalAlignment(HAlign_Fill);
	FillSlot->SetVerticalAlignment(VAlign_Fill);

	Glyph = WidgetTree->ConstructWidget<UPadelButtonGlyph>(UPadelButtonGlyph::StaticClass());
	Glyph->SetInput(Padel::Ui::EUiInput::Confirm);
	Glyph->SetDiameter(32.0f);
	Row->AddChildToHorizontalBox(Glyph)->SetVerticalAlignment(VAlign_Center);

	LabelText = MakeTokenText(Label, EUiType::Heading, EUiColour::TextSecondary);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Row->AddChildToHorizontalBox(Gap)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	TrailingText = MakeTokenText(Trailing, EUiType::Heading, EUiColour::TextSecondary);
	Row->AddChildToHorizontalBox(TrailingText)->SetVerticalAlignment(VAlign_Center);

	Chevron = WidgetTree->ConstructWidget<UPadelIcon>(UPadelIcon::StaticClass());
	Chevron->SetIcon(EPadelIcon::ChevronRight);
	ChevronBox = MakeSized(Chevron, 32.0f, 32.0f);
	Row->AddChildToHorizontalBox(ChevronBox)->SetVerticalAlignment(VAlign_Center);

	// List rows: the 6 px lime bar on the left edge (10 px while pressed).
	ListBar = MakeImage(PadelUiPaint::Solid(PadelUi::Colour(EUiColour::Accent)));
	ListBarBox = MakeSized(ListBar, PadelUi::Theme().Layout.FocusRowBar, 0.0f);
	UOverlaySlot* BarSlot = Root->AddChildToOverlay(ListBarBox);
	BarSlot->SetHorizontalAlignment(HAlign_Left);
	BarSlot->SetVerticalAlignment(VAlign_Fill);

	AddFocusRing(Root, PadelUi::Theme().Layout.RadiusControl);
	RefreshVisuals();
}

void UPadelButton::RefreshVisuals()
{
	if (!Fill || !LabelText || !SizeRoot)
	{
		return;
	}
	const Padel::Ui::FUiLayout& Layout = PadelUi::Theme().Layout;
	const bool bFocused = IsElementFocused() && !IsDisabled();
	const bool bPressed = IsPressedVisual() && !IsDisabled();
	const bool bList = Kind == EPadelButtonKind::List;
	const bool bLead = Kind == EPadelButtonKind::Primary || Kind == EPadelButtonKind::Danger;

	const FVector2D Default = DefaultButtonSize(Kind);
	const float W = Width > 0.0f ? Width : Default.X;
	const float H = Height > 0.0f ? Height : Default.Y;
	if (W > 0.0f)
	{
		SizeRoot->SetWidthOverride(W);
	}
	else
	{
		SizeRoot->ClearWidthOverride();
	}
	SizeRoot->SetHeightOverride(H);

	FLinearColor FillColour = FLinearColor::Transparent;
	FLinearColor Stroke = FLinearColor::Transparent;
	float StrokeWidth = 0.0f;
	EUiColour LabelColour = EUiColour::TextSecondary;
	FLinearColor LabelOverride = FLinearColor::Transparent;

	switch (Kind)
	{
	case EPadelButtonKind::Primary:
		if (IsDisabled())
		{
			FillColour = PadelUi::Colour(EUiColour::GlassFill);
			LabelColour = EUiColour::TextDisabled;
		}
		else if (bPressed)
		{
			FillColour = PadelUi::Colour(EUiColour::AccentPressed);
			LabelColour = EUiColour::OnAccent;
		}
		else if (bFocused)
		{
			FillColour = PadelUi::Colour(EUiColour::Accent);
			LabelColour = EUiColour::OnAccent;
		}
		else
		{
			FillColour = ButtonTintOverGlass(EUiColour::Accent);
			Stroke = PadelUi::Colour(EUiColour::Accent);
			StrokeWidth = 2.0f;
			LabelColour = EUiColour::TextPrimary;
		}
		break;
	case EPadelButtonKind::Danger:
		if (bFocused || bPressed)
		{
			FillColour = PadelUi::Colour(EUiColour::StateDanger);
			LabelOverride = FLinearColor::White;
		}
		else
		{
			FillColour = ButtonTintOverGlass(EUiColour::StateDanger);
			Stroke = PadelUi::Colour(EUiColour::StateDanger);
			StrokeWidth = 2.0f;
			LabelColour = EUiColour::TextPrimary;
		}
		break;
	case EPadelButtonKind::Secondary:
		if (IsDisabled())
		{
			FillColour = PadelUi::Colour(EUiColour::GlassFill, 0.5f);
			LabelColour = EUiColour::TextDisabled;
		}
		else if (bFocused || bPressed)
		{
			FillColour = PadelUi::Colour(EUiColour::GlassFillRaised);
			LabelColour = EUiColour::TextPrimary;
		}
		else
		{
			FillColour = PadelUi::Colour(EUiColour::GlassFill);
			Stroke = PadelUi::Colour(EUiColour::GlassEdge);
			StrokeWidth = 2.0f;
			LabelColour = EUiColour::TextSecondary;
		}
		break;
	case EPadelButtonKind::List:
		FillColour = (bFocused || bPressed) ? PadelUi::Colour(EUiColour::GlassFillRaised) : FLinearColor::Transparent;
		LabelColour = IsDisabled() ? EUiColour::TextDisabled : ((bFocused || bPressed) ? EUiColour::TextPrimary : EUiColour::TextSecondary);
		break;
	}

	Fill->SetBrush(PadelUiPaint::Rounded(FillColour, Layout.RadiusControl, Stroke, StrokeWidth));
	const FLinearColor TextColour = LabelOverride.A > 0.0f ? LabelOverride : PadelUi::Colour(LabelColour);

	// Primary and danger labels are Type.Title, centred; the others Type.Heading.
	LabelText->SetText(Label);
	LabelText->SetFont(PadelUi::Font(bLead ? EUiType::Title : EUiType::Heading));
	LabelText->SetColorAndOpacity(FSlateColor(TextColour));
	if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(LabelText->Slot))
	{
		LabelSlot->SetSize(FSlateChildSize(bList ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill));
		LabelSlot->SetHorizontalAlignment(bList ? HAlign_Left : HAlign_Center);
		LabelSlot->SetPadding(FMargin(bShowGlyph && bLead ? 12.0f : 0.0f, 0.0f, 0.0f, 0.0f));
	}
	LabelText->SetJustification(bList ? ETextJustify::Left : ETextJustify::Center);

	// List rows: 32 px padding, the label slides 8 px right on focus.
	const float Slide = bList && (bFocused || bPressed) ? Layout.FocusRowSlide : 0.0f;
	Fill->SetPadding(bList ? FMargin(32.0f + Slide, 0.0f, 32.0f, 0.0f) : FMargin(24.0f, 0.0f));
	Fill->SetHorizontalAlignment(HAlign_Fill);

	if (Glyph)
	{
		Glyph->SetFamily(GlyphFamily);
		Glyph->SetVisibility(bShowGlyph && bLead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TrailingText)
	{
		TrailingText->SetText(Trailing);
		TrailingText->SetColorAndOpacity(FSlateColor(TextColour));
		TrailingText->SetVisibility(bList && !Trailing.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ChevronBox && Chevron)
	{
		Chevron->SetTint(TextColour);
		ChevronBox->SetVisibility(bList && bTrailingChevron ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ListBarBox)
	{
		ListBarBox->SetWidthOverride(bPressed ? Layout.FocusRowBarPressed : Layout.FocusRowBar);
		ListBarBox->SetVisibility(bList && (bFocused || bPressed) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// Rows show focus with the bar; every other button with the ring.
	ShowFocusRing(!bList && bFocused);
}
