// The menu foundation in the engine (docs/ui/MENU_SPEC.md 11.5): the router walks spec 3's
// flow with focus memory, the dialog opens on its safe option, the hint bar keeps its
// order, the value controls keep their edge rules, and every screen's focus map is sound.
// The rules themselves are covered without the engine in Tests/Native/UI.
#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Screens/PadelScreenWidget.h"
#include "UI/Widgets/PadelDialog.h"
#include "UI/Widgets/PadelHintBar.h"
#include "UI/Widgets/PadelSegmented.h"
#include "UI/Widgets/PadelSelector.h"
#include "UI/Widgets/PadelToggle.h"

#if WITH_DEV_AUTOMATION_TESTS

using Padel::Ui::EUiInput;

namespace PadelMenuFoundationTests
{
	struct FMenuTestWorld
	{
		UWorld* World = nullptr;

		FMenuTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}

		~FMenuTestWorld()
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

	/** Go to a screen and skip its entry lock, as if 120 ms had passed. */
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
		// A press that changed screen lands on a new, locked screen; unlock it like time would.
		if (UPadelScreenWidget* Widget = Root->GetScreenWidget())
		{
			Widget->SkipEntryAnimation();
		}
	}

	FString FocusOf(UPadelMenuRoot* Root)
	{
		return Root->GetScreenWidget() ? Root->GetScreenWidget()->GetFocusedId().ToString() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMenuFlowTest, "Padel.UI.Foundation.Flow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMenuFlowTest::RunTest(const FString& Parameters)
{
	using namespace PadelMenuFoundationTests;
	FMenuTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}

	Root->ShowScreen(EPadelScreen::Title);
	TestTrue(TEXT("Title shows a screen"), Root->GetScreenWidget() != nullptr);
	TestTrue(TEXT("Entry lock holds input (spec 3)"), Root->GetScreenWidget()->IsInputLocked());
	Root->HandleInput(EUiInput::Confirm);
	TestEqual(TEXT("A during the entry lock does nothing"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Title));

	Root->GetScreenWidget()->SkipEntryAnimation();
	Press(Root, EUiInput::Confirm);
	TestEqual(TEXT("Title -> Main menu"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::MainMenu));
	const FString MainDefault = FocusOf(Root);
	TestFalse(TEXT("Main menu has a default focus"), MainDefault.IsEmpty());

	// Move off the default, open Settings, come back: focus returns where it was (spec 5.4).
	Press(Root, EUiInput::Down);
	const FString Moved = FocusOf(Root);
	TestNotEqual(TEXT("Down moves focus"), Moved, MainDefault);
	Show(Root, EPadelScreen::Settings);
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("B on Settings returns to its opener"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::MainMenu));
	TestEqual(TEXT("Focus restored on the way back"), FocusOf(Root), Moved);

	// Settings opened from Pause goes back to Pause, not the main menu.
	Show(Root, EPadelScreen::Paused);
	Show(Root, EPadelScreen::Settings);
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("Settings returns to Pause"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Paused));
	Show(Root, EPadelScreen::MainMenu);

	// B on the main menu asks before quitting, and B again keeps the game open.
	Press(Root, EUiInput::Back);
	TestTrue(TEXT("Main menu B opens Quit game?"), Root->IsDialogOpen());
	TestTrue(TEXT("The dialog opens on the safe option"), Root->GetDialog()->IsSafeFocused());
	Press(Root, EUiInput::Back);
	TestFalse(TEXT("B picks the safe option"), Root->IsDialogOpen());
	TestEqual(TEXT("Still on the main menu"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::MainMenu));

	// A fresh forward open starts on the default, not the remembered element.
	Show(Root, EPadelScreen::MatchSetup);
	Press(Root, EUiInput::Back);
	TestEqual(TEXT("B on Setup -> Main menu"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::MainMenu));

	Show(Root, EPadelScreen::Match);
	TestFalse(TEXT("The match shows no menu"), Root->IsShowingMenu());
	TestTrue(TEXT("Start may open Pause during play"), Root->GetFlow().CanOpenPause(false, false));
	TestFalse(TEXT("Not over the golden point prompt"), Root->GetFlow().CanOpenPause(true, false));
	Show(Root, EPadelScreen::Paused);
	TestFalse(TEXT("No Pause over Pause"), Root->GetFlow().CanOpenPause(false, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMenuDialogTest, "Padel.UI.Foundation.Dialog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMenuDialogTest::RunTest(const FString& Parameters)
{
	using namespace PadelMenuFoundationTests;
	FMenuTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MainMenu);

	int32 Actions = 0;
	int32 Safes = 0;
	FPadelConfirm Confirm;
	Confirm.Title = FText::FromString(TEXT("Quit to main menu?"));
	Confirm.ActionLabel = FText::FromString(TEXT("Quit match"));
	Confirm.SafeLabel = FText::FromString(TEXT("Keep playing"));
	Confirm.bDanger = true;
	Confirm.OnAction = [&Actions]() { ++Actions; };
	Confirm.OnSafe = [&Safes]() { ++Safes; };

	Root->ShowConfirm(Confirm);
	TestTrue(TEXT("Opens on the safe option (spec 6.12)"), Root->GetDialog()->IsSafeFocused());
	Root->HandleInput(EUiInput::Pause);
	TestTrue(TEXT("Start can't dismiss a dialog"), Root->IsDialogOpen());
	Root->HandleInput(EUiInput::Down);
	TestEqual(TEXT("The screen underneath hears nothing"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::MainMenu));
	Root->HandleInput(EUiInput::Left);
	TestFalse(TEXT("Left moves to the action"), Root->GetDialog()->IsSafeFocused());
	Root->HandleInput(EUiInput::Back);
	TestEqual(TEXT("B picks the safe option even with the action focused"), Safes, 1);
	TestEqual(TEXT("and not the action"), Actions, 0);

	Root->ShowConfirm(Confirm);
	Root->HandleInput(EUiInput::Left);
	Root->HandleInput(EUiInput::Confirm);
	TestEqual(TEXT("A on the action runs it"), Actions, 1);
	TestFalse(TEXT("and closes the dialog"), Root->IsDialogOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMenuHintBarTest, "Padel.UI.Foundation.HintOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMenuHintBarTest::RunTest(const FString& Parameters)
{
	using namespace PadelMenuFoundationTests;
	FMenuTestWorld Test;
	UPadelHintBar* Bar = CreateWidget<UPadelHintBar>(Test.World, UPadelHintBar::StaticClass());
	if (!TestNotNull(TEXT("Hint bar created"), Bar))
	{
		return false;
	}
	Bar->TakeWidget();
	Bar->SetHints({{EUiInput::Back, FText::FromString(TEXT("Back"))}, {EUiInput::Secondary, FText::FromString(TEXT("Reset"))},
		{EUiInput::TabRight, FText::FromString(TEXT("Change tab"))}, {EUiInput::Confirm, FText::FromString(TEXT("Select"))}});
	const TArray<EUiInput> Expected = {EUiInput::Confirm, EUiInput::Secondary, EUiInput::TabLeft, EUiInput::Back};
	TestTrue(TEXT("A, X, LB RB, B whatever the screen listed (spec 6.1)"), Bar->GetDisplayedInputs() == Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMenuControlsTest, "Padel.UI.Foundation.Controls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMenuControlsTest::RunTest(const FString& Parameters)
{
	using namespace PadelMenuFoundationTests;
	FMenuTestWorld Test;

	UPadelSelector* Selector = CreateWidget<UPadelSelector>(Test.World, UPadelSelector::StaticClass());
	UPadelToggle* Toggle = CreateWidget<UPadelToggle>(Test.World, UPadelToggle::StaticClass());
	UPadelSegmented* Segmented = CreateWidget<UPadelSegmented>(Test.World, UPadelSegmented::StaticClass());
	if (!TestNotNull(TEXT("Selector"), Selector) || !TestNotNull(TEXT("Toggle"), Toggle) || !TestNotNull(TEXT("Segmented"), Segmented))
	{
		return false;
	}
	Selector->TakeWidget();
	Toggle->TakeWidget();
	Segmented->TakeWidget();

	int32 Edges = 0;
	Selector->OnEdgeReached.AddLambda([&Edges](UPadelFocusableWidget*) { ++Edges; });
	Selector->SetOptions({FText::FromString(TEXT("Easy")), FText::FromString(TEXT("Medium")), FText::FromString(TEXT("Hard"))});
	Selector->SetIndex(0);
	TestTrue(TEXT("Left at the start is used"), Selector->HandleInput(EUiInput::Left, false));
	TestEqual(TEXT("but doesn't wrap (spec 6.8)"), Selector->GetIndex(), 0);
	TestEqual(TEXT("and plays the edge"), Edges, 1);
	Selector->HandleInput(EUiInput::Right, false);
	Selector->HandleInput(EUiInput::Right, false);
	TestEqual(TEXT("Right steps"), Selector->GetIndex(), 2);
	Selector->Activate();
	TestEqual(TEXT("A wraps forward"), Selector->GetIndex(), 0);
	TestFalse(TEXT("Up isn't the selector's"), Selector->HandleInput(EUiInput::Up, false));

	Toggle->SetOn(false);
	Toggle->HandleInput(EUiInput::Right, false);
	TestTrue(TEXT("Right means On (spec 6.7)"), Toggle->IsOn());
	Toggle->HandleInput(EUiInput::Right, false);
	TestTrue(TEXT("Right again stays On"), Toggle->IsOn());
	Toggle->Activate();
	TestFalse(TEXT("A toggles"), Toggle->IsOn());

	Segmented->SetOptions({FText::FromString(TEXT("1 set")), FText::FromString(TEXT("Best of 3"))});
	Segmented->SetIndex(0);
	Segmented->HandleInput(EUiInput::Right, false);
	TestEqual(TEXT("Right selects directly (spec 6.9)"), Segmented->GetIndex(), 1);
	Segmented->Activate();
	TestEqual(TEXT("A does nothing extra"), Segmented->GetIndex(), 1);
	Segmented->HandleInput(EUiInput::Right, false);
	TestEqual(TEXT("No wrap"), Segmented->GetIndex(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMenuFocusMapsTest, "Padel.UI.Foundation.FocusMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMenuFocusMapsTest::RunTest(const FString& Parameters)
{
	using namespace PadelMenuFoundationTests;
	FMenuTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	// Every menu screen builds, has a sound focus map and lands focus somewhere (spec 5.3).
	for (int32 Index = 0; Index < static_cast<int32>(Padel::Ui::EUiScreen::Count); ++Index)
	{
		const EPadelScreen Screen = static_cast<EPadelScreen>(Index);
		if (!Padel::Ui::IsMenuScreen(ToUiScreen(Screen)))
		{
			continue;
		}
		Show(Root, Screen);
		const FString Name = UTF8_TO_TCHAR(Padel::Ui::ScreenName(ToUiScreen(Screen)));
		UPadelScreenWidget* Widget = Root->GetScreenWidget();
		if (!TestNotNull(*FString::Printf(TEXT("%s builds"), *Name), Widget))
		{
			continue;
		}
		const std::vector<std::string> Problems = Widget->GetFocusMap().Validate();
		TestTrue(*FString::Printf(TEXT("%s focus map is sound%s"), *Name,
			Problems.empty() ? TEXT("") : *FString::Printf(TEXT(": %s"), UTF8_TO_TCHAR(Problems.front().c_str())))
			, Problems.empty());
		TestFalse(*FString::Printf(TEXT("%s focuses an element"), *Name), Widget->GetFocusedId().IsNone());
		TestTrue(*FString::Printf(TEXT("%s has hints"), *Name), Widget->GetHints().Num() > 0);
	}
	return true;
}

#endif
