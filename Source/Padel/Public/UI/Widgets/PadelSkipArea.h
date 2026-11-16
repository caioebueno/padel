#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "PadelSkipArea.generated.h"

/**
 * An invisible focusable that covers a screen with nothing else to press, so A, Enter and
 * a click anywhere reach the screen as one element (spec 5.1: Skip is A, Enter or a click).
 * The Versus card uses it for "Skip". It draws nothing in any state.
 */
UCLASS()
class PADEL_API UPadelSkipArea : public UPadelFocusableWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual float FocusedScale() const override { return 1.0f; }
	virtual float PressedScale() const override { return 1.0f; }
};
