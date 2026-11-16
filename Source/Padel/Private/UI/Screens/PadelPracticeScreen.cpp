#include "UI/Screens/PadelPracticeScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelPracticeControlList.h"
#include "UI/Widgets/PadelButton.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

namespace
{
	// Board "Practice": the panel spans C3-C10 from y 266 (centred in the content area
	// at the default Text size), with 48 px padding; 16 px under the title, 32 px before
	// the controls, 40 px before the button.
	constexpr int32 PracticePanelColumn = 3;
	constexpr int32 PracticePanelSpan = 8;
	constexpr float PracticePanelTop = 266.0f;
	constexpr float PracticePanelPadding = 48.0f;
	constexpr float PracticeBodyGap = 16.0f;
	constexpr float PracticeControlsGap = 32.0f;
	constexpr float PracticeButtonGap = 40.0f;

	const FName PracticeStartId(TEXT("Practice.Start"));
}

void UPadelPracticeScreen::BuildScreen()
{
	// The title lives in the panel (board "Practice"), not the header band.
	SetHeaderVisible(false);
	ShownFamily = GetPadFamily();

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->AddChildToVerticalBox(MakeTokenText(LOCTEXT("Practice.Title", "Practice"), EUiType::Display, EUiColour::TextPrimary));

	UTextBlock* Body = MakeTokenText(LOCTEXT("Practice.Body", "The ball machine feeds you balls. Play any shot, including off the glass."),
		EUiType::Body, EUiColour::TextSecondary);
	Body->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(Body)->SetPadding(FMargin(0.0f, PracticeBodyGap, 0.0f, 0.0f));

	Controls = WidgetTree->ConstructWidget<UPadelPracticeControlList>(UPadelPracticeControlList::StaticClass());
	Controls->SetFamily(ShownFamily);
	Column->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0.0f, PracticeControlsGap, 0.0f, 0.0f));

	StartButton = MakeElement<UPadelButton>(PracticeStartId);
	StartButton->SetKind(EPadelButtonKind::Primary);
	StartButton->SetLabel(LOCTEXT("Practice.Start", "Start practice"));
	StartButton->SetShowGlyph(true, ShownFamily);
	UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(StartButton);
	ButtonSlot->SetHorizontalAlignment(HAlign_Left);
	ButtonSlot->SetPadding(FMargin(0.0f, PracticeButtonGap, 0.0f, 0.0f));

	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	UWidget* Panel = MakeGlassPanel(Column, FMargin(PracticePanelPadding), Theme.Layout.RadiusPanel);
	PlaceInColumns(Panel, PracticePanelColumn, PracticePanelSpan, PracticePanelTop, 0.0f);

	FocusMap = Padel::Ui::FFocusMap::VerticalList({TCHAR_TO_UTF8(*PracticeStartId.ToString())});
}

void UPadelPracticeScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The glyphs follow the device in use (spec 5.5).
	if (ShownFamily != GetPadFamily())
	{
		ShownFamily = GetPadFamily();
		if (Controls)
		{
			Controls->SetFamily(ShownFamily);
		}
		if (StartButton)
		{
			StartButton->SetShowGlyph(true, ShownFamily);
		}
	}
}

void UPadelPracticeScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	if (Element && Element->GetElementId() == PracticeStartId)
	{
		if (IPadelMenuHost* Host = GetHost())
		{
			Host->MenuStartPractice();
		}
	}
}

TArray<FPadelHint> UPadelPracticeScreen::GetHints() const
{
	// Board "Practice": A Start, B Back (to the main menu).
	return {
		{EUiInput::Confirm, LOCTEXT("Hint.Start", "Start")},
		{EUiInput::Back, LOCTEXT("Hint.Back", "Back")},
	};
}

#undef LOCTEXT_NAMESPACE
