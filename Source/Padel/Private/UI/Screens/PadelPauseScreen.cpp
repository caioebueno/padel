#include "UI/Screens/PadelPauseScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "UI/Model/PadelUiPauseView.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Widgets/PadelButton.h"
#include "UI/Widgets/PadelNameStrap.h"
#include "UI/Widgets/PadelScreenHeader.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EPauseContext;
using Padel::Ui::EUiColour;
using Padel::Ui::EUiInput;
using Padel::Ui::EUiType;

const FName UPadelPauseScreen::ResumeId(TEXT("Pause.Resume"));
const FName UPadelPauseScreen::RestartId(TEXT("Pause.Restart"));
const FName UPadelPauseScreen::ControlsId(TEXT("Pause.Controls"));
const FName UPadelPauseScreen::SettingsId(TEXT("Pause.Settings"));
const FName UPadelPauseScreen::QuitId(TEXT("Pause.Quit"));

namespace
{
	// Spec 7.5 and the "Pause" board, in 1080p px and grid columns.
	constexpr float PauseContentTop = 216.0f;
	constexpr int32 PauseMenuColumns = 4;       // C1-C4
	constexpr int32 PauseContextColumn = 6;     // C6-C12
	constexpr int32 PauseContextColumns = 7;
	constexpr float PauseContextHeight = 544.0f; // y 216-760
	constexpr float PauseRowHeight = 72.0f;
	constexpr float PauseMenuPadding = 16.0f;
	constexpr float PauseContextPadding = 40.0f;
	constexpr float PauseScoreGap = 20.0f;
	constexpr float PauseServeDot = 16.0f;
	constexpr float PauseStrapGap = 16.0f;

	FText PauseUtf8(const std::string& Text)
	{
		return FText::FromString(UTF8_TO_TCHAR(Text.c_str()));
	}

	FText PauseDifficultyText(Padel::Ui::EDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case Padel::Ui::EDifficulty::Easy:
			return LOCTEXT("Setup.Difficulty.Easy", "Easy");
		case Padel::Ui::EDifficulty::Hard:
			return LOCTEXT("Setup.Difficulty.Hard", "Hard");
		case Padel::Ui::EDifficulty::Medium:
		default:
			return LOCTEXT("Setup.Difficulty.Medium", "Medium");
		}
	}

	// "MEDIUM · BEST OF 3 · GOLDEN POINT" (Versus.Rules; upper case comes from the style).
	FText PauseRulesText(const Padel::Ui::FMatchSetup& Setup)
	{
		const FText Format = Setup.SetsToWin <= 1 ? LOCTEXT("Setup.Format.OneSet", "1 set") : LOCTEXT("Setup.Format.BestOf3", "Best of 3");
		// Spec 7.12: with golden point off the rules read "ADVANTAGE".
		const FText Deuce = Setup.bGoldenPoint ? LOCTEXT("Setup.GoldenPoint", "Golden point") : LOCTEXT("Versus.Advantage", "Advantage");
		return FText::Format(LOCTEXT("Versus.Rules", "{0} · {1} · {2}"), PauseDifficultyText(Setup.Difficulty), Format, Deuce);
	}

	// "Ventara · Arunde": the human first on their own team, as on the board.
	FText PausePairText(int32 Team, int32 HumanPlayerId)
	{
		int32 First = Team * 2;
		int32 Second = Team * 2 + 1;
		if (Padel::Ui::TeamOfPlayer(HumanPlayerId) == Team && HumanPlayerId == Second)
		{
			Swap(First, Second);
		}
		return FText::Format(LOCTEXT("Pause.Pair", "{0} · {1}"),
			PauseUtf8(Padel::Ui::Surname(First)), PauseUtf8(Padel::Ui::Surname(Second)));
	}

	const Padel::Ui::FMatchSetup& PauseSetup(IPadelMenuHost* Host)
	{
		static const Padel::Ui::FMatchSetup Default;
		return Host ? Host->GetMenuMatchSetup() : Default;
	}

	const Padel::Ui::FScoreView& PauseScore(IPadelMenuHost* Host)
	{
		static const Padel::Ui::FScoreView Default;
		return Host ? Host->GetMenuScore() : Default;
	}
}

