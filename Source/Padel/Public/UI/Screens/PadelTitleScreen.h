#pragma once

#include "CoreMinimal.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelTitleScreen.generated.h"

class UImage;
class UTextBlock;

/**
 * Spec 7.1, boards "Title" and "TitleKey": the working-title wordmark, the tagline and
 * "Press any button" (or "Press any key" with no pad), pulsing. Any key or button
 * continues, but not the sticks or mouse movement; a click anywhere does too. There is
 * no B and no hint bar. The root already knows the device from the press (spec 5.5).
 *
 * The screen has one focus element, Title.Continue, a clear full-screen catcher: it
 * gives the focus map a default (spec 5.3), takes A and clicks, and is never drawn.
 */
UCLASS()
class PADEL_API UPadelTitleScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

public:
	/** Any button: on to the main menu. */
	void Continue();
	bool HasContinued() const { return bLeaving; }
	/** The prompt's text right now (tests). */
	FText GetPromptText() const;
	/** Advance the title's own timers (NativeTick; tests drive it directly). */
	void AdvanceTitle(float DeltaSeconds);

protected:
	virtual void BuildScreen() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;
	virtual bool OnBack() override;
	virtual bool OnScreenInput(Padel::Ui::EUiInput Input, bool bRepeat) override;
	virtual bool OnAnyKey(const FString& KeyName) override;
	virtual TArray<FPadelHint> GetHints() const override;
	virtual Padel::Ui::EUiColour GetScrimColour() const override { return Padel::Ui::EUiColour::BgScrimTitle; }

private:
	bool ShowsPadPrompt() const;
	void ApplyPrompt();

	float Seconds = 0.0f;
	bool bLeaving = false;
	bool bPadPrompt = false;

	UPROPERTY()
	TObjectPtr<UTextBlock> Prompt;

	UPROPERTY()
	TObjectPtr<UImage> Underline;
};
