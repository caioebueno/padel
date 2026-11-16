#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/Model/PadelUiScreenFlow.h"
#include "UI/Model/PadelUiTheme.h"
#include "UI/PadelScreenTypes.h"
#include "UI/PadelWidgetBase.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "PadelScreenWidget.generated.h"

class IPadelMenuHost;
class UCanvasPanel;
class UPadelFocusableWidget;
class UPadelHintBar;
class UPadelMenuRoot;
class UPadelScreenHeader;

/**
 * Base for every menu screen (spec 11.1). It owns:
 *
 * - The stage: a 1920-wide canvas in 1080p pixels (taller on narrower screens), with the
 *   header band at y 64-160 and the hint bar band at y 960-1016 (spec 4.3). Screens place
 *   their content with Place(), in the spec's own grid columns and pixel rows.
 * - Focus: an explicit FFocusMap over the registered elements (spec 5.3). Directions move
 *   focus through the map; an element may use left/right itself (selectors, toggles).
 *   Hover focuses; A activates; B runs the screen's Back (spec 3's table).
 * - The entry lock (120 ms, spec 3) and the entry animation (spec 9): a 48 px slide and
 *   fade, or a fade alone for Pause, the match and Reduce motion.
 * - The hint bar, rebuilt from GetHints() whenever RefreshHints() is called.
 *
 * A screen subclass overrides BuildScreen() to add its widgets and focus map, and the
 * On* hooks to react. It reaches the game through GetHost() and other screens through
 * GetMenuRoot(). It never takes Slate keyboard focus: the menu root keeps that and feeds
 * HandleMenuInput().
 */
UCLASS(Abstract)
class PADEL_API UPadelScreenWidget : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	/** Called by the menu root right after creating the screen, before it's built. */
	void InitScreen(UPadelMenuRoot* InRoot, EPadelScreen InScreen);
	EPadelScreen GetScreenId() const { return ScreenId; }

	/** Arrival: restore focus (empty = the map's default), start the lock and animation. */
	void EnterScreen(const FString& RestoreFocus, Padel::Ui::EUiTransition Transition);

	/** One menu input. bRepeat for held-direction repeats. True if the screen used it. */
	bool HandleMenuInput(Padel::Ui::EUiInput Input, bool bRepeat);

	FName GetFocusedId() const { return FocusedId; }
	UPadelFocusableWidget* GetFocusedElement() const;
	/** Move focus to an element of this screen. False if it isn't in the map or is disabled. */
	bool FocusElement(FName Id);
	UPadelFocusableWidget* FindElement(FName Id) const;

	/** The hint items for the current state (spec 6.1). Only live actions. */
	virtual TArray<FPadelHint> GetHints() const;
	/** The full-screen scrim behind this screen (spec 4.1): Bg.Scrim unless it says otherwise. */
	virtual Padel::Ui::EUiColour GetScrimColour() const { return Padel::Ui::EUiColour::BgScrim; }
	/** True to blur the frozen match behind the screen (Pause, spec 7.5). */
	virtual bool WantsBackgroundBlur() const { return false; }
	/** Extra Confirm lock after the entry lock (Stats: 1 s, spec 7.8). */
	virtual float GetConfirmLockSeconds() const { return 0.0f; }
	/** A further Confirm gate on top of the lock (Stats: until its entry animation ends). */
	virtual bool IsConfirmBlocked() const { return false; }

	void SetPadFamily(Padel::Ui::EPadFamily InFamily);
	Padel::Ui::EPadFamily GetPadFamily() const { return PadFamily; }
	/** Spec 6.1: the A glyph flashes on every A press. */
	void FlashConfirmHint();
	/** Rebuild the hint bar from GetHints(). */
	void RefreshHints();

	UPadelHintBar* GetHintBar() const { return HintBar; }
	UPadelScreenHeader* GetHeader() const { return Header; }
	const Padel::Ui::FFocusMap& GetFocusMap() const { return FocusMap; }
	/** True while the entry lock holds input (tests). */
	bool IsInputLocked() const { return !InputLock.AcceptsInput(); }
	/** Skip the entry lock and animation (tests, and rebuilds after a settings change). */
	void SkipEntryAnimation();