void UPadelPauseScreen::BuildScreen()
{
	SetHeader(LOCTEXT("Pause.Title", "Paused"));
	if (UPadelScreenHeader* PauseHeader = GetHeader())
	{
		// The header keeps its right slot only once its own tree exists.
		PauseHeader->EnsureBuilt();
		PauseHeader->SetRightContent(BuildScoreLine());
	}
	SetHintBarPauseStrip(true);

	// The menu column: a glass panel of list buttons, 72 px each, 8 px apart.
	struct FPauseItem
	{
		FName Id;
		FText Label;
	};
	const TArray<FPauseItem> Items = {
		{ResumeId, LOCTEXT("Pause.Resume", "Resume")},
		{RestartId, LOCTEXT("Pause.Restart", "Restart match")},
		{ControlsId, LOCTEXT("Pause.Controls", "Controls")},
		{SettingsId, LOCTEXT("Pause.Settings", "Settings")},
		{QuitId, LOCTEXT("Pause.Quit", "Quit to main menu")},
	};
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	std::vector<std::string> Ids;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		UPadelButton* Button = MakeElement<UPadelButton>(Items[Index].Id);
		Button->SetKind(EPadelButtonKind::List);
		Button->SetLabel(Items[Index].Label);
		Button->SetSize(0.0f, PauseRowHeight);
		UVerticalBoxSlot* RowSlot = List->AddChildToVerticalBox(Button);
		RowSlot->SetPadding(FMargin(0.0f, Index == 0 ? 0.0f : PadelUi::Space(1), 0.0f, 0.0f));
		Ids.push_back(TCHAR_TO_UTF8(*Items[Index].Id.ToString()));
	}
	const float Radius = PadelUi::Theme().Layout.RadiusPanel;
	PlaceInColumns(MakeGlassPanel(List, FMargin(PauseMenuPadding), Radius), 1, PauseMenuColumns, PauseContentTop, 0.0f);

	// Spec 7.5: a vertical list, wrapping; left and right do nothing. Default focus Resume.
	FocusMap = Padel::Ui::FFocusMap::VerticalList(Ids);

	// The context panel: one child per EPauseContext, the focused item's shown.
	UOverlay* ContextStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Contexts.Reset();
	Contexts.Add(BuildRulesContext());
	Contexts.Add(BuildTextContext(LOCTEXT("Main.Controls.Desc", "See every button.")));
	Contexts.Add(BuildTextContext(LOCTEXT("Main.Settings.Desc", "Audio and accessibility.")));
	Contexts.Add(BuildTextContext(LOCTEXT("Pause.Quit.Desc", "The match will not be saved.")));
	for (UWidget* Context : Contexts)
	{
		UOverlaySlot* ContextSlot = ContextStack->AddChildToOverlay(Context);
		ContextSlot->SetHorizontalAlignment(HAlign_Fill);
		ContextSlot->SetVerticalAlignment(VAlign_Top);
	}
	PlaceInColumns(MakeGlassPanel(ContextStack, FMargin(PauseContextPadding), Radius), PauseContextColumn, PauseContextColumns,
		PauseContentTop, PauseContextHeight);
	ShowContext(static_cast<int32>(EPauseContext::Rules));
}

