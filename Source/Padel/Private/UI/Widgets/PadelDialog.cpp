#include "UI/Widgets/PadelDialog.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"
#include "UI/Widgets/PadelButton.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	constexpr int32 DialogColumns = 6;
	constexpr float DialogPadding = 40.0f;
	constexpr float DialogTextGap = 12.0f;
	constexpr float DialogButtonsTop = 28.0f;
	constexpr float DialogButtonGap = 24.0f;
	constexpr float DialogButtonWidth = 320.0f;
	constexpr float DialogButtonHeight = 72.0f;
	constexpr int32 DialogBodyMaxLines = 3;
}

void UPadelDialog::BuildTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// Spec 6.12: an extra scrim over whatever is behind, which also eats clicks.
	Scrim = MakeImage(PadelUiPaint::Solid(PadelUi::Colour(EUiColour::BgScrimDialog)));
	Scrim->SetVisibility(ESlateVisibility::Visible);
	UOverlaySlot* ScrimSlot = Root->AddChildToOverlay(Scrim);
	ScrimSlot->SetHorizontalAlignment(HAlign_Fill);
	ScrimSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TitleText = MakeTokenText(FText::GetEmpty(), EUiType::Title, EUiColour::TextPrimary);
	TitleText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(TitleText);
	BodyText = MakeTokenText(FText::GetEmpty(), EUiType::Body, EUiColour::TextSecondary);
	BodyText->SetAutoWrapText(true);
	// Spec 6.12: at most three lines of body.
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	USizeBox* BodyBox = MakeSized(BodyText, 0.0f, 0.0f);
	BodyBox->SetMaxDesiredHeight(Theme.Type(EUiType::Body).LineHeight * DialogBodyMaxLines
		* Theme.TextScale[static_cast<int32>(PadelUi::Options().TextSize)]);
	BodyBox->SetClipping(EWidgetClipping::ClipToBounds);
	Column->AddChildToVerticalBox(BodyBox)->SetPadding(FMargin(0.0f, DialogTextGap, 0.0f, 0.0f));

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ActionButton = WidgetTree->ConstructWidget<UPadelButton>(UPadelButton::StaticClass());
	ActionButton->SetSize(DialogButtonWidth, DialogButtonHeight);
	ActionButton->SetElementId(TEXT("Dialog.Action"));
	Buttons->AddChildToHorizontalBox(ActionButton);
	SafeButton = WidgetTree->ConstructWidget<UPadelButton>(UPadelButton::StaticClass());
	SafeButton->SetKind(EPadelButtonKind::Secondary);
	SafeButton->SetSize(DialogButtonWidth, DialogButtonHeight);
	SafeButton->SetElementId(TEXT("Dialog.Safe"));
	Buttons->AddChildToHorizontalBox(SafeButton)->SetPadding(FMargin(DialogButtonGap, 0.0f, 0.0f, 0.0f));
	UVerticalBoxSlot* ButtonsSlot = Column->AddChildToVerticalBox(Buttons);
	ButtonsSlot->SetHorizontalAlignment(HAlign_Right);
	ButtonsSlot->SetPadding(FMargin(0.0f, DialogTextGap + DialogButtonsTop, 0.0f, 0.0f));

	for (UPadelButton* Button : {ActionButton.Get(), SafeButton.Get()})
	{
		Button->OnActivated.AddUObject(this, &UPadelDialog::OnButtonActivated);
		Button->OnHovered.AddUObject(this, &UPadelDialog::OnButtonHovered);
	}

	// Solid charcoal, not glass, so the question reads (spec 6.12), with the glass edge light.
	UBorder* Fill = nullptr;
	UOverlay* Glass = MakeGlassPanel(Column, FMargin(DialogPadding), Theme.Layout.RadiusPanel, &Fill);
	if (Fill)
	{
		Fill->SetBrush(PadelUiPaint::Rounded(PadelUi::Colour(EUiColour::SurfaceCharcoal), Theme.Layout.RadiusPanel));
	}
	Panel = MakeSized(Glass, PadelUi::SpanWidth(DialogColumns), 0.0f);
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	// Open() may have run before the tree existed.
	ApplyConfirm();
	SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UPadelDialog::ApplyConfirm()
{
	if (!TitleText || !BodyText || !ActionButton || !SafeButton)
	{
		return;
	}
	TitleText->SetText(Confirm.Title);
	BodyText->SetText(Confirm.Body);
	BodyText->SetVisibility(Confirm.Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ActionButton->SetKind(Confirm.bDanger ? EPadelButtonKind::Danger : EPadelButtonKind::Primary);
	ActionButton->SetLabel(Confirm.ActionLabel);
	SafeButton->SetLabel(Confirm.SafeLabel);
	SetSafeFocused(bSafeFocused);
}

void UPadelDialog::Open(const FPadelConfirm& InConfirm)
{
	Confirm = InConfirm;
	bClosing = false;
	bOpen = true;
	bSafeFocused = true;
	ApplyConfirm();
	SetVisibility(ESlateVisibility::Visible);
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	Fade.Snap(0.0f);
	Fade.Start(1.0f, PadelUi::Options().bReduceMotion ? Motion.ReduceMotionFade : Motion.DialogIn);
}

void UPadelDialog::Dismiss()
{
	bOpen = false;
	bClosing = false;
	Fade.Snap(0.0f);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UPadelDialog::HandleInput(EUiInput Input)
{
	if (!bOpen)
	{
		return;
	}
	switch (Input)
	{
	case EUiInput::Left:
		SetSafeFocused(false);
		break;
	case EUiInput::Right:
		SetSafeFocused(true);
		break;
	case EUiInput::Confirm:
		if (UPadelButton* Focused = bSafeFocused ? SafeButton.Get() : ActionButton.Get())
		{
			Focused->Activate();
		}
		break;
	case EUiInput::Back:
		// Spec 6.12: B always picks the safe option, whatever has focus.
		if (SafeButton)
		{
			SafeButton->Activate();
		}
		break;
	default:
		// Start and everything else: a dialog can't be dismissed by Start.
		break;
	}
}

void UPadelDialog::SetSafeFocused(bool bSafe)
{
	bSafeFocused = bSafe;
	if (ActionButton && SafeButton)
	{
		ActionButton->SetElementFocused(!bSafe);
		SafeButton->SetElementFocused(bSafe);
	}
}

void UPadelDialog::OnButtonHovered(UPadelFocusableWidget* Element)
{
	if (bOpen)
	{
		SetSafeFocused(Element == SafeButton);
	}
}

void UPadelDialog::OnButtonActivated(UPadelFocusableWidget* Element)
{
	if (bOpen)
	{
		Choose(Element == SafeButton);
	}
}

void UPadelDialog::Choose(bool bSafe)
{
	// The choice runs at once (so the game never waits on a fade); the panel then fades
	// out over Motion.DialogOut, ignoring input.
	bOpen = false;
	bClosing = true;
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	Fade.Start(0.0f, Motion.DialogOut);
	const TFunction<void()> Callback = bSafe ? Confirm.OnSafe : Confirm.OnAction;
	if (Callback)
	{
		Callback();
	}
}

void UPadelDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bOpen && !bClosing)
	{
		return;
	}
	Fade.Tick(InDeltaTime);
	const float Alpha = Fade.Value();
	SetRenderOpacity(Alpha);
	if (Panel)
	{
		const float From = PadelUi::Options().bReduceMotion ? 1.0f : PadelUi::Theme().Motion.DialogScaleFrom;
		const float Scale = FMath::Lerp(From, 1.0f, Alpha);
		Panel->SetRenderScale(FVector2D(Scale, Scale));
	}
	if (bClosing && Fade.IsDone())
	{
		bClosing = false;
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
