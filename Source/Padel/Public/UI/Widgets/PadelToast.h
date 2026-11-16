#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "PadelToast.generated.h"

class UTextBlock;

/**
 * Spec 6.13: a 480 px glass note, bottom-centre above the hint bar, one line of
 * Type.BodySmall. Shows for 2.5 s and fades over 200 ms. Not focusable; a new one
 * replaces the old. The menu root places it.
 */
UCLASS()
class PADEL_API UPadelToast : public UPadelWidgetBase
{
	GENERATED_BODY()

public:
	void Show(const FText& InMessage);
	bool IsShowing() const { return Remaining > 0.0f; }
	FText GetMessage() const { return Message; }

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FText Message;
	float Remaining = 0.0f;

	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;
};
