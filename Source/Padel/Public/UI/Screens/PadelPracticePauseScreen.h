#pragma once

#include "CoreMinimal.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelPracticePauseScreen.generated.h"

class UPadelPracticeControlList;
class UTextBlock;

/**
 * Spec 7.9's Practice pause, board "PracticePause": the Pause layout (7.5) with RESUME,
 * CONTROLS, SETTINGS and QUIT PRACTICE on the left, the practice controls on the right,
 * and the current drill's name in the header instead of a score. Quit practice needs no
 * confirm (nothing is lost). B and Start resume, over the frozen court with the blur.
 */
UCLASS()
class PADEL_API UPadelPracticePauseScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;
	virtual Padel::Ui::EUiColour GetScrimColour() const override { return Padel::Ui::EUiColour::BgScrimPause; }
	virtual bool WantsBackgroundBlur() const override { return true; }

private:
	Padel::Ui::EPadFamily ShownFamily = Padel::Ui::EPadFamily::Xbox;

	UPROPERTY()
	TObjectPtr<UPadelPracticeControlList> Controls;

	UPROPERTY()
	TObjectPtr<UTextBlock> DrillName;
};
