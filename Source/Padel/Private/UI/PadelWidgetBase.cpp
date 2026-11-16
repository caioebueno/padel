#include "UI/PadelWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateTypes.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiStyle.h"
#include "UI/PadelUiTokens.h"

TSharedRef<SWidget> UPadelWidgetBase::RebuildWidget()
{
	EnsureBuilt();
	return Super::RebuildWidget();
}

void UPadelWidgetBase::EnsureBuilt()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
}

void UPadelWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Slate's own focus outline is faint on a TV; make the focused button obvious.
	for (int32 Index = 0; Index < FocusOrder.Num(); ++Index)
	{
		UButton* Button = FocusOrder[Index];
		UTextBlock* Label = FocusLabels.IsValidIndex(Index) ? FocusLabels[Index].Get() : nullptr;
		if (!Button || !Label)
		{
			continue;
		}
		const bool bFocused = Button->HasKeyboardFocus() || Button->IsHovered();
		Button->SetBackgroundColor(bFocused ? PadelUiStyle::Accent : FLinearColor::White);
		Label->SetColorAndOpacity(FSlateColor(bFocused ? PadelUiStyle::PointsText : PadelUiStyle::Text));
	}
}

UTextBlock* UPadelWidgetBase::MakeText(const FText& Text, int32 Size, const FLinearColor& Colour)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(PadelUiStyle::Font(Size));
	Block->SetColorAndOpacity(FSlateColor(Colour));
	return Block;
}

UBorder* UPadelWidgetBase::MakePanel(const FLinearColor& Colour, float InnerPadding)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Border->SetBrushColor(Colour);
	Border->SetPadding(FMargin(InnerPadding));
	return Border;
}

UButton* UPadelWidgetBase::MakeMenuButton(const FText& Label, UTextBlock** OutLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = Button->GetStyle();
	Style.Normal.TintColor = FSlateColor(PadelUiStyle::ButtonNormal);
	Style.Hovered.TintColor = FSlateColor(PadelUiStyle::ButtonFocused);
	Style.Pressed.TintColor = FSlateColor(PadelUiStyle::ButtonFocused);
	Style.NormalPadding = FMargin(24.0f, 12.0f);
	Style.PressedPadding = FMargin(24.0f, 12.0f);
	Button->SetStyle(Style);

	UTextBlock* Text = MakeText(Label, PadelUiStyle::FontBody, PadelUiStyle::Text);
	Button->AddChild(Text);
	if (OutLabel)
	{
		*OutLabel = Text;
	}
	FocusOrder.Add(Button);
	FocusLabels.Add(Text);
	return Button;
}

UTextBlock* UPadelWidgetBase::MakeTokenText(const FText& Text, Padel::Ui::EUiType Type, Padel::Ui::EUiColour Colour)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetFont(PadelUi::Font(Type));
	Block->SetColorAndOpacity(FSlateColor(PadelUi::Colour(Colour)));
	if (PadelUi::IsUpper(Type))
	{
		Block->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	return Block;
}

UBorder* UPadelWidgetBase::MakeRoundedBorder(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth,
	const FMargin& Padding, UWidget* Content)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Border->SetBrush(PadelUiPaint::Rounded(Fill, Radius, Outline, OutlineWidth));
	Border->SetPadding(Padding);
	if (Content)
	{
		Border->SetContent(Content);
	}
	return Border;
}

UOverlay* UPadelWidgetBase::MakeGlassPanel(UWidget* Content, const FMargin& Padding, float Radius, UBorder** OutFill)
{
	using Padel::Ui::EUiColour;
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	const bool bSolid = PadelUi::Options().bReduceTransparency;
	UOverlay* Panel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());

	// Drop shadow: #000 at 0.45, 12 px down, 32 px blur. The soft box spreads Blur/2 past
	// the panel so its falloff centres on the panel's edge, as a CSS blur does.
	const float Spread = Theme.Layout.ShadowBlur * 0.5f;
	UImage* Shadow = MakeImage(PadelUiPaint::SoftBox(PadelUi::Colour(EUiColour::GlassShadow), Theme.Layout.ShadowBlur));
	UOverlaySlot* ShadowSlot = Panel->AddChildToOverlay(Shadow);
	ShadowSlot->SetHorizontalAlignment(HAlign_Fill);
	ShadowSlot->SetVerticalAlignment(VAlign_Fill);
	ShadowSlot->SetPadding(FMargin(-Spread, -Spread + Theme.Layout.ShadowOffsetY, -Spread, -Spread - Theme.Layout.ShadowOffsetY));

	UBorder* Fill = MakeRoundedBorder(PadelUi::Colour(bSolid ? EUiColour::SurfaceCharcoal : EUiColour::GlassFill), Radius,
		PadelUi::Colour(EUiColour::GlassEdge), 1.0f, Padding, Content);
	UOverlaySlot* FillSlot = Panel->AddChildToOverlay(Fill);
	FillSlot->SetHorizontalAlignment(HAlign_Fill);
	FillSlot->SetVerticalAlignment(VAlign_Fill);

	// Glass.EdgeTop: a 1 px light catch along the top, inset by the corner radius.
	FSlateBrush TopBrush = PadelUiPaint::Solid(PadelUi::Colour(EUiColour::GlassEdgeTop));
	TopBrush.ImageSize = FVector2D(1.0f, 1.0f);
	UImage* TopEdge = MakeImage(TopBrush);
	UOverlaySlot* TopSlot = Panel->AddChildToOverlay(TopEdge);
	TopSlot->SetHorizontalAlignment(HAlign_Fill);
	TopSlot->SetVerticalAlignment(VAlign_Top);
	TopSlot->SetPadding(FMargin(Radius, 0.0f, Radius, 0.0f));

	if (OutFill)
	{
		*OutFill = Fill;
	}
	return Panel;
}

USizeBox* UPadelWidgetBase::MakeSized(UWidget* Content, float Width, float Height)
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (Width > 0.0f)
	{
		Size->SetWidthOverride(Width);
	}
	if (Height > 0.0f)
	{
		Size->SetHeightOverride(Height);
	}
	if (Content)
	{
		Size->SetContent(Content);
	}
	return Size;
}

UImage* UPadelWidgetBase::MakeImage(const FSlateBrush& Brush)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Image->SetBrush(Brush);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Image;
}

void UPadelWidgetBase::FocusButton(UButton* Button)
{
	if (Button && Button->GetOwningLocalPlayer())
	{
		Button->SetFocus();
	}
}

void UPadelWidgetBase::FocusFirst()
{
	for (UButton* Button : FocusOrder)
	{
		if (Button && Button->IsVisible())
		{
			FocusButton(Button);
			return;
		}
	}
}
