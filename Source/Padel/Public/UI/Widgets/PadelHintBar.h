#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/PadelScreenTypes.h"
#include "UI/PadelWidgetBase.h"
#include "PadelHintBar.generated.h"

class UBorder;
class UHorizontalBox;
class UPadelButtonGlyph;
class USizeBox;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelHintClicked, Padel::Ui::EUiInput /*Input*/);

/**
 * Spec 6.1: the strip at the bottom of every menu screen naming what each live button does.
 * Items are right-aligned in the fixed order A, X, Y, LB RB, Start, B (OrderHints), so B
 * always sits at the right edge. Optional context text on the left. On Pause it sits on a
 * full-width charcoal strip. Items are clickable and do what the button does.
 */
UCLASS()
class PADEL_API UPadelHintBar : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	/** Replaces the items. Only live actions belong here: a dead one is removed, not greyed. */
	void SetHints(const TArray<FPadelHint>& InHints);
	void SetContext(const FText& InContext);
	void SetFamily(Padel::Ui::EPadFamily InFamily);
	/** Pause: an 88 px Surface.Charcoal strip at 0.8 behind the items. */
	void SetPauseStrip(bool bInStrip);
	/** Spec 6.1: the A glyph flashes Accent for 120 ms whenever A is pressed. */
	void FlashConfirm();

	/** The inputs in display order, left to right (tests). TabRight is folded into TabLeft. */
	TArray<Padel::Ui::EUiInput> GetDisplayedInputs() const { return Shown; }

	FOnPadelHintClicked OnHintClicked;

protected:
	virtual void BuildTree() override;
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void Rebuild();
	/** The item under a screen position, or None. */
	Padel::Ui::EUiInput HitTest(const FVector2D& ScreenPosition) const;

	TArray<FPadelHint> Hints;
	TArray<Padel::Ui::EUiInput> Shown;
	FText Context;
	Padel::Ui::EPadFamily Family = Padel::Ui::EPadFamily::Xbox;
	bool bPauseStrip = false;
	Padel::Ui::EUiInput PressedItem = Padel::Ui::EUiInput::None;

	UPROPERTY()
	TObjectPtr<USizeBox> SizeRoot;

	UPROPERTY()
	TObjectPtr<UBorder> Strip;

	UPROPERTY()
	TObjectPtr<UTextBlock> ContextText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Items;

	/** One per shown item, parallel to Shown. */
	UPROPERTY()
	TArray<TObjectPtr<UHorizontalBox>> ItemBoxes;

	UPROPERTY()
	TArray<TObjectPtr<UPadelButtonGlyph>> ItemGlyphs;
};
