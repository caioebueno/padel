#include "UI/Screens/PadelPlaceholderScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PadelMenuHost.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Widgets/PadelButton.h"

#define LOCTEXT_NAMESPACE "PadelMenu"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiNavKind;

namespace
{
	constexpr float PlaceholderListTop = 208.0f;
	constexpr int32 PlaceholderListColumns = 5;
	constexpr float PlaceholderRowGap = 8.0f;
}

TArray<UPadelPlaceholderScreen::FExit> UPadelPlaceholderScreen::MakeExits()
{
	UPadelMenuRoot* Root = GetMenuRoot();
	auto Go = [Root](EPadelScreen To)
	{
		return [Root, To]()
		{
			if (Root)
			{
				Root->ShowScreen(To, EUiNavKind::Forward);
			}
		};
	};
	auto Host = [this]() { return GetHost(); };

	TArray<FExit> Out;
	switch (GetScreenId())
	{
	case EPadelScreen::Title:
		Out.Add({TEXT("Title.Continue"), LOCTEXT("Ph.Continue", "Continue"), Go(EPadelScreen::MainMenu)});
		break;
	case EPadelScreen::MainMenu:
		Out.Add({TEXT("Main.QuickMatch"), LOCTEXT("Ph.QuickMatch", "Quick Match"), Go(EPadelScreen::MatchSetup)});
		Out.Add({TEXT("Main.Practice"), LOCTEXT("Ph.Practice", "Practice"), Go(EPadelScreen::Practice)});
		Out.Add({TEXT("Main.Controls"), LOCTEXT("Ph.Controls", "Controls"), Go(EPadelScreen::Controls)});
		Out.Add({TEXT("Main.Settings"), LOCTEXT("Ph.Settings", "Settings"), Go(EPadelScreen::Settings)});
		break;
	case EPadelScreen::MatchSetup:
		Out.Add({TEXT("Setup.Start"), LOCTEXT("Ph.StartMatch", "Start match"), Go(EPadelScreen::Versus)});
		break;
	case EPadelScreen::Versus:
		Out.Add({TEXT("Versus.Skip"), LOCTEXT("Ph.Skip", "Skip"), [Host]()
		{
			if (IPadelMenuHost* H = Host())
			{
				H->MenuStartMatch();
			}
		}});
		break;
	case EPadelScreen::Paused:
		Out.Add({TEXT("Pause.Resume"), LOCTEXT("Ph.Resume", "Resume"), [Host]()
		{
			if (IPadelMenuHost* H = Host())
			{
				H->MenuResumeMatch();
			}
		}});
		Out.Add({TEXT("Pause.Restart"), LOCTEXT("Ph.Restart", "Restart match"), [Root, Host]()
		{
			FPadelConfirm Confirm;
			Confirm.Title = LOCTEXT("Dlg.Restart.Title", "Restart the match?");
			Confirm.Body = LOCTEXT("Dlg.Restart.Body", "The score goes back to 0–0.");
			Confirm.ActionLabel = LOCTEXT("Dlg.Restart.Action", "Restart");
			Confirm.SafeLabel = LOCTEXT("Dlg.KeepPlaying", "Keep playing");
			Confirm.OnAction = [Host]()
			{
				if (IPadelMenuHost* H = Host())
				{
					H->MenuRestartMatch();
				}
			};
			Root->ShowConfirm(Confirm);
		}});
		Out.Add({TEXT("Pause.Controls"), LOCTEXT("Ph.Controls", "Controls"), Go(EPadelScreen::Controls)});
		Out.Add({TEXT("Pause.Settings"), LOCTEXT("Ph.Settings", "Settings"), Go(EPadelScreen::Settings)});
		Out.Add({TEXT("Pause.Quit"), LOCTEXT("Ph.QuitToMenu", "Quit to main menu"), [Root, Host]()
		{
			FPadelConfirm Confirm;
			Confirm.Title = LOCTEXT("Dlg.QuitMatch.Title", "Quit to main menu?");
			Confirm.Body = LOCTEXT("Dlg.QuitMatch.Body", "The match will not be saved.");
			Confirm.ActionLabel = LOCTEXT("Dlg.QuitMatch.Action", "Quit match");
			Confirm.SafeLabel = LOCTEXT("Dlg.KeepPlaying", "Keep playing");
			Confirm.bDanger = true;
			Confirm.OnAction = [Host]()
			{
				if (IPadelMenuHost* H = Host())
				{
					H->MenuQuitMatch();
				}
			};
			Root->ShowConfirm(Confirm);
		}});
		break;
	case EPadelScreen::Stats:
		Out.Add({TEXT("Stats.Rematch"), LOCTEXT("Ph.Rematch", "Rematch"), Go(EPadelScreen::Versus)});
		Out.Add({TEXT("Stats.MainMenu"), LOCTEXT("Ph.MainMenu", "Main menu"), Go(EPadelScreen::MainMenu)});
		break;
	case EPadelScreen::Controls:
	case EPadelScreen::Settings:
		Out.Add({TEXT("Placeholder.Back"), LOCTEXT("Ph.Back", "Back"), [Root]()
		{
			if (Root)
			{
				Root->ReturnToOpener();
			}
		}});
		break;
	case EPadelScreen::Practice:
		Out.Add({TEXT("Practice.MainMenu"), LOCTEXT("Ph.MainMenu", "Main menu"), Go(EPadelScreen::MainMenu)});
		break;
	case EPadelScreen::PracticePaused:
		Out.Add({TEXT("PracticePause.Resume"), LOCTEXT("Ph.Resume", "Resume"), [Host]()
		{
			if (IPadelMenuHost* H = Host())
			{
				H->MenuResumeMatch();
			}
		}});
		Out.Add({TEXT("PracticePause.Quit"), LOCTEXT("Ph.QuitPractice", "Quit practice"), Go(EPadelScreen::MainMenu)});
		break;
	case EPadelScreen::Match:
		break;
	}
	return Out;
}

