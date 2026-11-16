#include "UI/Widgets/PadelToggle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiPaint.h"
#include "UI/PadelUiTokens.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	constexpr float ToggleTrackWidth = 72.0f;
	constexpr float ToggleTrackHeight = 36.0f;
	constexpr float ToggleKnob = 28.0f;
	constexpr float ToggleInset = 4.0f;
	constexpr float ToggleWordWidth = 48.0f;
	constexpr float ToggleWordGap = 16.0f;
}

void UPadelToggle::SetOn(bool bInOn)
{
	Model.bOn = bInOn;
	RefreshVisuals();
}

bool UPadelToggle::HandleInput(EUiInput Input, bool bRepeat)
{
	if (IsDisabled() || (Input != EUiInput::Left && Input != EUiInput::Right))
	{
		return false;
	}
	if (Model.Push(Input == EUiInput::Right ? 1 : -1))
	{
		RefreshVisuals();
		OnValueChanged.Broadcast(this);
	}
	else if (!bRepeat)
	{
		OnEdgeReached.Broadcast(this);
	}
	return true;
}

void UPadelToggle::Activate()
{
	if (IsDisabled())
	{
		return;
	}
	Model.Toggle();
	Super::Activate();
	OnValueChanged.Broadcast(this);
}

void UPadelToggle::BuildValue(UHorizontalBox* Row)
{
	// Board 06: the word sits left of the switch (48 px, right-aligned, 16 px gap), and the
	// knob slides inside the 72 x 36 pill.
	Word = MakeTokenText(FText::GetEmpty(), EUiType::Label, EUiColour::TextPrimary);
	Word->SetJustification(ETextJustify::Right);
	UHorizontalBoxSlot* WordSlot = Row->AddChildToHorizontalBox(MakeSized(Word, ToggleWordWidth, 0.0f));
	WordSlot->SetVerticalAlignment(VAlign_Center);
	WordSlot->SetPadding(FMargin(0.0f, 0.0f, ToggleWordGap, 0.0f));

	TrackRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Track = MakeRoundedBorder(FLinearColor::Transparent, PadelUi::Theme().Layout.RadiusPill, FLinearColor::Transparent, 0.0f,
		FMargin(ToggleInset), TrackRow);
	Track->SetVerticalAlignment(VAlign_Center);

	Knob = MakeRoundedBorder(FLinearColor::White, ToggleKnob * 0.5f, FLinearColor::Transparent, 0.0f, FMargin(0.0f));
	KnobBox = MakeSized(Knob, ToggleKnob, ToggleKnob);
	TrackRow->AddChildToHorizontalBox(KnobBox)->SetVerticalAlignment(VAlign_Center);

	Row->AddChildToHorizontalBox(MakeSized(Track, ToggleTrackWidth, ToggleTrackHeight))->SetVerticalAlignment(VAlign_Center);
}

void UPadelToggle::RefreshValue()
{
	if (!Track || !TrackRow || !Knob || !KnobBox || !Word)
	{
		return;
	}
	const bool bOn = Model.bOn;
	const bool bDisabled = IsDisabled();
	const Padel::Ui::FUiLayout& Layout = PadelUi::Theme().Layout;
	Track->SetBrush(PadelUiPaint::Rounded(PadelUi::Colour(bOn && !bDisabled ? EUiColour::Accent : EUiColour::GlassEdge), Layout.RadiusPill));
	Knob->SetBrush(PadelUiPaint::Rounded(
		PadelUi::Colour(bDisabled ? EUiColour::TextDisabled : (bOn ? EUiColour::OnAccent : EUiColour::TextSecondary)), ToggleKnob * 0.5f));
	Word->SetText(bOn ? LOCTEXT("ToggleOn", "On") : LOCTEXT("ToggleOff", "Off"));
	Word->SetColorAndOpacity(FSlateColor(PadelUi::Colour(bDisabled ? EUiColour::TextDisabled : EUiColour::TextPrimary)));
	// Off: knob on the left; On: on the right.
	if (UHorizontalBoxSlot* KnobSlot = Cast<UHorizontalBoxSlot>(KnobBox->Slot))
	{
		KnobSlot->SetPadding(FMargin(bOn ? ToggleTrackWidth - 2.0f * ToggleInset - ToggleKnob : 0.0f, 0.0f, 0.0f, 0.0f));
	}
}

#undef LOCTEXT_NAMESPACE
