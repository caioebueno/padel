#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "PadelScreenHeader.generated.h"

class UNamedSlot;
class USizeBox;
class UTextBlock;

/**
 * Spec 6.14: the 96 px header band. The screen title in Type.Display on the left, an
 * optional breadcrumb above it in Type.Label ("PAUSE / SETTINGS"), and a right-hand slot
 * (the Pause screen's score line; nothing elsewhere in the slice).
 */
UCLASS()
class PADEL_API UPadelScreenHeader : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void SetTitle(const FText& InTitle);
	void SetBreadcrumb(const FText& InBreadcrumb);
	void SetRightContent(UWidget* Content);

	FText GetTitle() const { return Title; }

protected:
	virtual void BuildTree() override;

private:
	void Refresh();

	FText Title;
	FText Breadcrumb;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BreadcrumbText;

	UPROPERTY()
	TObjectPtr<UNamedSlot> RightSlot;
};
