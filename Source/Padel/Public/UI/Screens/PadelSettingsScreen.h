#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiSettings.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelSettingsScreen.generated.h"

class UPadelTabBar;
class UTextBlock;
class UVerticalBox;

/**
 * Spec 7.7, boards "Settings" and "SettingsAccess": the few settings the slice needs, on
 * an AUDIO and an ACCESSIBILITY tab. A glass list on the left (C1-C7) with one row per
 * setting (slider, selector or toggle); the focused row's name and description in a panel
 * on the right (C8-C12).
 *
 * Every change applies at once and is saved (UPadelSettings) when B leaves the screen.
 * X resets the tab after Dlg.ResetTab; its hint shows only while something on the tab
 * isn't default. LB/RB change tab and focus its first row. Text size, Reduce motion and
 * Reduce transparency go through PadelUi::SetOptions, which makes the menu root rebuild
 * this screen: the tab and the focused row survive it.
 *
 * Opened from Pause it shows "PAUSE / SETTINGS" and the Pause scrim and blur.
 */
UCLASS()
class PADEL_API UPadelSettingsScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

public:
	Padel::Ui::ESettingsTab GetTab() const { return Tab; }
	const Padel::Ui::FSettingsValues& GetValues() const { return Values; }
	/** The element id of a setting's row ("Set.Vol.Master"). */
	static FName RowId(Padel::Ui::ESetting Setting);

	virtual TArray<FPadelHint> GetHints() const override;
	virtual Padel::Ui::EUiColour GetScrimColour() const override;
	virtual bool WantsBackgroundBlur() const override;

protected:
	virtual void BuildScreen() override;
	virtual void OnElementValueChanged(UPadelFocusableWidget* Element) override;
	virtual void OnFocusChanged(UPadelFocusableWidget* Element) override;
	virtual bool OnBack() override;
	virtual bool OnScreenInput(Padel::Ui::EUiInput Input, bool bRepeat) override;

private:
	void BuildRow(Padel::Ui::ESetting Setting, UVerticalBox* List);
	/** Show a tab's rows and focus map; bFocusFirst moves focus to its first row. */
	void ShowTab(Padel::Ui::ESettingsTab NewTab, bool bFocusFirst);
	void HandleTabChanged(int32 NewIndex);
	/** Hand the values to UPadelSettings, which applies them (this may rebuild the screen). */
	void ApplyValues();
	/** Set every row's control from Values (after a reset). */
	void SyncRows();
	void ConfirmResetTab();
	void UpdateDescription(UPadelFocusableWidget* Element);
	bool FindSetting(FName Id, Padel::Ui::ESetting& Out) const;
	bool OpenedFromPause() const;

	static FText SettingLabel(Padel::Ui::ESetting Setting);
	static FText SettingDescription(Padel::Ui::ESetting Setting);
	static FText TabLabel(Padel::Ui::ESettingsTab InTab);

	Padel::Ui::FSettingsValues Values;
	Padel::Ui::ESettingsTab Tab = Padel::Ui::ESettingsTab::Audio;
	/** Spec 7.7: the Menu sounds row only once menu sounds exist (something listens to the root's cues). */
	bool bMenuSounds = false;

	UPROPERTY()
	TObjectPtr<UPadelTabBar> TabBar;

	UPROPERTY()
	TArray<TObjectPtr<UVerticalBox>> TabLists;

	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> DescriptionBody;
};
