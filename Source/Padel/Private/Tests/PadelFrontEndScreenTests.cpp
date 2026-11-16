// Workstream A's screens in the engine (docs/ui/MENU_SPEC.md 7.1, 7.2, 7.9): the Title
// continues on any button but not the stick, the main menu's focus paths and locked
// tiles, and the Practice intro and pause. The rules themselves are covered without the
// engine in Tests/Native/UI/UiFrontEndTests.cpp.
#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UI/Model/PadelUiFrontEnd.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMainMenuScreen.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "UI/Screens/PadelTitleScreen.h"
#include "UI/Widgets/PadelHintBar.h"
#include "UI/Widgets/PadelMenuTile.h"
#include "UI/Widgets/PadelToast.h"

#if WITH_DEV_AUTOMATION_TESTS

using Padel::Ui::EUiInput;

namespace PadelFrontEndScreenTests
{
	struct FFrontEndTestWorld
	{
		UWorld* World = nullptr;

		FFrontEndTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}

		~FFrontEndTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	UPadelMenuRoot* MakeRoot(UWorld* World)
	{
		UPadelMenuRoot* Root = CreateWidget<UPadelMenuRoot>(World, UPadelMenuRoot::StaticClass());
		if (Root)
		{
			Root->TakeWidget();
		}
		return Root;
	}

	void Show(UPadelMenuRoot* Root, EPadelScreen Screen)
	{
		Root->ShowScreen(Screen);
		if (UPadelScreenWidget* Widget = Root->GetScreenWidget())
		{
			Widget->SkipEntryAnimation();
		}
	}

	void Press(UPadelMenuRoot* Root, EUiInput Input)
	{
		Root->HandleInput(Input, false);
		if (UPadelScreenWidget* Widget = Root->GetScreenWidget())
		{
			Widget->SkipEntryAnimation();
		}
	}

	FString FocusOf(UPadelMenuRoot* Root)
	{
		return Root->GetScreenWidget() ? Root->GetScreenWidget()->GetFocusedId().ToString() : FString();
	}

	int32 ScreenOf(UPadelMenuRoot* Root)
	{
		return static_cast<int32>(Root->GetCurrentScreen());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelTitleAnyButtonTest, "Padel.UI.Title.AnyButton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelTitleAnyButtonTest::RunTest(const FString& Parameters)
{
	using namespace PadelFrontEndScreenTests;
	FFrontEndTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}

	Show(Root, EPadelScreen::Title);
	UPadelTitleScreen* Title = Cast<UPadelTitleScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Title is the real screen"), Title))
	{
		return false;
	}
	TestTrue(TEXT("No hint bar on the title (spec 7.1)"),
		Title->GetHintBar() && Title->GetHintBar()->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("The prompt has words"), Title->GetPromptText().IsEmpty());

	// The stick and mouse movement don't count (spec 7.1).
	TestFalse(TEXT("Stick key ignored"), Title->HandleAnyKey(TEXT("Gamepad_LeftStick_Up")));
	TestFalse(TEXT("Mouse movement ignored"), Title->HandleAnyKey(TEXT("MouseX")));
	Press(Root, EUiInput::Up);
	TestEqual(TEXT("A stick direction stays on the title"), ScreenOf(Root), static_cast<int32>(EPadelScreen::Title));

	// Any other key continues: a letter nobody mapped.
	TestTrue(TEXT("A letter continues"), Title->HandleAnyKey(TEXT("G")));
	TestEqual(TEXT("Title -> Main menu"), ScreenOf(Root), static_cast<int32>(EPadelScreen::MainMenu));

	// B is a button too: it continues rather than going anywhere back (spec 3).
	Show(Root, EPadelScreen::Title);
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("B continues from the title"), ScreenOf(Root), static_cast<int32>(EPadelScreen::MainMenu));

