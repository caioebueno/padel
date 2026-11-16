#pragma once

#include "CoreMinimal.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelPracticeScreen.generated.h"

class UPadelButton;
class UPadelPracticeControlList;

/**
 * Spec 7.9's Practice intro, board "Practice": a glass panel (C3-C10) with the title, one
 * line on the ball machine, its four controls and START PRACTICE. A starts the drills
 * through IPadelMenuHost::MenuStartPractice (the session itself is W9's); B goes back to
 * the main menu (DefaultBack). While drilling the HUD hides the menus, so this screen
 * only shows before the first ball.
 */
UCLASS()
class PADEL_API UPadelPracticeScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildScreen() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;
	virtual TArray<FPadelHint> GetHints() const override;

private:
	Padel::Ui::EPadFamily ShownFamily = Padel::Ui::EPadFamily::Xbox;

	UPROPERTY()
	TObjectPtr<UPadelButton> StartButton;

	UPROPERTY()
	TObjectPtr<UPadelPracticeControlList> Controls;
};
