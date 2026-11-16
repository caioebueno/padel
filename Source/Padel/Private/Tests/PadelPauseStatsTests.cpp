// Workstream C in the engine (docs/ui/MENU_SPEC.md 7.5, 7.8, 7.10, 11.5): Pause's list,
// context panel, hints and confirm dialogs, and the Stats screen's stray-press lock, tabs
// and dead B. The rules behind them are covered without the engine in
// Tests/Native/UI/UiStatsScreenTests.cpp.
#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UI/Model/PadelUiPauseView.h"
#include "UI/Model/PadelUiStatsView.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Screens/PadelPauseScreen.h"
#include "UI/Screens/PadelStatsScreen.h"
#include "UI/Screens/PadelStatsTable.h"
#include "UI/Widgets/PadelDialog.h"
#include "UI/Widgets/PadelHintBar.h"

#if WITH_DEV_AUTOMATION_TESTS

using Padel::Ui::EUiInput;

namespace PadelPauseStatsTests
{
	struct FPauseStatsTestWorld
	{
		UWorld* World = nullptr;

		FPauseStatsTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}

		~FPauseStatsTestWorld()
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

	/** Go to a screen and skip its 120 ms entry lock (not the Stats lock, which is its own). */
	void ShowUnlocked(UPadelMenuRoot* Root, EPadelScreen Screen)
	{
		Root->ShowScreen(Screen);
		if (UPadelScreenWidget* Widget = Root->GetScreenWidget())
		{
			Widget->SkipEntryAnimation();
		}
	}

	FString FocusOf(UPadelMenuRoot* Root)
	{
		return Root->GetScreenWidget() ? Root->GetScreenWidget()->GetFocusedId().ToString() : FString();
	}

