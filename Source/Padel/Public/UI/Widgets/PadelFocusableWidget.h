#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/PadelWidgetBase.h"
#include "PadelFocusableWidget.generated.h"

class UBorder;
class UImage;
class UOverlay;
class UPadelFocusableWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelElementEvent, UPadelFocusableWidget* /*Element*/);

/** A small eased value for C++-driven motion (spec 9: no Widget Blueprint timelines). */
struct FPadelTween
{
	float From = 0.0f;
	float To = 0.0f;
	float Duration = 0.0f;
	float Elapsed = 0.0f;

	void Snap(float Value) { From = To = Value; Duration = 0.0f; Elapsed = 0.0f; }
	void Start(float Target, float Seconds);
	void Tick(float DeltaSeconds) { Elapsed += DeltaSeconds; }
	float Value() const;
	bool IsDone() const { return Elapsed >= Duration; }
};

/**
 * Base for everything a player can focus in the menus (spec 6, "state vocabulary"):
 * buttons, tiles, toggles, selectors. Focus is virtual: the screen owns which element is
 * focused (through its FFocusMap) and tells the element; Slate's own keyboard focus stays
 * on the menu root. Mouse hover asks the screen for focus (hover = focus, spec 5.6).
 */
UCLASS(Abstract)
class PADEL_API UPadelFocusableWidget : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	/** The element's id in its screen's focus map, e.g. "Setup.Start". */
	void SetElementId(FName InId) { ElementId = InId; }
	FName GetElementId() const { return ElementId; }

	void SetElementFocused(bool bFocused);
	bool IsElementFocused() const { return bIsFocused; }
	/** Disabled: skipped by navigation, can't be activated (spec 6). */
	void SetDisabled(bool bInDisabled);
	bool IsDisabled() const { return bIsDisabled; }
	/** Locked: focusable to explain itself; A makes the screen show a toast (spec 5.3). */
	void SetLocked(bool bInLocked);
	bool IsLocked() const { return bIsLocked; }
	void SetSelected(bool bInSelected);
	bool IsSelected() const { return bIsSelected; }

	/** Input while focused. True if the element used it (left/right on a value control). */
	virtual bool HandleInput(Padel::Ui::EUiInput Input, bool bRepeat) { return false; }
	/** A or a click: plays the press, then tells the screen (also when locked). */
	virtual void Activate();
	/**
	 * Seconds between repeats while left or right is held on this element, or 0 for the
	 * menu's usual rate. The slider repeats faster (spec 5.3: every 60 ms).
	 */
	virtual float GetHeldRepeatRate() const { return 0.0f; }

	FOnPadelElementEvent OnActivated;
	/** Mouse hover: the screen moves focus here. */
	FOnPadelElementEvent OnHovered;
	/** A value control changed its value. */
	FOnPadelElementEvent OnValueChanged;
	/** A value control was pushed past its end (spec 6.7, 6.8: the edge sound). */
	FOnPadelElementEvent OnEdgeReached;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** A left click released over the element. Activates it; a selector steps by arrow instead. */
	virtual void OnClicked(const FPointerEvent& InMouseEvent);

	/** Apply the current state flags to the widget tree. Called on every state change. */
	virtual void RefreshVisuals() {}
	/** Scale when focused: 1.03 for tiles, cards and buttons; list rows slide instead (1.0). */
	virtual float FocusedScale() const;
	virtual float PressedScale() const;

	/** True from a press until Motion.Press (and at least Input.PressMinimum) has passed. */
	bool IsPressedVisual() const { return PressTimer > 0.0f || bPointerDown; }

	/**
	 * The focus ring of spec 4.3 around Target's box: 3 px Accent outside the edge with a
	 * 2 px gap, matching Radius, plus the Accent.Glow halo. Added to Root behind or over
	 * the content; ShowFocusRing toggles it.
	 */
	void AddFocusRing(UOverlay* Root, float Radius);
	void ShowFocusRing(bool bShow);

	UPROPERTY()
	TObjectPtr<UBorder> FocusRing;

	UPROPERTY()
	TObjectPtr<UImage> FocusGlow;

private:
	void ApplyScale();

	FName ElementId;
	bool bIsFocused = false;
	bool bIsDisabled = false;
	bool bIsLocked = false;
	bool bIsSelected = false;
	bool bPointerDown = false;
	float PressTimer = 0.0f;
	FPadelTween ScaleTween{1.0f, 1.0f};
};
