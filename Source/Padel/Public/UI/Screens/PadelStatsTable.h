#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiMatchStats.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/Model/PadelUiStatsView.h"
#include "UI/PadelWidgetBase.h"
#include "PadelStatsTable.generated.h"

class UCanvasPanel;
class UImage;
class UPadelTabBar;
class UScrollBox;
class UVerticalBox;

/**
 * Spec 7.8's stats panel on the Stats screen (board "Stats", y 424-848, C1-C12): the
 * SUMMARY / SHOTS / POINTS tabs (LB/RB), the team labels, and one row per stat: label,
 * Team A's number right-aligned at C5, a comparison bar growing out from the centre of
 * C6-C9 in each team's colour (scaled to the larger number), and Team B's number at C10.
 * The leading number is Text.Primary and the other Text.Secondary, flipped where lower is
 * better. More rows than fit scroll (mouse wheel).
 */
UCLASS()
class PADEL_API UPadelStatsTable : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	/** Call before the table is built. */
	void SetStats(const Padel::Ui::FMatchStats& InStats);
	void SetFamily(Padel::Ui::EPadFamily InFamily);
	/** LB / RB. True if the tab changed. */
	bool HandleTabInput(Padel::Ui::EUiInput Input);

	Padel::Ui::EStatsTab GetTab() const { return Tab; }
	int32 GetRowCount() const { return RowsA.Num(); }
	/** 0..1 growth of a row's bars (the entry animation); eased here. */
	void SetRowProgress(int32 Row, float Progress);
	/** All bars at full length (after the animation, and on a tab change). */
	void ShowAllBars();

protected:
	virtual void BuildTree() override;

private:
	void SelectTab(int32 Index);
	void RebuildRows();
	void AddRow(const Padel::Ui::FStatRow& Row);
	FText RowLabel(const Padel::Ui::FStatRow& Row) const;

	Padel::Ui::FMatchStats Stats;
	Padel::Ui::EStatsTab Tab = Padel::Ui::EStatsTab::Summary;
	Padel::Ui::EPadFamily Family = Padel::Ui::EPadFamily::Xbox;

	/** Each row's full bar fraction per team, parallel to the bar images. */
	TArray<float> FractionsA;
	TArray<float> FractionsB;

	UPROPERTY()
	TObjectPtr<UPadelTabBar> TabBar;

	UPROPERTY()
	TObjectPtr<UScrollBox> Scroll;

	UPROPERTY()
	TObjectPtr<UVerticalBox> RowList;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> RowsA;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> RowsB;
};
