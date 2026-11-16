#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "PadelPadDiagram.generated.h"

/**
 * The Controls screen's pad drawing (spec 7.6, board "Controls7"): Padel::Ui::PadOutline()
 * in Text.Secondary 2 px strokes, and a leader line with a dot for each labelled control,
 * drawn as vector lines in NativePaint (no image assets). It covers the whole 1920 x 1080
 * stage and scales with it.
 *
 * It is also the Controls screen's one focusable element: spec 7.6 has nothing to focus,
 * but the screen holds focus so LB/RB and B work. It shows no focus state and A does
 * nothing on it.
 */
UCLASS()
class PADEL_API UPadelPadDiagram : public UPadelFocusableWidget
{
	GENERATED_BODY()

public:
	/** The pad keys that have a label, so only their leaders are drawn. */
	void SetLabelledKeys(const TArray<FString>& InKeys);

	virtual void Activate() override {}

protected:
	virtual void BuildTree() override;
	virtual float FocusedScale() const override { return 1.0f; }
	virtual float PressedScale() const override { return 1.0f; }
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TArray<FString> LabelledKeys;
	FSlateBrush DotBrush;
};
