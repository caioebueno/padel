#pragma once

#include "CoreMinimal.h"
#include "UI/PadelWidgetBase.h"
#include "PadelLoadingIndicator.generated.h"

class UImage;

/**
 * Spec 6.16's loading indicator, to the "VersusLoading" board: a 20 px ball bouncing on a
 * short line, one bounce every Motion.LoadingLoop, then "Loading" in Type.Label. The
 * screen decides when it shows (only after a wait, Input.LoadingIndicatorAfter). Reduce
 * motion holds the ball still.
 */
UCLASS()
class PADEL_API UPadelLoadingIndicator : public UPadelWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float Phase = 0.0f;

	UPROPERTY()
	TObjectPtr<UImage> Ball;
};
