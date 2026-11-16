#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "PadelRowControl.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;
class UVerticalBox;

/**
 * A 72 px settings row (spec 6.7, 6.8, 6.9, 6.10): label left in Type.Body (Text.Secondary,
 * Text.Primary when focused, as on the design's board 06), a value control on the right. Focus looks like a list button: raised fill, a 6 px lime bar on the left and
 * an 8 px slide, no ring. An optional description line under the row shows only while
 * focused. UPadelToggle and UPadelSelector build on it; so does the Settings slider.
 */
UCLASS(Abstract)
class PADEL_API UPadelRowControl : public UPadelFocusableWidget
{
	GENERATED_BODY()

public:
	void SetLabel(const FText& InLabel);
	FText GetLabel() const { return Label; }
	/** Row width in px; 0 fills the column. */
	void SetRowWidth(float InWidth);
	/** Shown under the row only while it's focused (spec 6.8). Empty hides it. */
	void SetDescription(const FText& InDescription);
	FText GetDescription() const { return Description; }

protected:
	virtual void BuildTree() override;
	virtual void RefreshVisuals() override;
	virtual float FocusedScale() const override { return 1.0f; }

	/** Add the value widgets to the row's right side. */
	virtual void BuildValue(UHorizontalBox* Row) PURE_VIRTUAL(UPadelRowControl::BuildValue, );
	/** Update the value widgets from the control's state. */
	virtual void RefreshValue() {}
	/**
	 * True: focus is the list-row look (raised fill, bar, slide). The segmented control
	 * returns false and rings its pill instead (spec 6.9); the label still brightens.
	 */
	virtual bool ShowsRowFocus() const { return true; }
	/** The line under the row; a selector shows the current option's own line if it has one. */
	virtual FText CurrentDescription() const { return Description; }
	/** Right padding in px: 24 for toggles, 16 for selectors, 8 for the segmented pill (design boards). */
	virtual float RowRightPadding() const { return 24.0f; }
	/** The row's overlay, for subclasses that add a ring around their value. */
	UPROPERTY()
	TObjectPtr<UOverlay> RowRoot;

	FText Label;
	FText Description;
	float RowWidth = 0.0f;

	UPROPERTY()
	TObjectPtr<USizeBox> RowBox;

	UPROPERTY()
	TObjectPtr<UBorder> RowFill;

	UPROPERTY()
	TObjectPtr<USizeBox> BarBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionText;
};
