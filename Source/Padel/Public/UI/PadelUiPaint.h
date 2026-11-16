// Brushes for the menu look (docs/ui/MENU_SPEC.md 4.3): rounded glass panels, soft
// shadows and glows, the vignette and the tile's bottom fade. The soft shapes are small
// textures made once at runtime from code, so nothing here is a binary asset.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UTexture2D;

namespace PadelUiPaint
{
	/** A filled rounded box with an optional inner stroke (Glass.Edge, the focus ring). */
	FSlateBrush Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f);

	/** A flat fill. */
	FSlateBrush Solid(const FLinearColor& Fill);

	/**
	 * A soft halo around a box and clear inside it (a CSS box-shadow): drop shadows and the
	 * focus glow. Place it BlurPx / 2 outside the box on every side.
	 */
	FSlateBrush SoftBox(const FLinearColor& Tint, float BlurPx);

	/** Full-screen radial vignette, clear in the middle, Tint at the edges (Bg.Vignette). */
	FSlateBrush Vignette(const FLinearColor& Tint);

	/** Vertical fade, clear at the top and Tint at the bottom (the tile's 0 -> 0.85 gradient). */
	FSlateBrush FadeUp(const FLinearColor& Tint);
}
