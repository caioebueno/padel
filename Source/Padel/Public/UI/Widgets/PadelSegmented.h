#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiControls.h"
#include "UI/Widgets/PadelRowControl.h"
#include "PadelSegmented.generated.h"

class UBorder;
class UHorizontalBox;
class UTextBlock;

/**
 * Spec 6.9: two or three short options side by side ("1 SET | BEST OF 3") in a 56 px pill,
 * as a labelled row. Focus rings the whole pill; left/right select directly, no wrap,
 * and A does nothing. A click picks the segment under the cursor.
 */
UCLASS()
class PADEL_API UPadelSegmented : public UPadelRowControl
{
	GENERATED_BODY()

public:
	void SetOptions(const TArray<FText>& InOptions);
	void SetIndex(int32 InIndex);
	int32 GetIndex() const { return Model.Index; }

	virtual bool HandleInput(Padel::Ui::EUiInput Input, bool bRepeat) override;
	virtual void Activate() override {}

protected:
	virtual void BuildValue(UHorizontalBox* Row) override;
	virtual void RefreshValue() override;
	virtual bool ShowsRowFocus() const override { return false; }
	virtual float RowRightPadding() const override { return 8.0f; }
	virtual float FocusedScale() const override { return 1.0f; }
	virtual void OnClicked(const FPointerEvent& InMouseEvent) override;

private:
	void RebuildSegments();
	void Select(int32 InIndex);

	Padel::Ui::FSegmentedModel Model;
	TArray<FText> Options;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Segments;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SegmentFills;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SegmentLabels;
};
