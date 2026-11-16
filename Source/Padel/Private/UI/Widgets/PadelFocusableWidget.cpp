#include "UI/Widgets/PadelFocusableWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "InputCoreTypes.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;

void FPadelTween::Start(float Target, float Seconds)
{
	From = Value();
	To = Target;
	Duration = FMath::Max(0.0f, Seconds);
	Elapsed = 0.0f;
}

float FPadelTween::Value() const
{
	if (Duration <= 0.0f || Elapsed >= Duration)
	{
		return To;
	}
	return FMath::Lerp(From, To, Padel::Ui::EaseOutCubic(Elapsed / Duration));
}

void UPadelFocusableWidget::SetElementFocused(bool bFocused)
{
	if (bIsFocused == bFocused)
	{
		return;
	}
	bIsFocused = bFocused;
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	ScaleTween.Start(bFocused ? FocusedScale() : 1.0f, bFocused ? Motion.Focus : Motion.FocusOut);
	RefreshVisuals();
}

void UPadelFocusableWidget::SetDisabled(bool bInDisabled)
{
	if (bIsDisabled != bInDisabled)
	{
		bIsDisabled = bInDisabled;
		RefreshVisuals();
	}
}

void UPadelFocusableWidget::SetLocked(bool bInLocked)
{
	if (bIsLocked != bInLocked)
	{
		bIsLocked = bInLocked;
		RefreshVisuals();
	}
}

void UPadelFocusableWidget::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		RefreshVisuals();
	}
}

void UPadelFocusableWidget::Activate()
{
	if (bIsDisabled)
	{
		return;
	}
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	// Spec 6: the pressed state lasts at least 80 ms.
	PressTimer = FMath::Max(Theme.Motion.Press, Theme.Input.PressMinimum);
	ScaleTween.Start(PressedScale(), Theme.Motion.Press);
	RefreshVisuals();
	OnActivated.Broadcast(this);
}

float UPadelFocusableWidget::FocusedScale() const
{
	return PadelUi::Theme().Layout.FocusScale;
}

float UPadelFocusableWidget::PressedScale() const
{
	return PadelUi::Theme().Layout.PressScale;
}

void UPadelFocusableWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// User widgets don't take hits by default; elements must, for hover = focus (spec 5.6).
	SetVisibility(ESlateVisibility::Visible);
}

void UPadelFocusableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (PressTimer > 0.0f)
	{
		PressTimer -= InDeltaTime;
		if (PressTimer <= 0.0f)
		{
			PressTimer = 0.0f;
			ScaleTween.Start(bIsFocused ? FocusedScale() : 1.0f, PadelUi::Theme().Motion.Press);
			RefreshVisuals();
		}
	}
	ScaleTween.Tick(InDeltaTime);
	ApplyScale();
}

void UPadelFocusableWidget::ApplyScale()
{
	// Reduce motion: no scales (spec 9); focus still shows by ring and fill.
	const float Scale = PadelUi::Options().bReduceMotion ? 1.0f : ScaleTween.Value();
	SetRenderScale(FVector2D(Scale, Scale));
}

void UPadelFocusableWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (!bIsDisabled)
	{
		OnHovered.Broadcast(this);
	}
}

void UPadelFocusableWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (bPointerDown)
	{
		bPointerDown = false;
		RefreshVisuals();
	}
}

FReply UPadelFocusableWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || bIsDisabled)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	bPointerDown = true;
	OnHovered.Broadcast(this);
	RefreshVisuals();
	return FReply::Handled();
}

FReply UPadelFocusableWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !bPointerDown)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	bPointerDown = false;
	OnClicked(InMouseEvent);
	RefreshVisuals();
	return FReply::Handled();
}

void UPadelFocusableWidget::OnClicked(const FPointerEvent& InMouseEvent)
{
	Activate();
}

void UPadelFocusableWidget::AddFocusRing(UOverlay* Root, float Radius)
{
	const Padel::Ui::FUiLayout& Layout = PadelUi::Theme().Layout;
	const float Out = Layout.FocusGap + Layout.FocusRing;

	// Glow: a soft lime halo centred on the ring (CSS: 0 0 12px 5px Accent.Glow).
	const float GlowOut = Out + Layout.FocusGlowBlur * 0.5f;
	FocusGlow = MakeImage(PadelUiPaint::SoftBox(PadelUi::Colour(EUiColour::AccentGlow), Layout.FocusGlowBlur));
	// Behind everything else in Root, so it only shows outside the element.
	UOverlaySlot* GlowSlot = Cast<UOverlaySlot>(Root->InsertChildAt(0, FocusGlow));
	if (GlowSlot)
	{
		GlowSlot->SetHorizontalAlignment(HAlign_Fill);
		GlowSlot->SetVerticalAlignment(VAlign_Fill);
		GlowSlot->SetPadding(FMargin(-GlowOut));
	}

	FocusRing = MakeRoundedBorder(FLinearColor::Transparent, Radius + Out, PadelUi::Colour(EUiColour::Accent), Layout.FocusRing, FMargin(0.0f));
	FocusRing->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* RingSlot = Root->AddChildToOverlay(FocusRing);
	RingSlot->SetHorizontalAlignment(HAlign_Fill);
	RingSlot->SetVerticalAlignment(VAlign_Fill);
	RingSlot->SetPadding(FMargin(-Out));
	ShowFocusRing(false);
}

void UPadelFocusableWidget::ShowFocusRing(bool bShow)
{
	const ESlateVisibility Visibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	if (FocusRing)
	{
		FocusRing->SetVisibility(Visibility);
	}
	if (FocusGlow)
	{
		FocusGlow->SetVisibility(Visibility);
	}
}
