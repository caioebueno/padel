#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiStatsView.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelStatsScreen.generated.h"

class UPadelButton;
class UPadelStatsScorePanel;
class UPadelStatsTable;

/**
 * Spec 7.8, boards "Stats" and "StatsLocked": the result and how it happened. "{TEAM}
 * WIN" in the winner's colour, the score panel (sets, tie-breaks, a check for the
 * winner), the stats panel with SUMMARY / SHOTS / POINTS tabs on LB/RB, the footer line,
 * and Rematch (Versus) and Main menu.
 *
 * B does nothing (spec 3), so a stray press can't throw the stats away. The stray-press
 * lock: A is also the shot button, so Confirm stays shut until the entry animation has
 * finished and Input.StatsConfirmLockSeconds have passed; an A during the animation only
 * finishes it. The buttons show their Disabled look and the A hint is gone meanwhile.
 */
UCLASS()
class PADEL_API UPadelStatsScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

public:
	virtual TArray<FPadelHint> GetHints() const override;
	virtual float GetConfirmLockSeconds() const override;
	virtual bool IsConfirmBlocked() const override { return Gate.IsLocked(); }

	/** Time passing for the entry animation and the lock (NativeTick; tests call it directly). */
	void AdvanceTime(float DeltaSeconds);
	bool IsConfirmLocked() const { return Gate.IsLocked(); }
	bool IsAnimating() const { return Gate.IsAnimating(); }
	UPadelStatsTable* GetTable() const { return Table; }

	static const FName RematchId;
	static const FName MainMenuId;

protected:
	virtual void BuildScreen() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;
	virtual bool OnBack() override { return false; }
	virtual bool OnScreenInput(Padel::Ui::EUiInput Input, bool bRepeat) override;
	virtual bool OnBlockedConfirm() override;

private:
	void ApplyAnimation();
	void ApplyLockLook();

	Padel::Ui::FStatsConfirmGate Gate;
	Padel::Ui::FStatsTimeline Timeline;
	bool bShownLocked = false;

	UPROPERTY()
	TObjectPtr<UPadelStatsScorePanel> ScorePanel;

	UPROPERTY()
	TObjectPtr<UPadelStatsTable> Table;

	UPROPERTY()
	TObjectPtr<UPadelButton> Rematch;

	UPROPERTY()
	TObjectPtr<UPadelButton> MainMenu;
};
