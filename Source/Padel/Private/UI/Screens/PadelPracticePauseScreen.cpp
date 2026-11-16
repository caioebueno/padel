#include "UI/Screens/PadelPracticePauseScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Model/PadelUiFrontEnd.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Screens/PadelPracticeControlList.h"
#include "UI/Widgets/PadelButton.h"
#include "UI/Widgets/PadelHintBar.h"
#include "UI/Widgets/PadelScreenHeader.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiNavKind;
using Padel::Ui::EUiType;

namespace
{
	// Board "PracticePause": the list panel spans C1-C4 from y 216 with 16 px padding and
	// 8 px between rows; the controls panel spans C6-C12, 544 px tall, 40 px padding.
	constexpr float PracticePauseTop = 216.0f;
	constexpr int32 PracticePauseListColumn = 1;
	constexpr int32 PracticePauseListSpan = 4;
	constexpr float PracticePauseListPadding = 16.0f;
	constexpr float PracticePauseRowGap = 8.0f;
	constexpr int32 PracticePauseInfoColumn = 6;
	constexpr int32 PracticePauseInfoSpan = 7;
	constexpr float PracticePauseInfoHeight = 544.0f;
	constexpr float PracticePauseInfoPadding = 40.0f;
	constexpr float PracticePauseInfoListGap = 16.0f;

	FText PracticePauseLabel(const FString& Id)
	{
		if (Id == TEXT("PracticePause.Resume"))
		{
			return LOCTEXT("Pause.Resume", "Resume");
		}
		if (Id == TEXT("PracticePause.Controls"))
		{
			return LOCTEXT("Pause.Controls", "Controls");
		}
		if (Id == TEXT("PracticePause.Settings"))
		{
			return LOCTEXT("Pause.Settings", "Settings");
		}
		return LOCTEXT("Practice.Quit", "Quit practice");
	}
}

void UPadelPracticePauseScreen::BuildScreen()
{
	SetHeader(LOCTEXT("Pause.Title", "Paused"));
	ShownFamily = GetPadFamily();
	// No score in practice: the header's right side names the drill (spec 7.9).
	DrillName = MakeTokenText(FText::GetEmpty(), EUiType::Heading, EUiColour::TextPrimary);
	if (const IPadelMenuHost* Host = GetHost())
	{
		DrillName->SetText(Host->GetMenuPracticeDrill());
	}
	if (UPadelScreenHeader* ScreenHeader = GetHeader())
	{
		ScreenHeader->SetRightContent(DrillName);
	}
	if (UPadelHintBar* Hints = GetHintBar())
	{
		Hints->SetPauseStrip(true);
	}

	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	std::vector<std::string> Ids;
	for (const std::string& Item : Padel::Ui::PracticePauseItems())
	{
		const FString Id = UTF8_TO_TCHAR(Item.c_str());
		UPadelButton* Row = MakeElement<UPadelButton>(FName(*Id));
		Row->SetKind(EPadelButtonKind::List);
		Row->SetLabel(PracticePauseLabel(Id));
		List->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, Ids.empty() ? 0.0f : PracticePauseRowGap, 0.0f, 0.0f));
		Ids.push_back(Item);
	}
	UWidget* ListPanel = MakeGlassPanel(List, FMargin(PracticePauseListPadding), Theme.Layout.RadiusPanel);
	PlaceInColumns(ListPanel, PracticePauseListColumn, PracticePauseListSpan, PracticePauseTop, 0.0f);

	UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Info->AddChildToVerticalBox(MakeTokenText(LOCTEXT("Practice.ControlsHeading", "Practice controls"), EUiType::Label,
		EUiColour::TextSecondary));
	Controls = WidgetTree->ConstructWidget<UPadelPracticeControlList>(UPadelPracticeControlList::StaticClass());
	Controls->SetFamily(ShownFamily);
	Info->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0.0f, PracticePauseInfoListGap, 0.0f, 0.0f));
	UWidget* InfoPanel = MakeGlassPanel(Info, FMargin(PracticePauseInfoPadding), Theme.Layout.RadiusPanel);
	PlaceInColumns(InfoPanel, PracticePauseInfoColumn, PracticePauseInfoSpan, PracticePauseTop, PracticePauseInfoHeight);

	FocusMap = Padel::Ui::FFocusMap::VerticalList(Ids);
}

void UPadelPracticePauseScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ShownFamily != GetPadFamily() && Controls)
	{
		ShownFamily = GetPadFamily();
		Controls->SetFamily(ShownFamily);
	}
}

void UPadelPracticePauseScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	UPadelMenuRoot* Root = GetMenuRoot();
	if (!Element || !Root)
	{
		return;
	}
	const FString Id = Element->GetElementId().ToString();
	IPadelMenuHost* Host = GetHost();
	if (Id == TEXT("PracticePause.Resume"))
	{
		if (Host)
		{
			Host->MenuResumeMatch();
		}
	}
	else if (Id == TEXT("PracticePause.Controls"))
	{
		Root->ShowScreen(EPadelScreen::Controls, EUiNavKind::Forward);
	}
	else if (Id == TEXT("PracticePause.Settings"))
	{
		Root->ShowScreen(EPadelScreen::Settings, EUiNavKind::Forward);
	}
	else if (Id == TEXT("PracticePause.Quit"))
	{
		// Spec 7.9: no confirm, nothing is lost.
		if (Host)
		{
			Host->MenuQuitPractice();
		}
		else
		{
			Root->ShowScreen(EPadelScreen::MainMenu, EUiNavKind::Forward);
		}
	}
}

#undef LOCTEXT_NAMESPACE
