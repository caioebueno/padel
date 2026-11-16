#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/PadelFocusableWidget.h"
#include "PadelTitleCatcher.generated.h"

/**
 * The Title's only focus element (spec 7.1): clear, full screen and never drawn. It
 * takes a click anywhere and A like any element, and gives the screen's focus map its
 * default, so the Title follows the same screen contract as every other screen.
 */
UCLASS()
class PADEL_API UPadelTitleCatcher : public UPadelFocusableWidget
{
	GENERATED_BODY()

public:
	/** Swallows directions: the stick reaches here, and must neither continue nor bump an edge. */
	virtual bool HandleInput(Padel::Ui::EUiInput Input, bool bRepeat) override { return Padel::Ui::IsDirection(Input); }

protected:
	virtual void BuildTree() override;
	virtual float FocusedScale() const override { return 1.0f; }
	virtual float PressedScale() const override { return 1.0f; }
};
