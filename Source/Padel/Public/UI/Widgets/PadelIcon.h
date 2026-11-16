#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PadelIcon.generated.h"

/** The menu's line icons (spec 4.5): our own drawings, 2 px strokes on a 32 px grid. */
UENUM()
enum class EPadelIcon : uint8
{
	None,
	Padlock,
	Trophy,
	LinkedFigures,
	Court,
	Ball,
	Check,
	ChevronLeft,
	ChevronRight,
	// PlayStation face buttons, drawn as plain shapes (spec 4.5), not any platform's art.
	ShapeCross,
	ShapeCircle,
	ShapeSquare,
	ShapeTriangle,
	// The D-pad with one direction marked (practice controls, spec 7.9).
	DPadUp,
	DPadDown,
	DPadLeft,
	DPadRight,
};

/**
 * Draws one EPadelIcon as vector strokes in NativePaint, scaled to its allotted size, so
 * icons need no texture assets. Put it in a USizeBox to give it a size.
 */
UCLASS()
class PADEL_API UPadelIcon : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetIcon(EPadelIcon InIcon) { Icon = InIcon; }
	EPadelIcon GetIcon() const { return Icon; }
	void SetTint(const FLinearColor& InTint) { Tint = InTint; }
	/** Stroke width in px at the icon's 32 px design size; scales with it. */
	void SetStroke(float InStroke) { Stroke = InStroke; }

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	EPadelIcon Icon = EPadelIcon::None;
	FLinearColor Tint = FLinearColor::White;
	float Stroke = 2.0f;
};
