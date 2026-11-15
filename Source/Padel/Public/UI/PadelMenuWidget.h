#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "PadelMenuWidget.generated.h"

class UPadelMenuButtonRelay;
class UTextBlock;
class UVerticalBox;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelMenuOption, int32 /*Index*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPadelMenuCycle, int32 /*Index*/, int32 /*Step*/);
DECLARE_MULTICAST_DELEGATE(FOnPadelMenuBack);

/**
 * One full-screen menu layout used for every screen in the slice: main menu, match
 * setup, pause and the stats screen. A title, optional body text and table, and a
 * column of big buttons. Fully usable with a gamepad (GDD 12 item 1): the D-pad or
 * stick moves focus, A presses, left/right cycles a setup option, B goes back.
 */
UCLASS()
class PADEL_API UPadelMenuWidget : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void SetTitle(const FText& Title);
	void SetBody(const FText& Body);
	/** Rows of cells; the first row is the header. Empty hides the table. */
	void SetTable(const TArray<TArray<FText>>& Rows);
	void SetOptions(const TArray<FText>& Labels);
	void SetOptionLabel(int32 Index, const FText& Label);
	void FocusOption(int32 Index);
	int32 GetFocusedOption() const;
	int32 GetOptionCount() const { return FocusOrder.Num(); }
	FString GetOptionLabel(int32 Index) const;
	FString GetTitle() const;

	/** Same as pressing the button; used by the relay and by tests. */
	void ChooseOption(int32 Index);

	FOnPadelMenuOption OnOptionChosen;
	FOnPadelMenuCycle OnOptionCycled;
	FOnPadelMenuBack OnBack;

protected:
	virtual void BuildTree() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> TableBox;

	UPROPERTY()
	TObjectPtr<UVerticalBox> OptionsBox;

	UPROPERTY()
	TArray<TObjectPtr<UPadelMenuButtonRelay>> Relays;
};
