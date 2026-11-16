#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/PadelScreenTypes.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "PadelDialog.generated.h"

class UBorder;
class UImage;
class UPadelButton;
class UTextBlock;

/**
 * Spec 6.12 and 7.10: the confirm dialog, over everything with its own scrim. A 6-column
 * (852 px) Surface.Charcoal panel: title, body, then the action button and the safe one
 * side by side, right-aligned. The safe option is on the right and focused when it opens;
 * B always picks it; Start is ignored. Opens in 160 ms (fade, 0.96 -> 1.0), closes in 100.
 *
 * The menu root owns it as a layer: it routes input here while IsOpen().
 */
UCLASS()
class PADEL_API UPadelDialog : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void Open(const FPadelConfirm& InConfirm);
	/** Close without choosing (the owner tears the screen down). Neither callback runs. */
	void Dismiss();
	bool IsOpen() const { return bOpen; }

	/** Menu input while open. Always consumed: nothing reaches the screen underneath. */
	void HandleInput(Padel::Ui::EUiInput Input);

	/** True when the safe option has focus (tests: spec 7.10's "opens on the safe option"). */
	bool IsSafeFocused() const { return bSafeFocused; }
	FText GetTitle() const { return Confirm.Title; }

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void ApplyConfirm();
	void Choose(bool bSafe);
	void SetSafeFocused(bool bSafe);
	void OnButtonActivated(UPadelFocusableWidget* Element);
	void OnButtonHovered(UPadelFocusableWidget* Element);

	FPadelConfirm Confirm;
	bool bOpen = false;
	bool bSafeFocused = true;
	/** Fading out after a choice; input is ignored. */
	bool bClosing = false;
	FPadelTween Fade;

	UPROPERTY()
	TObjectPtr<UImage> Scrim;

	UPROPERTY()
	TObjectPtr<UWidget> Panel;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY()
	TObjectPtr<UPadelButton> ActionButton;

	UPROPERTY()
	TObjectPtr<UPadelButton> SafeButton;
};