UWidget* UPadelPauseScreen::BuildScoreLine()
{
	// "● LAGOON 6–4 3–2 30–15 EMBER": team names in their colours, tabular numbers, and a
	// dot before the serving team (spec 7.5; the board draws the ball as a plain dot).
	const Padel::Ui::FScoreView& Score = PauseScore(GetHost());
	const float HeadingPx = PadelUi::Theme().Type(EUiType::Heading).Size;
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	bool bFirst = true;
	auto Add = [Line, &bFirst](UWidget* Part)
	{
		UHorizontalBoxSlot* PartSlot = Line->AddChildToHorizontalBox(Part);
		PartSlot->SetVerticalAlignment(VAlign_Center);
		PartSlot->SetPadding(FMargin(bFirst ? 0.0f : PauseScoreGap, 0.0f, 0.0f, 0.0f));
		bFirst = false;
	};
	auto AddServeDot = [this, &Add]()
	{
		const float DotRadius = PauseServeDot * 0.5f;
		Add(MakeSized(MakeRoundedBorder(PadelUi::Colour(EUiColour::TextPrimary), DotRadius, FLinearColor::Transparent, 0.0f, FMargin(0.0f)),
			PauseServeDot, PauseServeDot));
	};
	auto AddTeam = [this, &Add, &AddServeDot, &Score](int32 Team)
	{
		if (!Score.bMatchOver && Score.ServingTeam == Team)
		{
			AddServeDot();
		}
		UTextBlock* Name = MakeTokenText(PauseUtf8(Padel::Ui::TeamName(Team)), EUiType::Heading, EUiColour::TextPrimary);
		Name->SetColorAndOpacity(FSlateColor(PadelUi::TeamColour(Team)));
		Add(Name);
	};

	AddTeam(Padel::Ui::TeamA);
	for (const std::string& Part : Padel::Ui::PauseScoreParts(Score))
	{
		UTextBlock* Numbers = MakeTokenText(PauseUtf8(Part), EUiType::Heading, EUiColour::TextPrimary);
		// Spec 7.5: tabular figures, so the line doesn't jitter. Heading has no tabular
		// face, so the numbers use Stat's tabular face at Heading's size.
		Numbers->SetFont(PadelUi::FontAtPx(EUiType::Stat, HeadingPx));
		Add(Numbers);
	}
	AddTeam(Padel::Ui::TeamB);
	return Line;
}

