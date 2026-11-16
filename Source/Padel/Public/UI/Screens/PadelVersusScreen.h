#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiQuickMatch.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "PadelVersusScreen.generated.h"

class UPadelLoadingIndicator;
class UPadelSkipArea;
class UTextBlock;
class UWidget;

/**
 * Spec 7.4, the Versus card, to the design boards "Versus" and "VersusLoading": a short
 * broadcast beat between Setup (or Rematch) and the first serve. Lagoon on the left, Ember
 * on the right, "VS" between them and the rules line under them; no header.
 *
 * The card moves on by itself after Input.VersusSeconds, or at once on A ("Skip"), and
 * then starts the match through the host. Both only happen once the match is ready; if it
 * isn't when the card's time is up, the card stays and the loading indicator shows after
 * Input.LoadingIndicatorAfter. Past Input.MatchStartTimeout it offers the way back to the
 * menu (spec 7.12). B does nothing here (spec 3).
 */
UCLASS()
class PADEL_API UPadelVersusScreen : public UPadelScreenWidget
{
	GENERATED_BODY()

public:
	/** Steps the card's clock (NativeTick calls it; tests call it directly). */
	void AdvanceCard(float DeltaSeconds);
	/** A or a click: start now, if the match is ready. */
	void Skip();
	bool HasStartedMatch() const { return Clock.bStarted; }
	bool IsLoadingShown() const;
	FText GetRulesLine() const;
	/** The pair line as drawn ("VENTARA · ARUNDE" before upper case), for tests. */
	FString GetPairText(int32 Team) const;

	virtual TArray<FPadelHint> GetHints() const override;

protected:
	virtual void BuildScreen() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnElementActivated(UPadelFocusableWidget* Element) override;

	/**
	 * Whether the match can start. There is no "match ready" signal yet (spec 11.4 W5), so
	 * this is always true and the card never waits. When W5 lands, this is the one place
	 * to read it.
	 */
	virtual bool IsMatchReady() const;

private:
	UWidget* MakeTeamBlock(int32 Team);
	/** The one place the card hands over to the match (swap in W5's start here). */
	void StartMatch();
	void ShowStartTimeout();
	void ApplyEntryMotion();
	void UpdateLoading();

	Padel::Ui::FVersusClock Clock;
	float EntryElapsed = 0.0f;
	bool bLoadingShown = false;

	UPROPERTY()
	TObjectPtr<UPadelSkipArea> SkipArea;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> TeamBlocks;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PairTexts;

	UPROPERTY()
	TObjectPtr<UTextBlock> VsText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RulesText;

	UPROPERTY()
	TObjectPtr<UWidget> LoadingBox;
};
