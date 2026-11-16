#include "UI/Screens/PadelStatsScreen.h"

#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Screens/PadelStatsScorePanel.h"
#include "UI/Screens/PadelStatsTable.h"
#include "UI/Widgets/PadelButton.h"
#include "UI/Widgets/PadelScreenHeader.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

const FName UPadelStatsScreen::RematchId(TEXT("Stats.Rematch"));
const FName UPadelStatsScreen::MainMenuId(TEXT("Stats.MainMenu"));

namespace
{
	// Spec 7.8 and the "Stats" board, in 1080p px and grid columns.
	constexpr float StatsScoreTop = 184.0f;
	constexpr float StatsScoreHeight = 216.0f;
	constexpr float StatsTableTop = 424.0f;
	constexpr float StatsTableHeight = 424.0f;
	constexpr float StatsFooterTop = 872.0f;
	constexpr float StatsFooterHeight = 64.0f;
	constexpr float StatsRematchWidth = 320.0f;
	constexpr float StatsMainMenuWidth = 240.0f;
	constexpr int32 StatsFooterColumns = 7; // the footer line stops short of the buttons

	FText StatsUtf8(const std::string& Text)
	{
		return FText::FromString(UTF8_TO_TCHAR(Text.c_str()));
	}

	const Padel::Ui::FMatchStats& StatsOf(IPadelMenuHost* Host)
	{
		static const Padel::Ui::FMatchStats Empty;
		return Host ? Host->GetMenuMatchStats() : Empty;
	}

	int32 StatsHumanPlayer(IPadelMenuHost* Host)
	{
		static const Padel::Ui::FMatchSetup Default;
		return Host ? Host->GetMenuMatchSetup().HumanPlayerId : Default.HumanPlayerId;
	}
}

void UPadelStatsScreen::BuildScreen()
{
	const Padel::Ui::FMatchStats& Stats = StatsOf(GetHost());
	const Padel::Ui::FUiTheme& Theme = PadelUi::Theme();

	// "{TEAM} WIN" in the winner's colour, or "MATCH STATS" without a winner (spec 7.8).
	const bool bWinner = Stats.MatchWinner == Padel::Ui::TeamA || Stats.MatchWinner == Padel::Ui::TeamB;
	SetHeader(bWinner ? FText::Format(LOCTEXT("Stats.Win", "{0} win"), StatsUtf8(Padel::Ui::TeamName(Stats.MatchWinner)))
		: LOCTEXT("Stats.Title", "Match stats"));
	if (UPadelScreenHeader* StatsHeader = GetHeader())
	{
		StatsHeader->EnsureBuilt();
		if (bWinner)
		{
			StatsHeader->SetTitleColour(PadelUi::TeamColour(Stats.MatchWinner));
		}
		StatsHeader->SetRightContent(MakeTokenText(LOCTEXT("Stats.Title", "Match stats"), EUiType::Label, EUiColour::TextSecondary));
	}

	ScorePanel = WidgetTree->ConstructWidget<UPadelStatsScorePanel>(UPadelStatsScorePanel::StaticClass());
	ScorePanel->SetStats(Stats, StatsHumanPlayer(GetHost()));
	PlaceInColumns(ScorePanel, 1, static_cast<int32>(Theme.Layout.GridColumns), StatsScoreTop, StatsScoreHeight);

	Table = WidgetTree->ConstructWidget<UPadelStatsTable>(UPadelStatsTable::StaticClass());
	Table->SetStats(Stats);
	Table->SetFamily(GetPadFamily());
	PlaceInColumns(Table, 1, static_cast<int32>(Theme.Layout.GridColumns), StatsTableTop, StatsTableHeight);

	// "Average rally 8.4 shots · 31% of points had a shot off the glass".
	UTextBlock* Footer = MakeTokenText(FText::Format(LOCTEXT("Stats.Footer", "Average rally {0} shots · {1}% of points had a shot off the glass"),
		StatsUtf8(Padel::Ui::RallyText(Stats)), FText::AsNumber(Padel::Ui::GlassPercent(Stats))), EUiType::BodySmall, EUiColour::TextSecondary);
	Footer->SetAutoWrapText(true);
	USizeBox* FooterBox = MakeSized(Footer, PadelUi::SpanWidth(StatsFooterColumns), StatsFooterHeight);
	if (USizeBoxSlot* FooterSlot = Cast<USizeBoxSlot>(Footer->Slot))
	{
		FooterSlot->SetVerticalAlignment(VAlign_Center);
	}
	Place(FooterBox, Theme.Layout.SafeX, StatsFooterTop, PadelUi::SpanWidth(StatsFooterColumns), StatsFooterHeight);

	// Rematch (primary, with its A glyph) and Main menu (secondary), right-aligned.
	const float Right = 1920.0f - Theme.Layout.SafeX;
	MainMenu = MakeElement<UPadelButton>(MainMenuId);
	MainMenu->SetKind(EPadelButtonKind::Secondary);
	MainMenu->SetLabel(LOCTEXT("Stats.MainMenu", "Main menu"));
	MainMenu->SetSize(StatsMainMenuWidth, StatsFooterHeight);
	const float MainMenuX = Right - StatsMainMenuWidth;
	Place(MainMenu, MainMenuX, StatsFooterTop, StatsMainMenuWidth, StatsFooterHeight);

	Rematch = MakeElement<UPadelButton>(RematchId);
	Rematch->SetKind(EPadelButtonKind::Primary);
	Rematch->SetLabel(LOCTEXT("Stats.Rematch", "Rematch"));
	Rematch->SetSize(StatsRematchWidth, StatsFooterHeight);
	Place(Rematch, MainMenuX - PadelUi::Space(3) - StatsRematchWidth, StatsFooterTop, StatsRematchWidth, StatsFooterHeight);

	// Spec 7.8: Rematch <-> Main menu, no wrap; up and down do nothing. Default Rematch.
	FocusMap = Padel::Ui::FFocusMap::Row({TCHAR_TO_UTF8(*RematchId.ToString()), TCHAR_TO_UTF8(*MainMenuId.ToString())});

	// The entry animation and the stray-press lock start with the screen. Reduce motion
	// shows the end state at once; the lock still applies (spec 7.8).
	Timeline.SetColumns = Padel::Ui::SetCount(Stats);
	Timeline.Rows = static_cast<int>(Padel::Ui::StatsTabRows(Stats, Padel::Ui::EStatsTab::Summary).size());
	Timeline.ColumnStagger = Theme.Motion.StatsSetStagger;
	Timeline.BarGrow = Theme.Motion.StatsBarGrow;
	Timeline.RowStagger = Theme.Motion.Stagger;
	Timeline.StaggerMax = static_cast<int>(Theme.Motion.StaggerMax);
	Gate.Begin(GetConfirmLockSeconds(), PadelUi::Options().bReduceMotion ? 0.0f : Timeline.Duration());
	ScorePanel->EnsureBuilt();
	Table->EnsureBuilt();
	ApplyAnimation();
	ApplyLockLook();
}

