#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "UI/Model/PadelUiScore.h"
#include "PadelScoreBugWidget.generated.h"

class UHorizontalBox;
class UTextBlock;

/**
 * The compact corner score bug (GDD 10; GDD 1 pillar 4 "score, serve, side always
 * clear"): both pairs, a serve dot, games per set and the current points, plus a tag
 * for golden point, tiebreak and big points. Team A is always the top row.
 */
UCLASS()
class PADEL_API UPadelScoreBugWidget : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void UpdateScore(const Padel::Ui::FScoreView& View);

	/** For tests: what each row shows right now. */
	FString GetPointsText(int32 Team) const;
	FString GetGamesText(int32 Team) const;
	bool IsServing(int32 Team) const;
	FString GetTagText() const;

protected:
	virtual void BuildTree() override;

private:
	void FillSets(int32 Team, const std::vector<int>& Columns, int32 LiveIndex);

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ServeDots;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PointTexts;

	UPROPERTY()
	TArray<TObjectPtr<UHorizontalBox>> SetBoxes;

	UPROPERTY()
	TObjectPtr<UTextBlock> Tag;

	Padel::Ui::FScoreView Pending;
	bool bHasPending = false;
};
