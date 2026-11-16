#include "UI/Widgets/PadelPadDiagram.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElements.h"
#include "UI/Model/PadelUiPadDiagram.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;

namespace
{
	// The drawing's coordinates are the spec's 1920-wide stage.
	constexpr float DiagramStageWidth = 1920.0f;

	TArray<FVector2f> DiagramPoints(const Padel::Ui::FDiagramLine& Line, float Scale)
	{
		TArray<FVector2f> Points;
		Points.Reserve(static_cast<int32>(Line.size()));
		for (const Padel::Ui::FDiagramPoint& Point : Line)
		{
			Points.Add(FVector2f(Point.X * Scale, Point.Y * Scale));
		}
		return Points;
	}
}

void UPadelPadDiagram::SetLabelledKeys(const TArray<FString>& InKeys)
{
	LabelledKeys = InKeys;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UPadelPadDiagram::BuildTree()
{
	// Nothing to lay out: the drawing is all NativePaint.
	WidgetTree->RootWidget = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DotBrush = PadelUiPaint::Rounded(FLinearColor::White, PadelUi::Theme().Layout.RadiusPill);
}

int32 UPadelPadDiagram::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled) + 1;
	const Padel::Ui::FPadDiagramLayout& Layout = Padel::Ui::PadDiagramLayout();
	const float Scale = AllottedGeometry.GetLocalSize().X / DiagramStageWidth;
	if (Scale <= 0.0f)
	{
		return Layer;
	}
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const FPaintGeometry Paint = AllottedGeometry.ToPaintGeometry();

	// Spec 7.6: Text.Secondary 2 px strokes for the pad.
	const FLinearColor PadColour = PadelUi::Colour(EUiColour::TextSecondary) * Tint;
	for (const Padel::Ui::FDiagramLine& Line : Padel::Ui::PadOutline())
	{
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Paint, DiagramPoints(Line, Scale), ESlateDrawEffect::None, PadColour, true,
			Layout.Stroke * Scale);
	}

	// Board: leaders in Text.Disabled, a Text.Primary dot on the control.
	const FLinearColor LeaderColour = PadelUi::Colour(EUiColour::TextDisabled) * Tint;
	const FLinearColor DotColour = PadelUi::Colour(EUiColour::TextPrimary) * Tint;
	const float Dot = Layout.DotRadius * 2.0f * Scale;
	for (const Padel::Ui::FPadCallout& Callout : Padel::Ui::PadCallouts())
	{
		if (!LabelledKeys.Contains(UTF8_TO_TCHAR(Callout.KeyName.c_str())))
		{
			continue;
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Paint, DiagramPoints(Callout.Leader, Scale), ESlateDrawEffect::None,
			LeaderColour, true, Layout.Stroke * Scale);
		const Padel::Ui::FDiagramPoint& End = Callout.Leader.back();
		const FVector2f DotAt(End.X * Scale - Dot * 0.5f, End.Y * Scale - Dot * 0.5f);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2f(Dot, Dot), FSlateLayoutTransform(DotAt)),
			&DotBrush, ESlateDrawEffect::None, DotColour);
	}
	return Layer + 1;
}
