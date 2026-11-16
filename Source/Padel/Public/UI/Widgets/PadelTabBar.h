#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiControls.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/PadelWidgetBase.h"
#include "PadelTabBar.generated.h"

class UHorizontalBox;
class UImage;
class UPadelButtonGlyph;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelTabChanged, int32 /*NewIndex*/);

/**
 * Spec 6.11: tab labels under the screen title in Type.Heading, 48 px apart, between the
 * LB and RB glyphs. The selected tab is Text.Primary with a 4 px Accent underline. Tabs
 * aren't in the focus map: LB/RB (Q/E) or a click change them, without wrapping. The
 * screen resets focus to the new tab's first element on OnTabChanged.
 */
UCLASS()
class PADEL_API UPadelTabBar : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void SetTabs(const TArray<FText>& InTabs);
	void SetIndex(int32 InIndex);
	int32 GetIndex() const { return Model.Index; }
	void SetFamily(Padel::Ui::EPadFamily InFamily);
	/** TabLeft or TabRight. True if the tab changed (false at an end, or for other input). */
	bool HandleInput(Padel::Ui::EUiInput Input);

	FOnPadelTabChanged OnTabChanged;

protected:
	virtual void BuildTree() override;
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void Rebuild();
	void Refresh();
	void Select(int32 InIndex);

	Padel::Ui::FTabModel Model;
	TArray<FText> Tabs;
	Padel::Ui::EPadFamily Family = Padel::Ui::EPadFamily::Xbox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> Row;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> TabRow;

	UPROPERTY()
	TObjectPtr<UPadelButtonGlyph> LeftGlyph;

	UPROPERTY()
	TObjectPtr<UPadelButtonGlyph> RightGlyph;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TabLabels;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> Underlines;
};
