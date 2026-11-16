#pragma once

#include "CoreMinimal.h"
#include "UI/Model/PadelUiControls.h"
#include "UI/Widgets/PadelRowControl.h"
#include "PadelSlider.generated.h"

class UCanvasPanel;
class UHorizontalBox;
class UImage;
class USizeBox;
class UTextBlock;

/**
 * Spec 6.10: a 72 px settings row with a 360 x 6 px track, a 24 px knob and the value
 * ("0" to "100") in a 64 px box on the right (board "Settings"). The filled part is Accent
 * when focused and Text.Primary at 0.6 otherwise; the focused knob glows.
 *
 * Left/right step by Input.SliderStep (5); held, they repeat every Input.SliderRepeatRate
 * by Input.SliderFineStep (1), then by the full step after Input.SliderFastAfter (spec
 * 5.3). A does nothing. The mouse drags the knob, a click on the track jumps there, and
 * the wheel steps a focused slider (5.6).
 */
UCLASS()
class PADEL_API UPadelSlider : public UPadelRowControl
{
	GENERATED_BODY()

public:
	void SetRange(int32 InMin, int32 InMax);
	void SetValue(int32 InValue);
	int32 GetValue() const { return Model.Value; }

	virtual bool HandleInput(Padel::Ui::EUiInput Input, bool bRepeat) override;
	/** Spec 7.7: A does nothing on a slider. */
	virtual void Activate() override {}
	virtual float GetHeldRepeatRate() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void BuildValue(UHorizontalBox* Row) override;
	virtual void RefreshValue() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnClicked(const FPointerEvent& InMouseEvent) override {}

private:
	/** Set from a pointer's position along the track. */
	void SetFromPointer(const FPointerEvent& InMouseEvent);
	void Changed(int32 Before, bool bEdgeIfSame);

	Padel::Ui::FSliderModel Model;
	/** Seconds since the last fresh left/right press: the repeat speeds up after FastAfter. */
	float HeldSeconds = 0.0f;
	bool bDragging = false;

	UPROPERTY()
	TObjectPtr<USizeBox> TrackBox;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> TrackCanvas;

	UPROPERTY()
	TObjectPtr<UImage> TrackFill;

	UPROPERTY()
	TObjectPtr<UImage> Knob;

	UPROPERTY()
	TObjectPtr<UImage> KnobGlow;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;
};
