#include "UI/Widgets/PadelButtonGlyph.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiStyle.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelIcon.h"

using Padel::Ui::EPadFamily;
using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	EPadelIcon PlayStationShape(EUiInput Input)
	{
		switch (Input)
		{
		case EUiInput::Confirm:
			return EPadelIcon::ShapeCross;
		case EUiInput::Back:
			return EPadelIcon::ShapeCircle;
		case EUiInput::Secondary:
			return EPadelIcon::ShapeSquare;
		case EUiInput::Tertiary:
			return EPadelIcon::ShapeTriangle;
		default:
			return EPadelIcon::None;
		}
	}

	bool IsShoulder(EUiInput Input)
	{
		return Input == EUiInput::TabLeft || Input == EUiInput::TabRight;
	}
}

void UPadelButtonGlyph::SetInput(EUiInput InInput)
{
	Input = InInput;
	Refresh();
}

void UPadelButtonGlyph::SetFamily(EPadFamily InFamily)
{
	Family = InFamily;
	Refresh();
}

void UPadelButtonGlyph::SetDiameter(float InDiameter)
{
	Diameter = InDiameter;
	Refresh();
}

void UPadelButtonGlyph::Flash()
{
	FlashTimer = PadelUi::Theme().Motion.HintFlash;
	Refresh();
}

FString UPadelButtonGlyph::GetGlyphText() const
{
	if (Family == EPadFamily::KeyboardMouse)
	{
		return UTF8_TO_TCHAR(Padel::Ui::KeyboardHintLabel(Input));
	}
	if (Family == EPadFamily::PlayStation && PlayStationShape(Input) != EPadelIcon::None)
	{
		return FString();
	}
	return PadelUiStyle::ToFString(Padel::Ui::GamepadKeyLabel(Padel::Ui::PadKeyName(Input), Family));
}

void UPadelButtonGlyph::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Box = MakeSized(Root, 0.0f, 0.0f);
	WidgetTree->RootWidget = Box;

	Cap = MakeRoundedBorder(FLinearColor::Transparent, 0.0f, FLinearColor::Transparent, 0.0f, FMargin(0.0f));
	UOverlaySlot* CapSlot = Root->AddChildToOverlay(Cap);
	CapSlot->SetHorizontalAlignment(HAlign_Fill);
	CapSlot->SetVerticalAlignment(VAlign_Fill);

	Label = MakeTokenText(FText::GetEmpty(), EUiType::Label, EUiColour::TextPrimary);
	Label->SetJustification(ETextJustify::Center);
	UOverlaySlot* LabelSlot = Root->AddChildToOverlay(Label);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	Shape = WidgetTree->ConstructWidget<UPadelIcon>(UPadelIcon::StaticClass());
	ShapeBox = MakeSized(Shape, 0.0f, 0.0f);
	UOverlaySlot* ShapeSlot = Root->AddChildToOverlay(ShapeBox);
	ShapeSlot->SetHorizontalAlignment(HAlign_Center);
	ShapeSlot->SetVerticalAlignment(VAlign_Center);
	Refresh();
}

void UPadelButtonGlyph::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (FlashTimer > 0.0f)
	{
		FlashTimer -= InDeltaTime;
		if (FlashTimer <= 0.0f)
		{
			Refresh();
		}
	}
}

void UPadelButtonGlyph::Refresh()
{
	if (!Box || !Cap || !Label || !Shape)
	{
		return;
	}
	const bool bFlash = FlashTimer > 0.0f;
	const FString Text = GetGlyphText();
	const EPadelIcon Icon = Family == EPadFamily::PlayStation ? PlayStationShape(Input) : EPadelIcon::None;
	const bool bKey = Family == EPadFamily::KeyboardMouse;
	const bool bShoulder = !bKey && IsShoulder(Input);

	// A key cap and the shoulder buttons are rounded rectangles; face buttons are circles.
	const float Height = Diameter;
	const float Width = bShoulder ? Diameter * 1.5f : Diameter;
	Box->SetHeightOverride(Height);
	if (bKey)
	{
		Box->ClearWidthOverride();
		Box->SetMinDesiredWidth(Diameter);
	}
	else
	{
		Box->SetWidthOverride(Width);
	}
	const float Radius = (bKey || bShoulder) ? PadelUi::Theme().Layout.RadiusControl : Diameter * 0.5f;
	const FLinearColor Fill = PadelUi::Colour(bFlash ? EUiColour::Accent : EUiColour::SurfaceCharcoal);
	Cap->SetBrush(PadelUiPaint::Rounded(Fill, Radius, PadelUi::Colour(EUiColour::GlassEdge), 2.0f));

	Label->SetText(FText::FromString(Text));
	Label->SetColorAndOpacity(FSlateColor(PadelUi::Colour(bFlash ? EUiColour::OnAccent : EUiColour::TextPrimary)));
	Label->SetVisibility(Icon == EPadelIcon::None ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	// A key cap grows to fit its key name, with room either side.
	if (UOverlaySlot* LabelSlot = Cast<UOverlaySlot>(Label->Slot))
	{
		LabelSlot->SetPadding(FMargin(bKey ? 10.0f : 0.0f, 0.0f));
	}

	Shape->SetIcon(Icon);
	Shape->SetTint(PadelUi::Colour(bFlash ? EUiColour::OnAccent : EUiColour::TextPrimary));
	ShapeBox->SetWidthOverride(Diameter * 0.6f);
	ShapeBox->SetHeightOverride(Diameter * 0.6f);
	ShapeBox->SetVisibility(Icon == EPadelIcon::None ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