	// During the entry lock nothing continues (spec 3).
	Root->ShowScreen(EPadelScreen::Title);
	TestFalse(TEXT("Entry lock holds any key"), Root->GetScreenWidget()->HandleAnyKey(TEXT("Enter")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMainMenuFocusTest, "Padel.UI.MainMenu.Focus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMainMenuFocusTest::RunTest(const FString& Parameters)
{
	using namespace PadelFrontEndScreenTests;
	FFrontEndTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MainMenu);
	UPadelMainMenuScreen* Menu = Cast<UPadelMainMenuScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Main menu is the real screen"), Menu))
	{
		return false;
	}
	TestTrue(TEXT("Focus map is sound"), Menu->GetFocusMap().Validate().empty());
	TestEqual(TEXT("Default focus is Quick Match"), FocusOf(Root), FString(TEXT("Main.QuickMatch")));
	UPadelMenuTile* Hero = Menu->FindTile(TEXT("Main.QuickMatch"));
	TestTrue(TEXT("Quick Match is the hero tile"), Hero && Hero->IsHero());
	UPadelMenuTile* Career = Menu->FindTile(TEXT("Main.Career"));
	TestTrue(TEXT("Career is locked"), Career && Career->IsLocked());

	// Spec 7.2's paths, around the grid.
	struct FStep
	{
		EUiInput Input;
		const TCHAR* Expected;
	};
	const FStep Steps[] = {
		{EUiInput::Right, TEXT("Main.Practice")},
		{EUiInput::Right, TEXT("Main.Practice")}, // edge: no wrap
		{EUiInput::Down, TEXT("Main.Settings")},
		{EUiInput::Down, TEXT("Main.Controls")},
		{EUiInput::Left, TEXT("Main.TeamBuilder")},
		{EUiInput::Left, TEXT("Main.Career")},
		{EUiInput::Up, TEXT("Main.QuickMatch")},
		{EUiInput::Down, TEXT("Main.Career")},
	};
	for (const FStep& Step : Steps)
	{
		Press(Root, Step.Input);
		TestEqual(*FString::Printf(TEXT("Focus after a move reaches %s"), Step.Expected), FocusOf(Root), FString(Step.Expected));
	}

	// A locked tile explains itself with a toast and goes nowhere (spec 5.3, 6.13).
	Press(Root, EUiInput::Confirm);
	TestEqual(TEXT("Locked tile stays on the main menu"), ScreenOf(Root), static_cast<int32>(EPadelScreen::MainMenu));
	TestTrue(TEXT("Locked tile shows a toast"), Root->GetToast() && Root->GetToast()->IsShowing());
	TestTrue(TEXT("The toast names the mode"), Root->GetToast()->GetMessage().ToString().Contains(TEXT("Career")));

	// Hints: A Select, B Quit (spec 7.2).
	const TArray<FPadelHint> Hints = Menu->GetHints();
	TestTrue(TEXT("Two hints, A then B"), Hints.Num() == 2 && Hints[0].Input == EUiInput::Confirm && Hints[1].Input == EUiInput::Back);

	// The tiles lead where spec 7.2 says.
	Show(Root, EPadelScreen::MainMenu);
	Press(Root, EUiInput::Confirm);
	TestEqual(TEXT("Quick Match -> Match setup"), ScreenOf(Root), static_cast<int32>(EPadelScreen::MatchSetup));
	Show(Root, EPadelScreen::MainMenu);
	Press(Root, EUiInput::Right);
	Press(Root, EUiInput::Confirm);
	TestEqual(TEXT("Practice -> Practice intro"), ScreenOf(Root), static_cast<int32>(EPadelScreen::Practice));
	Show(Root, EPadelScreen::MainMenu);
	Press(Root, EUiInput::Right);
	Press(Root, EUiInput::Down);
	Press(Root, EUiInput::Down);
	Press(Root, EUiInput::Confirm);
	TestEqual(TEXT("Controls -> Controls"), ScreenOf(Root), static_cast<int32>(EPadelScreen::Controls));
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("B on Controls returns to the main menu"), ScreenOf(Root), static_cast<int32>(EPadelScreen::MainMenu));
	TestEqual(TEXT("with focus back on Controls (spec 5.4)"), FocusOf(Root), FString(TEXT("Main.Controls")));

	// B opens Quit game? (spec 3).
	Press(Root, EUiInput::Back);
	TestTrue(TEXT("B opens the quit dialog"), Root->IsDialogOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMainMenuOptionsTest, "Padel.UI.MainMenu.Options",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMainMenuOptionsTest::RunTest(const FString& Parameters)
{
	using namespace PadelFrontEndScreenTests;
	FFrontEndTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	const PadelUi::FUiOptions Saved = PadelUi::Options();

	// Reduce motion and Text size Largest rebuild every screen in place, keeping focus.
	for (const EPadelScreen Screen : {EPadelScreen::Title, EPadelScreen::MainMenu, EPadelScreen::Practice, EPadelScreen::PracticePaused})
	{
		Show(Root, Screen);
		if (Screen == EPadelScreen::MainMenu)
		{
			Press(Root, EUiInput::Right);
		}
		const FString Before = FocusOf(Root);
		PadelUi::FUiOptions Options;
		Options.TextSize = Padel::Ui::ETextSize::Largest;
		Options.bReduceMotion = true;
		PadelUi::SetOptions(Options);
		const FString Name = UTF8_TO_TCHAR(Padel::Ui::ScreenName(ToUiScreen(Screen)));
		TestNotNull(*FString::Printf(TEXT("%s rebuilds"), *Name), Root->GetScreenWidget());
		TestEqual(*FString::Printf(TEXT("%s keeps focus"), *Name), FocusOf(Root), Before);
		PadelUi::SetOptions(Saved);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelPracticeScreensTest, "Padel.UI.Practice.IntroAndPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelPracticeScreensTest::RunTest(const FString& Parameters)
{
	using namespace PadelFrontEndScreenTests;
	FFrontEndTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}

	// The intro: Start practice focused, A Start and B Back; B goes to the main menu.
	Show(Root, EPadelScreen::Practice);
	TestEqual(TEXT("Intro opens on Start practice"), FocusOf(Root), FString(TEXT("Practice.Start")));
	const TArray<FPadelHint> IntroHints = Root->GetScreenWidget()->GetHints();
	TestTrue(TEXT("Intro hints are A Start, B Back"),
		IntroHints.Num() == 2 && IntroHints[0].Input == EUiInput::Confirm && IntroHints[1].Input == EUiInput::Back);
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("B on the intro -> Main menu"), ScreenOf(Root), static_cast<int32>(EPadelScreen::MainMenu));

	// The pause: the list wraps, Controls returns here, Quit practice leaves with no dialog.
	Show(Root, EPadelScreen::PracticePaused);
	TestEqual(TEXT("Pause opens on Resume"), FocusOf(Root), FString(TEXT("PracticePause.Resume")));
	TestTrue(TEXT("Pause blurs the court"), Root->GetScreenWidget()->WantsBackgroundBlur());
	Press(Root, EUiInput::Up);
	TestEqual(TEXT("Up from the top wraps to Quit practice"), FocusOf(Root), FString(TEXT("PracticePause.Quit")));
	Press(Root, EUiInput::Down);
	Press(Root, EUiInput::Down);
	TestEqual(TEXT("Down to Controls"), FocusOf(Root), FString(TEXT("PracticePause.Controls")));
	Press(Root, EUiInput::Confirm);
	TestEqual(TEXT("Controls opens"), ScreenOf(Root), static_cast<int32>(EPadelScreen::Controls));
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("and returns to Practice pause"), ScreenOf(Root), static_cast<int32>(EPadelScreen::PracticePaused));
	TestEqual(TEXT("on Controls"), FocusOf(Root), FString(TEXT("PracticePause.Controls")));
	Press(Root, EUiInput::Up);
	Press(Root, EUiInput::Up);
	TestEqual(TEXT("Up to Quit practice, wrapping"), FocusOf(Root), FString(TEXT("PracticePause.Quit")));
	Press(Root, EUiInput::Confirm);
	TestFalse(TEXT("Quit practice needs no confirm (spec 7.9)"), Root->IsDialogOpen());
	TestEqual(TEXT("Quit practice -> Main menu"), ScreenOf(Root), static_cast<int32>(EPadelScreen::MainMenu));
	return true;
}

#endif