UWidget* UPadelPauseScreen::BuildRulesContext()
{
	const Padel::Ui::FMatchSetup& Setup = PauseSetup(GetHost());
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Box->AddChildToVerticalBox(MakeTokenText(LOCTEXT("Setup.Rules", "Match rules"), EUiType::Label, EUiColour::TextSecondary));
	UVerticalBoxSlot* RulesSlot = Box->AddChildToVerticalBox(MakeTokenText(PauseRulesText(Setup), EUiType::Heading, EUiColour::TextPrimary));
	RulesSlot->SetPadding(FMargin(0.0f, PadelUi::Space(1), 0.0f, 0.0f));

	for (int32 Team = Padel::Ui::TeamA; Team <= Padel::Ui::TeamB; ++Team)
	{
		UPadelNameStrap* Strap = WidgetTree->ConstructWidget<UPadelNameStrap>(UPadelNameStrap::StaticClass());
		Strap->SetCompact(true);
		Strap->SetPlayer(PauseUtf8(Padel::Ui::TeamName(Team)), PausePairText(Team, Setup.HumanPlayerId), Team);
		Strap->SetBadge(Padel::Ui::TeamOfPlayer(Setup.HumanPlayerId) == Team, EPadelBadgeKind::You);
		UVerticalBoxSlot* StrapSlot = Box->AddChildToVerticalBox(Strap);
		StrapSlot->SetPadding(FMargin(0.0f, Team == Padel::Ui::TeamA ? PadelUi::Space(4) : PauseStrapGap, 0.0f, 0.0f));
		StrapSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Box;
}

UWidget* UPadelPauseScreen::BuildTextContext(const FText& Text)
{
	// TODO(workstream D): on Controls, the small gamepad diagram of spec 7.6 once it lands.
	UTextBlock* Block = MakeTokenText(Text, EUiType::Body, EUiColour::TextPrimary);
	Block->SetAutoWrapText(true);
	return Block;
}

void UPadelPauseScreen::ShowContext(int32 Index)
{
	ContextIndex = Index;
	for (int32 Each = 0; Each < Contexts.Num(); ++Each)
	{
		if (Contexts[Each])
		{
			Contexts[Each]->SetVisibility(Each == Index ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UPadelPauseScreen::OnFocusChanged(UPadelFocusableWidget* Element)
{
	if (Element)
	{
		ShowContext(static_cast<int32>(Padel::Ui::PauseContextFor(TCHAR_TO_UTF8(*Element->GetElementId().ToString()))));
	}
}

void UPadelPauseScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	if (!Element)
	{
		return;
	}
	const FName Id = Element->GetElementId();
	UPadelMenuRoot* Root = GetMenuRoot();
	if (Id == ResumeId)
	{
		if (IPadelMenuHost* Host = GetHost())
		{
			Host->MenuResumeMatch();
		}
	}
	else if (Id == RestartId)
	{
		ConfirmRestart();
	}
	else if (Id == ControlsId && Root)
	{
		// The flow remembers Pause as the opener, so B on Controls comes back here.
		Root->ShowScreen(EPadelScreen::Controls);
	}
	else if (Id == SettingsId && Root)
	{
		Root->ShowScreen(EPadelScreen::Settings);
	}
	else if (Id == QuitId)
	{
		ConfirmQuit();
	}
}

void UPadelPauseScreen::ConfirmRestart()
{
	UPadelMenuRoot* Root = GetMenuRoot();
	if (!Root)
	{
		return;
	}
	// Spec 7.10 Dlg.Restart: the safe option keeps playing and returns focus to Restart.
	FPadelConfirm Confirm;
	Confirm.Title = LOCTEXT("Dlg.Restart.Title", "Restart the match?");
	Confirm.Body = LOCTEXT("Dlg.Restart.Body", "The score goes back to 0–0.");
	Confirm.ActionLabel = LOCTEXT("Dlg.Restart.Action", "Restart");
	Confirm.SafeLabel = LOCTEXT("Dlg.KeepPlaying", "Keep playing");
	TWeakObjectPtr<UPadelPauseScreen> WeakThis(this);
	Confirm.OnAction = [WeakThis]()
	{
		if (UPadelPauseScreen* Screen = WeakThis.Get())
		{
			if (IPadelMenuHost* Host = Screen->GetHost())
			{
				Host->MenuRestartMatch();
			}
		}
	};
	Confirm.OnSafe = [WeakThis]()
	{
		if (UPadelPauseScreen* Screen = WeakThis.Get())
		{
			Screen->RefreshHints();
		}
	};
	Root->ShowConfirm(Confirm);
	RefreshHints();
}

void UPadelPauseScreen::ConfirmQuit()
{
	UPadelMenuRoot* Root = GetMenuRoot();
	if (!Root)
	{
		return;
	}
	// Spec 7.10 Dlg.QuitMatch: destructive, so the action is the Danger button.
	FPadelConfirm Confirm;
	Confirm.Title = LOCTEXT("Dlg.QuitMatch.Title", "Quit to main menu?");
	Confirm.Body = LOCTEXT("Dlg.QuitMatch.Body", "The match will not be saved.");
	Confirm.ActionLabel = LOCTEXT("Dlg.QuitMatch.Action", "Quit match");
	Confirm.SafeLabel = LOCTEXT("Dlg.KeepPlaying", "Keep playing");
	Confirm.bDanger = true;
	TWeakObjectPtr<UPadelPauseScreen> WeakThis(this);
	Confirm.OnAction = [WeakThis]()
	{
		if (UPadelPauseScreen* Screen = WeakThis.Get())
		{
			if (IPadelMenuHost* Host = Screen->GetHost())
			{
				Host->MenuQuitMatch();
			}
		}
	};
	Confirm.OnSafe = [WeakThis]()
	{
		if (UPadelPauseScreen* Screen = WeakThis.Get())
		{
			Screen->RefreshHints();
		}
	};
	Root->ShowConfirm(Confirm);
	RefreshHints();
}

TArray<FPadelHint> UPadelPauseScreen::GetHints() const
{
	// The boards: "A Select · B Resume", and "A Select · B Back" while a dialog is open
	// (B there picks the safe option).
	if (UPadelMenuRoot* Root = GetMenuRoot(); Root && Root->IsDialogOpen())
	{
		return {{EUiInput::Confirm, LOCTEXT("Hint.Select", "Select")}, {EUiInput::Back, LOCTEXT("Hint.Back", "Back")}};
	}
	return Super::GetHints();
}

#undef LOCTEXT_NAMESPACE
