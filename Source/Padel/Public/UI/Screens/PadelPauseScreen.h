#pragma once

#include "CoreMinimal.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelPauseScreen.generated.h"

class UHorizontalBox;
class UWidget;

/**
 * Spec 7.5, boards "Pause", "PauseRestart" and "PauseQuit": the match frozen behind a
 * 0.70 scrim and the one blur layer, "PAUSED" with the live score line on the right, a
 * vertical list (Resume, Restart match, Controls, Settings, Quit to main menu) and a
 * context panel that says what the focused item does.
 *
 * B and Start resume (the default Back and the root). Restart and Quit ask first with
 * spec 7.10's dialogs: Restart goes through Versus, Quit drops the match without stats.
 */
UCLASS()
class PADEL_API UPadelPauseScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

public:
	virtual TArray<FPadelHint> GetHints() const override;
	virtual Padel::Ui::EUiColour GetScrimColour() const override { return Padel::Ui::EUiColour::BgScrimPause; }
	virtual bool WantsBackgroundBlur() const override { return true; }

	/** Which context panel shows (tests). */
	int32 GetContextIndex() const { return ContextIndex; }

	static const FName ResumeId;
	static const FName RestartId;
	static const FName ControlsId;
	static const FName SettingsId;
	static const FName QuitId;

protected:
	virtual void BuildScreen() override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;
	virtual void OnFocusChanged(UPadelFocusableWidget* Element) override;

private:
	UWidget* BuildScoreLine();
	UWidget* BuildRulesContext();
	UWidget* BuildTextContext(const FText& Text);
	void ShowContext(int32 Index);
	void ConfirmRestart();
	void ConfirmQuit();

	int32 ContextIndex = 0;

	/** One per Padel::Ui::EPauseContext, in its order. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> Contexts;
};