	bool HasHint(UPadelScreenWidget* Screen, EUiInput Input)
	{
		for (const FPadelHint& Hint : Screen->GetHints())
		{
			if (Hint.Input == Input)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelPauseScreenTest, "Padel.UI.Pause.Screen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelPauseScreenTest::RunTest(const FString& Parameters)
{
	using namespace PadelPauseStatsTests;
	FPauseStatsTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	ShowUnlocked(Root, EPadelScreen::Paused);
	UPadelPauseScreen* Pause = Cast<UPadelPauseScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Pause is the real screen, not the placeholder"), Pause))
	{
		return false;
	}
	TestEqual(TEXT("Default focus is Resume (spec 7.5)"), FocusOf(Root), UPadelPauseScreen::ResumeId.ToString());
	TestTrue(TEXT("The match is blurred behind Pause"), Pause->WantsBackgroundBlur());
	TestEqual(TEXT("Pause uses its 0.70 scrim"), static_cast<int32>(Pause->GetScrimColour()), static_cast<int32>(Padel::Ui::EUiColour::BgScrimPause));
	TestTrue(TEXT("Hints: A Select"), HasHint(Pause, EUiInput::Confirm));
	TestTrue(TEXT("Hints: B Resume"), HasHint(Pause, EUiInput::Back));
	TestEqual(TEXT("Resume shows the match rules"), Pause->GetContextIndex(), static_cast<int32>(Padel::Ui::EPauseContext::Rules));

	// A wrapping vertical list; left and right do nothing.
	const FName Order[] = {UPadelPauseScreen::RestartId, UPadelPauseScreen::ControlsId, UPadelPauseScreen::SettingsId,
		UPadelPauseScreen::QuitId, UPadelPauseScreen::ResumeId};
	for (const FName& Expected : Order)
	{
		Root->HandleInput(EUiInput::Down);
		TestEqual(*FString::Printf(TEXT("Down reaches %s"), *Expected.ToString()), FocusOf(Root), Expected.ToString());
	}
	Root->HandleInput(EUiInput::Up);
	TestEqual(TEXT("Up from Resume wraps to Quit"), FocusOf(Root), UPadelPauseScreen::QuitId.ToString());
	TestEqual(TEXT("Quit explains that the match won't be saved"), Pause->GetContextIndex(), static_cast<int32>(Padel::Ui::EPauseContext::Quit));
	Root->HandleInput(EUiInput::Left);
	Root->HandleInput(EUiInput::Right);
	TestEqual(TEXT("Left and right do nothing"), FocusOf(Root), UPadelPauseScreen::QuitId.ToString());

	// Settings from Pause, and back to Pause with focus on Settings (spec 5.4).
	Pause->FocusElement(UPadelPauseScreen::SettingsId);
	Root->HandleInput(EUiInput::Confirm);
	TestEqual(TEXT("Settings opens from Pause"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Settings));
	Root->GetScreenWidget()->SkipEntryAnimation();
	Root->HandleInput(EUiInput::Back);
	TestEqual(TEXT("B on Settings returns to Pause"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Paused));
	TestEqual(TEXT("with focus back on Settings"), FocusOf(Root), UPadelPauseScreen::SettingsId.ToString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelPauseDialogsTest, "Padel.UI.Dialog.SafeDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelPauseDialogsTest::RunTest(const FString& Parameters)
{
	using namespace PadelPauseStatsTests;
	FPauseStatsTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	ShowUnlocked(Root, EPadelScreen::Paused);
	UPadelScreenWidget* Pause = Root->GetScreenWidget();

	// Spec 7.10: each Pause dialog opens on its safe option, and B picks it, leaving focus
	// on the button that opened it.
	for (const FName& Opener : {UPadelPauseScreen::RestartId, UPadelPauseScreen::QuitId})
	{
		Pause->FocusElement(Opener);
		Root->HandleInput(EUiInput::Confirm);
		TestTrue(*FString::Printf(TEXT("%s opens a dialog"), *Opener.ToString()), Root->IsDialogOpen());
		TestTrue(TEXT("on its safe option"), Root->GetDialog()->IsSafeFocused());
		TestTrue(TEXT("The hint bar says B goes back while it's open"), HasHint(Pause, EUiInput::Back));
		Root->HandleInput(EUiInput::Pause);
		TestTrue(TEXT("Start can't dismiss it (spec 6.12)"), Root->IsDialogOpen());
		Root->HandleInput(EUiInput::Back);
		TestFalse(TEXT("B picks the safe option"), Root->IsDialogOpen());
		TestEqual(TEXT("Still paused"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Paused));
		TestEqual(TEXT("Focus returns to the opener"), FocusOf(Root), Opener.ToString());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelStatsStrayConfirmTest, "Padel.UI.Stats.StrayConfirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelStatsStrayConfirmTest::RunTest(const FString& Parameters)
{
	using namespace PadelPauseStatsTests;
	FPauseStatsTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	ShowUnlocked(Root, EPadelScreen::Stats);
	UPadelStatsScreen* Stats = Cast<UPadelStatsScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Stats is the real screen, not the placeholder"), Stats))
	{
		return false;
	}
	TestEqual(TEXT("Default focus is Rematch (spec 7.8)"), FocusOf(Root), UPadelStatsScreen::RematchId.ToString());
	TestTrue(TEXT("Confirm starts locked"), Stats->IsConfirmLocked());
	TestFalse(TEXT("No A hint during the lock (board StatsLocked)"), HasHint(Stats, EUiInput::Confirm));

	const bool bAnimates = Stats->IsAnimating();
	Root->HandleInput(EUiInput::Confirm);
	TestEqual(TEXT("A during the animation doesn't start a rematch"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Stats));
	if (bAnimates)
	{
		TestFalse(TEXT("It finishes the animation instead"), Stats->IsAnimating());
	}
	Root->HandleInput(EUiInput::Confirm);
	TestEqual(TEXT("A during the lock does nothing"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Stats));

	Stats->AdvanceTime(PadelUi::Theme().Input.StatsConfirmLockSeconds + 0.01f);
	TestFalse(TEXT("The lock ends after StatsConfirmLockSeconds"), Stats->IsConfirmLocked());
	TestTrue(TEXT("and the A hint comes back"), HasHint(Stats, EUiInput::Confirm));
	TestFalse(TEXT("B is never hinted (it does nothing here)"), HasHint(Stats, EUiInput::Back));

	Root->HandleInput(EUiInput::Back);
	TestEqual(TEXT("B does nothing on Stats (spec 3)"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Stats));
	Root->HandleInput(EUiInput::Confirm, /*bRepeat=*/true);
	TestEqual(TEXT("A held from before (a repeat) does nothing"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Stats));

	// LB/RB change the stats tab and don't wrap.
	UPadelStatsTable* Table = Stats->GetTable();
	if (TestNotNull(TEXT("Stats has its table"), Table))
	{
		Root->HandleInput(EUiInput::TabLeft);
		TestEqual(TEXT("LB at the first tab stays"), static_cast<int32>(Table->GetTab()), static_cast<int32>(Padel::Ui::EStatsTab::Summary));
		Root->HandleInput(EUiInput::TabRight);
		TestEqual(TEXT("RB goes to SHOTS"), static_cast<int32>(Table->GetTab()), static_cast<int32>(Padel::Ui::EStatsTab::Shots));
		Root->HandleInput(EUiInput::TabRight);
		Root->HandleInput(EUiInput::TabRight);
		TestEqual(TEXT("RB stops at POINTS"), static_cast<int32>(Table->GetTab()), static_cast<int32>(Padel::Ui::EStatsTab::Points));
	}

	Root->HandleInput(EUiInput::Right);
	TestEqual(TEXT("Right moves to Main menu"), FocusOf(Root), UPadelStatsScreen::MainMenuId.ToString());
	Root->HandleInput(EUiInput::Right);
	TestEqual(TEXT("and stops there"), FocusOf(Root), UPadelStatsScreen::MainMenuId.ToString());
	Root->HandleInput(EUiInput::Left);
	Root->HandleInput(EUiInput::Confirm);
	TestEqual(TEXT("A fresh A after the lock: Rematch goes to Versus"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::Versus));
	return true;
}

#endif
