#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiMatchStats.h"
#include "UI/PadelWidgetBase.h"
#include "PadelStatsScorePanel.generated.h"

class UCanvasPanel;
class UWidget;

/**
 * Spec 7.8's score panel on the Stats screen (board "Stats", y 184-400, C1-C12): one row
 * per team with its name in the team colour and the pair's surnames, then one column per
 * set (C7 onwards) in Type.Score, the set's winner in Text.Primary and the loser in
 * Text.Secondary, a raised tie-break score where the set had one, and a check after the
 * match winner. The set columns fade in one by one on entry (SetReveal).
 */
UCLASS()
class PADEL_API UPadelStatsScorePanel : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	/** Call before the panel is built. HumanPlayerId orders Team A's surnames (the human first). */
	void SetStats(const Padel::Ui::FMatchStats& InStats, int32 InHumanPlayerId);
	/** Opacity of each set column, 0..1 (the entry animation). */
	void SetColumnAlpha(int32 Column, float Alpha);
	int32 GetColumnCount() const { return Columns.Num(); }

protected:
	virtual void BuildTree() override;

private:
	void BuildTeamRow(UCanvasPanel* Canvas, int32 Team);
	UWidget* MakeSetCell(int32 Set, int32 Team);

	Padel::Ui::FMatchStats Stats;
	int32 HumanPlayerId = 1;

	/** Per set column, both teams' cells. */
	TArray<TArray<TObjectPtr<UWidget>>> Columns;

	/** Keeps the column cells alive for SetColumnAlpha (Columns isn't a UPROPERTY). */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> ColumnCells;
};
