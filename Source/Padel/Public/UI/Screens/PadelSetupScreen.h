#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelSetupScreen.generated.h"

class UPadelButton;
class UPadelPlayerCard;
class UPadelSegmented;
class UPadelSelector;
class UPadelToggle;

/**
 * Spec 7.3, Quick Match setup, to the design boards "Setup", "SetupDifficulty" and
 * "SetupSwapped". The matchup is the hero: your pair's two large cards on the left, the
 * opponents' small cards top right, and the match rules (Difficulty, Format, Golden point)
 * over Start match bottom right.
 *
 * It edits the host's FMatchSetup (kept for the session). Focus is one wrapping list of the
 * three rules and Start, defaulting to Start so the defaults play with one press. X (or a
 * click on a large card) swaps which Lagoon player you control from anywhere on the
 * screen; the cards stay put and their badges and frames cross over. Start match goes to
 * the Versus card; B goes back to the main menu (the base's Back).
 */
UCLASS()
class PADEL_API UPadelSetupScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

public:
	/** X's action: swap the human to the other Lagoon player. */
	void SwapPlayer();
	/** The setup this screen edits: the host's, or a local one with no host (tests). */
	Padel::Ui::FMatchSetup& Setup();

	UPadelPlayerCard* GetPairCard(int32 Index) const;
	UPadelPlayerCard* GetOpponentCard(int32 Index) const;

	virtual TArray<FPadelHint> GetHints() const override;

protected:
	virtual void BuildScreen() override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;
	virtual void OnElementValueChanged(UPadelFocusableWidget* Element) override;
	virtual bool OnScreenInput(Padel::Ui::EUiInput Input, bool bRepeat) override;

private:
	void BuildHeaderMatchup();
	void BuildPairPanel();
	void BuildOpponentsPanel();
	void BuildRulesPanel();
	UPadelPlayerCard* MakeCard(int32 PlayerId, bool bLarge);
	void UpdateCardRoles(bool bAnimate);
	void UpdateDescriptions();

	Padel::Ui::FMatchSetup LocalSetup;

	UPROPERTY()
	TArray<TObjectPtr<UPadelPlayerCard>> PairCards;

	UPROPERTY()
	TArray<TObjectPtr<UPadelPlayerCard>> OpponentCards;

	UPROPERTY()
	TObjectPtr<UPadelSelector> Difficulty;

	UPROPERTY()
	TObjectPtr<UPadelSegmented> Format;

	UPROPERTY()
	TObjectPtr<UPadelToggle> GoldenPoint;

	UPROPERTY()
	TObjectPtr<UPadelButton> StartButton;
};