protected:
	virtual void BuildTree() override final;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Add the screen's widgets (Place, RegisterElement) and fill FocusMap. Called once. */
	virtual void BuildScreen() PURE_VIRTUAL(UPadelScreenWidget::BuildScreen, );
	/** A or a click on an element (also a locked one: show its toast). */
	virtual void OnElementActivated(UPadelFocusableWidget* Element) {}
	/** A value control changed. */
	virtual void OnElementValueChanged(UPadelFocusableWidget* Element) {}
	/** Focus moved to Element (update a description line or context panel). */
	virtual void OnFocusChanged(UPadelFocusableWidget* Element) {}
	/**
	 * B. The default follows spec 3's Back table through the menu root (Main menu opens
	 * "Quit game?", Setup goes to the main menu, Pause resumes, Controls and Settings go
	 * back to their opener). Return true if handled.
	 */
	virtual bool OnBack();
	/** X, Y, LB, RB, Start: anything the focus logic didn't use. True if handled. */
	virtual bool OnScreenInput(Padel::Ui::EUiInput Input, bool bRepeat) { return false; }
	/**
	 * A fresh Confirm press while Confirm is locked or blocked, after the entry lock (Stats:
	 * finish the entry animation, spec 7.8). It never activates anything. True if used.
	 */
	virtual bool OnBlockedConfirm() { return false; }

	/** Header text: the Display title and the optional breadcrumb above it. */
	void SetHeader(const FText& Title, const FText& Breadcrumb = FText::GetEmpty());
	/** Hide the header band (Title, Versus). */
	void SetHeaderVisible(bool bVisible);
	void SetHintBarVisible(bool bVisible);
	/**
	 * Spec 6.1 on Pause: the hint bar sits on a full-width Surface.Charcoal strip, 88 px
	 * tall and centred on the hint bar band, so it reads over the frozen match.
	 */
	void SetHintBarPauseStrip(bool bStrip);

	/**
	 * Put a widget on the stage at spec coordinates: X and Y from the top-left of a 1920 x
	 * 1080 screen, in px. bFromBottom anchors it to the bottom edge instead (rows below the
	 * content area: the hint bar band, bottom-row tiles), so it stays put on taller screens.
	 * A width or height of 0 sizes the widget to its content on that axis.
	 */
	UWidget* Place(UWidget* Widget, float X, float Y, float Width, float Height, bool bFromBottom = false);
	/** Grid helpers: Place at column C (1-12) spanning N columns. */
	UWidget* PlaceInColumns(UWidget* Widget, int32 Column, int32 Span, float Y, float Height, bool bFromBottom = false);

	/** Create a focusable element of a class, give it an id and wire its events. */
	template <typename T>
	T* MakeElement(FName Id)
	{
		T* Element = WidgetTree->ConstructWidget<T>(T::StaticClass());
		RegisterElement(Element, Id);
		return Element;
	}
	void RegisterElement(UPadelFocusableWidget* Element, FName Id);

	/** Disabled elements are skipped by navigation; keep the map in step (spec 5.3). */
	void SetElementEnabled(FName Id, bool bEnabled);

	UPadelMenuRoot* GetMenuRoot() const { return MenuRoot; }
	IPadelMenuHost* GetHost() const;

	Padel::Ui::FFocusMap FocusMap;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> Stage;

private:
	void HandleElementActivated(UPadelFocusableWidget* Element);
	void HandleElementHovered(UPadelFocusableWidget* Element);
	void HandleElementValueChanged(UPadelFocusableWidget* Element);
	void ApplyEntryAnimation();

	EPadelScreen ScreenId = EPadelScreen::Title;
	FName FocusedId;
	bool bQuietFocus = false;
	Padel::Ui::FInputLock InputLock;
	Padel::Ui::EPadFamily PadFamily = Padel::Ui::EPadFamily::Xbox;
	Padel::Ui::EUiTransition EntryTransition = Padel::Ui::EUiTransition::None;
	FPadelTween EntryTween{1.0f, 1.0f};

	UPROPERTY()
	TObjectPtr<UPadelMenuRoot> MenuRoot;

	UPROPERTY()
	TObjectPtr<UPadelScreenHeader> Header;

	UPROPERTY()
	TObjectPtr<UPadelHintBar> HintBar;

	UPROPERTY()
	TMap<FName, TObjectPtr<UPadelFocusableWidget>> Elements;
};
