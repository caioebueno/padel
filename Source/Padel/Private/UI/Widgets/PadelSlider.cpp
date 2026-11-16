#include "UI/Widgets/PadelSlider.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	// Spec 6.10 and board "Settings": track 360 x 6, knob 24, value 32 px in a 64 px box,
	// 24 px between them.
	constexpr float SliderTrackWidth = 360.0f;
	constexpr float SliderTrackHeight = 6.0f;
	constexpr float SliderKnob = 24.0f;
	constexpr float SliderValuePx = 32.0f;
	constexpr float SliderValueWidth = 64.0f;
	constexpr float SliderValueGap = 24.0f;
	constexpr float SliderUnfocusedFillAlpha = 0.6f;

	void SliderPlace(UCanvasPanelSlot* CanvasSlot, float X, float Y, float W, float H)
	{
		if (CanvasSlot)
		{
			CanvasSlot->SetPosition(FVector2D(X, Y));
			CanvasSlot->SetSize(FVector2D(W, H));
		}
	}
}

void UPadelSlider::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	const Padel::Ui::FUiInputTiming& Input = PadelUi::Theme().Input;
	Model.StepSize = FMath::Max(1, FMath::RoundToInt32(Input.SliderStep));
	Model.FineStep = FMath::Max(1, FMath::RoundToInt32(Input.SliderFineStep));
	Model.FastAfter = Input.SliderFastAfter;
}

void UPadelSlider::SetRange(int32 InMin, int32 InMax)
{
	Model.Min = InMin;
	Model.Max = FMath::Max(InMin, InMax);
	Model.Set(Model.Value);
	RefreshVisuals();
}

void UPadelSlider::SetValue(int32 InValue)
{
	Model.Set(InValue);
	RefreshVisuals();
}

float UPadelSlider::GetHeldRepeatRate() const
{
	return PadelUi::Theme().Input.SliderRepeatRate;
}

bool UPadelSlider::HandleInput(EUiInput Input, bool bRepeat)
{
	if (IsDisabled() || (Input != EUiInput::Left && Input != EUiInput::Right))
	{
		return false;
	}
	const int32 Direction = Input == EUiInput::Right ? 1 : -1;
	const int32 Before = Model.Value;
	if (bRepeat)
	{
		Model.Repeat(Direction, HeldSeconds);
	}
	else
	{
		HeldSeconds = 0.0f;
		Model.Press(Direction);
	}
	// The edge sound only for a fresh press, so holding into the end stays quiet.
	Changed(Before, !bRepeat);
	return true;
}

void UPadelSlider::Changed(int32 Before, bool bEdgeIfSame)
{
	if (Model.Value != Before)
	{
		RefreshVisuals();
		OnValueChanged.Broadcast(this);
	}
	else if (bEdgeIfSame)
	{
		OnEdgeReached.Broadcast(this);
	}
}

void UPadelSlider::BuildValue(UHorizontalBox* Row)
{
	TrackCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	TrackBox = MakeSized(TrackCanvas, SliderTrackWidth, SliderKnob);
	Row->AddChildToHorizontalBox(TrackBox)->SetVerticalAlignment(VAlign_Center);

	const float Radius = PadelUi::Theme().Layout.RadiusPill;
	const float TrackY = (SliderKnob - SliderTrackHeight) * 0.5f;
	UImage* Track = MakeImage(PadelUiPaint::Rounded(PadelUi::Colour(EUiColour::GlassEdge), Radius));
	SliderPlace(TrackCanvas->AddChildToCanvas(Track), 0.0f, TrackY, SliderTrackWidth, SliderTrackHeight);
	TrackFill = MakeImage(PadelUiPaint::Rounded(FLinearColor::White, Radius));
	SliderPlace(TrackCanvas->AddChildToCanvas(TrackFill), 0.0f, TrackY, 0.0f, SliderTrackHeight);

	const float Blur = PadelUi::Theme().Layout.FocusGlowBlur;
	KnobGlow = MakeImage(PadelUiPaint::SoftBox(PadelUi::Colour(EUiColour::AccentGlow), Blur));
	SliderPlace(TrackCanvas->AddChildToCanvas(KnobGlow), 0.0f, 0.0f, SliderKnob + Blur, SliderKnob + Blur);
	Knob = MakeImage(PadelUiPaint::Rounded(PadelUi::Colour(EUiColour::TextPrimary), SliderKnob * 0.5f));
	SliderPlace(TrackCanvas->AddChildToCanvas(Knob), 0.0f, 0.0f, SliderKnob, SliderKnob);

	ValueText = MakeTokenText(FText::GetEmpty(), EUiType::Stat, EUiColour::TextSecondary);
	ValueText->SetFont(PadelUi::FontAtPx(EUiType::Stat, SliderValuePx));
	ValueText->SetJustification(ETextJustify::Right);
	UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(MakeSized(ValueText, SliderValueWidth, 0.0f));
	ValueSlot->SetVerticalAlignment(VAlign_Center);
	ValueSlot->SetPadding(FMargin(SliderValueGap, 0.0f, 0.0f, 0.0f));
}

