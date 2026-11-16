#include "UI/Widgets/PadelSilhouette.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "UI/PadelUiTokens.h"

namespace
{
	// The design boards' figure, on a 200 x 280 grid: a head circle (r 40 at 100, 74), a
	// neck (84, 110, 32 x 22) and shoulders whose top is a half-round 172 px wide.
	constexpr float SilhouetteGridW = 200.0f;
	constexpr float SilhouetteGridH = 280.0f;
	// Spec 6.6: the team colour at 0.25 alpha.
	constexpr float SilhouetteAlpha = 0.25f;

	struct FSilhouettePart
	{
		FVector2f Pos;
		FVector2f Size;
		FVector4 Radii;  // top-left, top-right, bottom-right, bottom-left
	};

	const FSilhouettePart SilhouetteParts[] = {
		{{60.0f, 34.0f}, {80.0f, 80.0f}, FVector4(40.0f, 40.0f, 40.0f, 40.0f)},
		{{84.0f, 108.0f}, {32.0f, 26.0f}, FVector4(0.0f, 0.0f, 0.0f, 0.0f)},
		{{14.0f, 130.0f}, {172.0f, 150.0f}, FVector4(86.0f, 86.0f, 0.0f, 0.0f)},
	};
}

int32 UPadelSilhouette::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	// Keep the figure's proportions and stand it on the bottom edge, centred across.
	const float Scale = FMath::Min(Size.X / SilhouetteGridW, Size.Y / SilhouetteGridH);
	const FVector2f Offset((Size.X - SilhouetteGridW * Scale) * 0.5f, Size.Y - SilhouetteGridH * Scale);
	const FLinearColor Colour = PadelUi::TeamColour(Team) * FLinearColor(1.0f, 1.0f, 1.0f, SilhouetteAlpha)
		* InWidgetStyle.GetColorAndOpacityTint();
	PartBrushes.SetNum(UE_ARRAY_COUNT(SilhouetteParts));
	for (int32 Index = 0; Index < PartBrushes.Num(); ++Index)
	{
		const FSilhouettePart& Part = SilhouetteParts[Index];
		PartBrushes[Index] = FSlateRoundedBoxBrush(FLinearColor::White, Part.Radii * Scale);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1,
			AllottedGeometry.ToPaintGeometry(Part.Size * Scale, FSlateLayoutTransform(Offset + Part.Pos * Scale)),
			&PartBrushes[Index], ESlateDrawEffect::None, Colour);
	}
	return Layer + 1;
}