void UPadelPlaceholderScreen::BuildScreen()
{
	SetHeader(FText::FromString(UTF8_TO_TCHAR(Padel::Ui::ScreenName(ToUiScreen(GetScreenId())))),
		LOCTEXT("Ph.Breadcrumb", "Placeholder"));

	Exits = MakeExits();
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	std::vector<std::string> Ids;
	for (const FExit& Exit : Exits)
	{
		UPadelButton* Button = MakeElement<UPadelButton>(Exit.Id);
		Button->SetKind(EPadelButtonKind::List);
		Button->SetLabel(Exit.Label);
		List->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, PlaceholderRowGap));
		Ids.push_back(TCHAR_TO_UTF8(*Exit.Id.ToString()));
	}
	PlaceInColumns(List, 1, PlaceholderListColumns, PlaceholderListTop, 0.0f);
	FocusMap = Padel::Ui::FFocusMap::VerticalList(Ids);
}

void UPadelPlaceholderScreen::OnElementActivated(UPadelFocusableWidget* Element)
{
	for (const FExit& Exit : Exits)
	{
		if (Element && Exit.Id == Element->GetElementId() && Exit.Action)
		{
			Exit.Action();
			return;
		}
	}
}

EUiColour UPadelPlaceholderScreen::GetScrimColour() const
{
	switch (GetScreenId())
	{
	case EPadelScreen::Title:
		return EUiColour::BgScrimTitle;
	case EPadelScreen::Paused:
	case EPadelScreen::PracticePaused:
		return EUiColour::BgScrimPause;
	default:
		return EUiColour::BgScrim;
	}
}

bool UPadelPlaceholderScreen::WantsBackgroundBlur() const
{
	return GetScreenId() == EPadelScreen::Paused || GetScreenId() == EPadelScreen::PracticePaused;
}

#undef LOCTEXT_NAMESPACE