void UPadelSlider::RefreshValue()
{
	if (!TrackFill || !Knob || !KnobGlow || !ValueText)
	{
		return;
	}
	const bool bDisabled = IsDisabled();
	const bool bFocused = IsElementFocused() && !bDisabled;
	const float FillWidth = SliderTrackWidth * Model.Fraction();
	const float TrackY = (SliderKnob - SliderTrackHeight) * 0.5f;
	SliderPlace(Cast<UCanvasPanelSlot>(TrackFill->Slot), 0.0f, TrackY, FillWidth, SliderTrackHeight);
	const FLinearColor Fill = bDisabled ? PadelUi::Colour(EUiColour::TextDisabled)
		: (bFocused ? PadelUi::Colour(EUiColour::Accent) : PadelUi::Colour(EUiColour::TextPrimary, SliderUnfocusedFillAlpha));
	TrackFill->SetColorAndOpacity(Fill);
	TrackFill->SetVisibility(FillWidth > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// The knob is centred on the fill's end (board: left = fill - 12).
	const float KnobX = FillWidth - SliderKnob * 0.5f;
	SliderPlace(Cast<UCanvasPanelSlot>(Knob->Slot), KnobX, 0.0f, SliderKnob, SliderKnob);
	Knob->SetColorAndOpacity(PadelUi::Colour(bDisabled ? EUiColour::TextDisabled : EUiColour::TextPrimary));
	const float Blur = PadelUi::Theme().Layout.FocusGlowBlur;
	SliderPlace(Cast<UCanvasPanelSlot>(KnobGlow->Slot), KnobX - Blur * 0.5f, -Blur * 0.5f, SliderKnob + Blur, SliderKnob + Blur);
	KnobGlow->SetVisibility(bFocused ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	ValueText->SetText(FText::AsNumber(Model.Value, &FNumberFormattingOptions::DefaultNoGrouping()));
	ValueText->SetColorAndOpacity(FSlateColor(PadelUi::Colour(
		bDisabled ? EUiColour::TextDisabled : (bFocused ? EUiColour::TextPrimary : EUiColour::TextSecondary))));
}

void UPadelSlider::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	HeldSeconds += InDeltaTime;
}

void UPadelSlider::SetFromPointer(const FPointerEvent& InMouseEvent)
{
	if (!TrackBox)
	{
		return;
	}
	const FGeometry& Track = TrackBox->GetCachedGeometry();
	const FVector2D Local = Track.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const float Width = FMath::Max(1.0f, static_cast<float>(Track.GetLocalSize().X));
	const float Fraction = FMath::Clamp(static_cast<float>(Local.X) / Width, 0.0f, 1.0f);
	const int32 Before = Model.Value;
	Model.Set(FMath::RoundToInt32(FMath::Lerp(static_cast<float>(Model.Min), static_cast<float>(Model.Max), Fraction)));
	Changed(Before, false);
}

FReply UPadelSlider::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || IsDisabled() || !TrackBox)
	{
		return Reply;
	}
	// Spec 6.10: a press on the track (or the knob hanging over its ends) grabs the knob.
	const FGeometry& Track = TrackBox->GetCachedGeometry();
	const FVector2D Local = Track.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const float Slack = SliderKnob * 0.5f;
	if (Local.X < -Slack || Local.X > Track.GetLocalSize().X + Slack || Local.Y < 0.0f || Local.Y > Track.GetLocalSize().Y)
	{
		return Reply;
	}
	bDragging = true;
	SetFromPointer(InMouseEvent);
	const TSharedPtr<SWidget> Cached = GetCachedWidget();
	return Cached.IsValid() ? FReply::Handled().CaptureMouse(Cached.ToSharedRef()) : FReply::Handled();
}

FReply UPadelSlider::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		SetFromPointer(InMouseEvent);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPadelSlider::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		return Reply.ReleaseMouseCapture();
	}
	return Reply;
}

FReply UPadelSlider::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Spec 5.6: the wheel steps a focused slider.
	if (!IsElementFocused() || IsDisabled() || FMath::IsNearlyZero(InMouseEvent.GetWheelDelta()))
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}
	const int32 Before = Model.Value;
	Model.Press(InMouseEvent.GetWheelDelta() > 0.0f ? 1 : -1);
	Changed(Before, true);
	return FReply::Handled();
}
