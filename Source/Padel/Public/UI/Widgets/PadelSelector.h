#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiControls.h"
#include "UI/Widgets/PadelRowControl.h"
#include "PadelSelector.generated.h"

class UHorizontalBox;
class UPadelIcon;
class USizeBox;
class UTextBlock;

/**
 * Spec 6.8: the left/right value picker ("Difficulty ‹ MEDIUM ›"). Left/right step with
 * no wrap (that arrow greys at an end); A steps forward and wraps. Dots under the value
 * for 2 to 5 options. Each option may carry its own description line, shown while focused.
 */
UCLASS()
class PADEL_API UPadelSelector : public UPadelRowControl
{
	GENERATED_BODY()

public:
	/** Options and their optional description lines (Descriptions may be shorter or empty). */
	void SetOptions(const TArray<FText>& InOptions, const TArray<FText>& InDescriptions = TArray<FText>());
	void SetIndex(int32 InIndex);
	int32 GetIndex() const { return Model.Index; }
	int32 NumOptions() const { return Model.Count; }
	FText GetValueText() const;

	virtual bool HandleInput(Padel::Ui::EUiInput Input, bool bRepeat) override;
	virtual void Activate() override;

protected:
	virtual void BuildValue(UHorizontalBox* Row) override;
	virtual void RefreshValue() override;
	virtual FText CurrentDescription() const override;
	virtual float RowRightPadding() const override { return 16.0f; }
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnClicked(const FPointerEvent& InMouseEvent) override;

private:
	/** Step by Direction; Wrap for A. Plays the slide, or the edge event at an end. */
	void StepValue(int32 Direction, bool bWrap, bool bRepeat);
	void RebuildDots();

	Padel::Ui::FSelectorModel Model;
	TArray<FText> Options;
	TArray<FText> Descriptions;
	/** -1 or +1: the side the last value slid in from (spec 9: 24 px, 140 ms). */
	float SlideFrom = 0.0f;
	FPadelTween SlideTween;
	float LeftPressTimer = 0.0f;
	float RightPressTimer = 0.0f;

	UPROPERTY()
	TObjectPtr<UPadelIcon> LeftArrow;

	UPROPERTY()
	TObjectPtr<USizeBox> LeftArrowBox;

	UPROPERTY()
	TObjectPtr<UPadelIcon> RightArrow;

	UPROPERTY()
	TObjectPtr<USizeBox> RightArrowBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Dots;
};
