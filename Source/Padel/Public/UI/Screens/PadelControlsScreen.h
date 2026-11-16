#pragma once

#include "CoreMinimal.h"
#include "Gameplay/PadelInputConfig.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelControlsScreen.generated.h"

class UPadelButtonGlyph;
class UPadelPadDiagram;
class UPadelTabBar;
class UWidget;

/**
 * Spec 7.6, board "Controls7": every match action and its button (GDD 7), read-only.
 *
 * GAMEPAD tab: an original line drawing of a generic pad (UPadelPadDiagram) with each
 * control's label beside it, glyphs for the pad in use. KEYBOARD tab: the same actions in
 * the same order with their key caps, in two columns. Opens on the tab for the device used
 * last. Labels come from the binding table IMC_Match is built from
 * (UPadelInputConfig::GetDefaultBindings), so a changed binding moves its label; actions
 * not in the slice are greyed with "Later". Opened from Practice pause, the D-pad rows
 * show the practice controls instead.
 *
 * Nothing on the screen is focusable in the spec; the drawing holds the focus, invisibly,
 * so LB/RB and B work.
 */
UCLASS()
class PADEL_API UPadelControlsScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

public:
	/** 0 Gamepad, 1 Keyboard. */
	int32 GetTabIndex() const { return TabIndex; }
	/** An action's label as this screen shows it (the practice D-pad rows differ). */
	FText GetActionLabel(EPadelInputAction Action) const;

	static const FName SheetId;

	virtual TArray<FPadelHint> GetHints() const override;
	virtual Padel::Ui::EUiColour GetScrimColour() const override;
	virtual bool WantsBackgroundBlur() const override;

protected:
	virtual void BuildScreen() override;
	virtual bool OnScreenInput(Padel::Ui::EUiInput Input, bool bRepeat) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildGamepadTab();
	void BuildKeyboardTab();
	/** One label for the pad drawing: text (and its detail line) and the key's glyph. */
	UWidget* MakeCallout(EPadelInputAction Action, FName PadKey, bool bGlyphFirst);
	UWidget* MakeActionText(EPadelInputAction Action, bool bAlignRight);
	UWidget* MakeKeyboardRow(EPadelInputAction Action);
	void ShowTab(int32 NewIndex);
	void HandleTabChanged(int32 NewIndex);
	void RefreshGlyphFamily();
	bool OpenedFrom(Padel::Ui::EUiScreen Screen) const;
	bool OpenedFromPause() const;

	FText ActionDetail(EPadelInputAction Action) const;
	static bool IsLater(EPadelInputAction Action);

	int32 TabIndex = 0;
	bool bPractice = false;
	/** The pad the Gamepad tab's glyphs name (Xbox until a pad is used). */
	Padel::Ui::EPadFamily GlyphFamily = Padel::Ui::EPadFamily::Xbox;
	/** The device family last seen, so glyphs only refresh when it changes. */
	Padel::Ui::EPadFamily SeenFamily = Padel::Ui::EPadFamily::KeyboardMouse;

	UPROPERTY()
	TObjectPtr<UPadelTabBar> TabBar;

	UPROPERTY()
	TObjectPtr<UPadelPadDiagram> Diagram;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> GamepadWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> KeyboardWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UPadelButtonGlyph>> PadGlyphs;
};
