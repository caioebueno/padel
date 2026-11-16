#pragma once

#include "CoreMinimal.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelPlaceholderScreen.generated.h"

/**
 * Stands in for a screen that isn't built yet: the screen's name as the header and a list
 * of its exits, so the whole flow of spec 3 can be walked with a pad from the first day.
 * It is also the smallest example of a UPadelScreenWidget.
 */
UCLASS()
class PADEL_API UPadelPlaceholderScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;
	virtual Padel::Ui::EUiColour GetScrimColour() const override;
	virtual bool WantsBackgroundBlur() const override;

private:
	struct FExit
	{
		FName Id;
		FText Label;
		TFunction<void()> Action;
	};

	TArray<FExit> MakeExits();

	TArray<FExit> Exits;
};
