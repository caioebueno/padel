#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/PadelWidgetBase.h"
#include "PadelButtonGlyph.generated.h"

class UBorder;
class UPadelIcon;
class USizeBox;
class UTextBlock;

/**
 * One button glyph (spec 4.5): a circle (40 px in the hint bar, 32 px inline) with the
 * button's letter, PlayStation's four shapes drawn as our own lines, shoulder buttons as
 * rounded rectangles, keys as a key cap. Text.Primary on Surface.Charcoal with a 2 px
 * Glass.Edge ring for every pad: the pads' face-button colours aren't reproduced.
 * Mapping is by position (GDD 7): confirm is the bottom face button on every pad.
 */
UCLASS()
class PADEL_API UPadelButtonGlyph : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void SetInput(Padel::Ui::EUiInput InInput);
	/**
	 * Show a game key instead of a menu input (the practice controls, spec 7.9): PadKey is
	 * an Unreal gamepad key name ("Gamepad_DPad_Left"), KeyCap the keyboard key's text.
	 * The D-pad draws as a cross with its arm marked; other pad keys use GamepadKeyLabel.
	 */
	void SetKey(const FString& InPadKey, const FText& InKeyCap);
	void SetFamily(Padel::Ui::EPadFamily InFamily);
	/** 40 for the hint bar, 32 inline. */
	void SetDiameter(float InDiameter);
	/** Spec 6.1: the A glyph flashes Accent for 120 ms when A is pressed. */
	void Flash();

	/** The text the glyph shows ("A", "LB", "Enter"), or empty when it draws a shape. */
	FString GetGlyphText() const;

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Refresh();

	Padel::Ui::EUiInput Input = Padel::Ui::EUiInput::Confirm;
	/** Set by SetKey: a game key rather than a menu input. */
	bool bRawKey = false;
	FString PadKey;
	FText KeyCap;
	Padel::Ui::EPadFamily Family = Padel::Ui::EPadFamily::Xbox;
	float Diameter = 40.0f;
	float FlashTimer = 0.0f;

	UPROPERTY()
	TObjectPtr<USizeBox> Box;

	UPROPERTY()
	TObjectPtr<UBorder> Cap;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	UPROPERTY()
	TObjectPtr<UPadelIcon> Shape;

	UPROPERTY()
	TObjectPtr<USizeBox> ShapeBox;
};