float UPadelStatsScreen::GetConfirmLockSeconds() const
{
	return PadelUi::Theme().Input.StatsConfirmLockSeconds;
}

void UPadelStatsScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceTime(InDeltaTime);
}

void UPadelStatsScreen::AdvanceTime(float DeltaSeconds)
{
	const bool bWasAnimating = Gate.IsAnimating();
	Gate.Tick(DeltaSeconds);
	if (bWasAnimating)
	{
		ApplyAnimation();
	}
	if (bShownLocked != Gate.IsLocked())
	{
		ApplyLockLook();
	}
}

void UPadelStatsScreen::ApplyAnimation()
{
	const float T = Gate.AnimationTime();
	if (ScorePanel)
	{
		for (int32 Column = 0; Column < ScorePanel->GetColumnCount(); ++Column)
		{
			ScorePanel->SetColumnAlpha(Column, Gate.IsAnimating() ? Timeline.ColumnAlpha(Column, T) : 1.0f);
		}
	}
	if (Table)
	{
		// Only the first tab animates; a tab change after the entry shows full bars.
		if (!Gate.IsAnimating())
		{
			Table->ShowAllBars();
			return;
		}
		for (int32 Row = 0; Row < Table->GetRowCount(); ++Row)
		{
			Table->SetRowProgress(Row, Timeline.BarProgress(Row, T));
		}
	}
}

void UPadelStatsScreen::ApplyLockLook()
{
	// The "StatsLocked" board: both buttons Disabled, no A glyph and no A hint, until the
	// lock ends; then Rematch shows its focus again.
	bShownLocked = Gate.IsLocked();
	if (Rematch)
	{
		Rematch->SetDisabled(bShownLocked);
		Rematch->SetShowGlyph(!bShownLocked, GetPadFamily());
	}
	if (MainMenu)
	{
		MainMenu->SetDisabled(bShownLocked);
	}
	RefreshHints();
}

bool UPadelStatsScreen::OnBlockedConfirm()
{
	// Spec 7.8: an A during the animation only finishes it; an A in the lock does nothing.
	if (Gate.Press() == Padel::Ui::EStatsConfirm::FinishAnimation)
	{
		ApplyAnimation();
		if (bShownLocked != Gate.IsLocked())
		{
			ApplyLockLook();
		}
	}
	return true;
}

bool UPadelStatsScreen::OnScreenInput(EUiInput Input, bool bRepeat)
{
	if ((Input == EUiInput::TabLeft || Input == EUiInput::TabRight) && !bRepeat && Table)
	{
		if (Table->HandleTabInput(Input))
		{
			// Tab changes after the entry show full bars; during it, the new tab joins in.
			ApplyAnimation();
		}
		return true;
	}
	return false;
}

void UPadelStatsScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	// A mouse click comes here without passing the Confirm gate, so check it again.
	if (!Element || Gate.IsLocked())
	{
		return;
	}
	UPadelMenuRoot* Root = GetMenuRoot();
	if (!Root)
	{
		return;
	}
	if (Element->GetElementId() == RematchId)
	{
		// Same setup, through the Versus card (spec 7.8; Versus starts the match).
		Root->ShowScreen(EPadelScreen::Versus);
	}
	else if (Element->GetElementId() == MainMenuId)
	{
		Root->ShowScreen(EPadelScreen::MainMenu);
	}
}

TArray<FPadelHint> UPadelStatsScreen::GetHints() const
{
	// "A Select · LB RB Change tab"; no B (it does nothing here), and no A during the lock.
	TArray<FPadelHint> Hints;
	if (!Gate.IsLocked() && !GetFocusedId().IsNone())
	{
		Hints.Add({EUiInput::Confirm, LOCTEXT("Hint.Select", "Select")});
	}
	Hints.Add({EUiInput::TabLeft, LOCTEXT("Hint.ChangeTab", "Change tab")});
	return Hints;
}

#undef LOCTEXT_NAMESPACE
