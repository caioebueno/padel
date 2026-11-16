// Workstream D in the engine (docs/ui/MENU_SPEC.md 6.10, 7.6, 7.7, 11.5): Settings tabs,
// the X Reset rule and dialog, accessibility options rebuilding the screen with focus
// kept, the slider's steps, persistence, and the Controls tabs and labels. The value rules
// themselves are covered without the engine in Tests/Native/UI/UiSettingsControlsTests.cpp.
#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UI/PadelSettings.h"
#include "UI/PadelUiTokens.h"
#include "UI/Screens/PadelControlsScreen.h"
#include "UI/Screens/PadelMenuRoot.h"
#include "UI/Screens/PadelSettingsScreen.h"
#include "UI/Widgets/PadelDialog.h"
#include "UI/Widgets/PadelSlider.h"

#if WITH_DEV_AUTOMATION_TESTS

using Padel::Ui::ESetting;
using Padel::Ui::ESettingsTab;
using Padel::Ui::EUiInput;

namespace PadelSettingsControlsTests
{
	struct FSettingsTestWorld
	{
		UWorld* World = nullptr;

		FSettingsTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
		}

		~FSettingsTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	/** Puts the player's settings and menu options back however a test ends. */
	struct FRestoreSettings
	{
		Padel::Ui::FSettingsValues Values;
		PadelUi::FUiOptions Options;

		FRestoreSettings()
			: Values(UPadelSettings::Get()->GetMenuValues())
			, Options(PadelUi::Options())
		{
			// Start every test from the defaults.
			UPadelSettings::Get()->SetMenuValues(Padel::Ui::FSettingsValues());
		}

		~FRestoreSettings()
		{
			UPadelSettings::Get()->SetMenuValues(Values);
			PadelUi::SetOptions(Options);
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

	FString FocusOf(UPadelMenuRoot* Root)
	{
		return Root->GetScreenWidget() ? Root->GetScreenWidget()->GetFocusedId().ToString() : FString();
	}

	bool HasHint(UPadelMenuRoot* Root, EUiInput Input)
	{
		if (!Root->GetScreenWidget())
		{
			return false;
		}
		for (const FPadelHint& Hint : Root->GetScreenWidget()->GetHints())
		{
			if (Hint.Input == Input)
			{
				return true;
			}
		}
		return false;
	}

	UPadelSettingsScreen* SettingsOf(UPadelMenuRoot* Root)
	{
		return Cast<UPadelSettingsScreen>(Root->GetScreenWidget());
	}

	FString RowIdString(ESetting Setting)
	{
		return UPadelSettingsScreen::RowId(Setting).ToString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSettingsTabsTest, "Padel.UI.Settings.Tabs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSettingsTabsTest::RunTest(const FString& Parameters)
{
	using namespace PadelSettingsControlsTests;
	FSettingsTestWorld Test;
	FRestoreSettings Restore;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MainMenu);
	Show(Root, EPadelScreen::Settings);
	UPadelSettingsScreen* Screen = SettingsOf(Root);
	if (!TestNotNull(TEXT("Settings is the real screen"), Screen))
	{
		return false;
	}
	TestEqual(TEXT("Opens on Audio"), static_cast<int32>(Screen->GetTab()), static_cast<int32>(ESettingsTab::Audio));
	TestEqual(TEXT("Focus on the first row"), FocusOf(Root), RowIdString(ESetting::MasterVolume));
	TestFalse(TEXT("A slider has no A hint (A does nothing on it)"), HasHint(Root, EUiInput::Confirm));
	TestFalse(TEXT("No X Reset hint at the defaults"), HasHint(Root, EUiInput::Secondary));
	TestTrue(TEXT("LB RB hint"), HasHint(Root, EUiInput::TabLeft));
	TestTrue(TEXT("B hint"), HasHint(Root, EUiInput::Back));

	Root->HandleInput(EUiInput::Up);
	TestNotEqual(TEXT("Up from the first row wraps (spec 7.7)"), FocusOf(Root), RowIdString(ESetting::MasterVolume));

	Root->HandleInput(EUiInput::TabRight);
	TestEqual(TEXT("RB: Accessibility"), static_cast<int32>(Screen->GetTab()), static_cast<int32>(ESettingsTab::Accessibility));
	TestEqual(TEXT("Changing tab focuses its first row (6.11)"), FocusOf(Root), RowIdString(ESetting::TextSize));
	TestTrue(TEXT("A Change on a selector"), HasHint(Root, EUiInput::Confirm));
	Root->HandleInput(EUiInput::TabRight);
	TestEqual(TEXT("Tabs don't wrap"), static_cast<int32>(Screen->GetTab()), static_cast<int32>(ESettingsTab::Accessibility));
	Root->HandleInput(EUiInput::TabLeft);
	TestEqual(TEXT("LB: back to Audio"), static_cast<int32>(Screen->GetTab()), static_cast<int32>(ESettingsTab::Audio));

	Root->HandleInput(EUiInput::Back);
	TestEqual(TEXT("B returns to the opener"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::MainMenu));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSettingsResetTest, "Padel.UI.Settings.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSettingsResetTest::RunTest(const FString& Parameters)
{
	using namespace PadelSettingsControlsTests;
	FSettingsTestWorld Test;
	FRestoreSettings Restore;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MainMenu);
	Show(Root, EPadelScreen::Settings);
	UPadelSettingsScreen* Screen = SettingsOf(Root);
	if (!TestNotNull(TEXT("Settings is the real screen"), Screen))
	{
		return false;
	}

	Root->HandleInput(EUiInput::Secondary);
	TestFalse(TEXT("X does nothing at the defaults"), Root->IsDialogOpen());

	Root->HandleInput(EUiInput::Left);
	TestEqual(TEXT("Left steps the master volume by 5"), Screen->GetValues().Get(ESetting::MasterVolume), 75);
	TestEqual(TEXT("and applies at once"), UPadelSettings::Get()->GetMenuValues().Get(ESetting::MasterVolume), 75);
	TestTrue(TEXT("X Reset hint appears"), HasHint(Root, EUiInput::Secondary));

	Root->HandleInput(EUiInput::Secondary);
	TestTrue(TEXT("X asks first (Dlg.ResetTab)"), Root->IsDialogOpen());
	TestTrue(TEXT("on the safe option"), Root->GetDialog()->IsSafeFocused());
	Root->HandleInput(EUiInput::Back);
	TestFalse(TEXT("B cancels"), Root->IsDialogOpen());
	TestEqual(TEXT("Cancel keeps the value"), Screen->GetValues().Get(ESetting::MasterVolume), 75);

	Root->HandleInput(EUiInput::Secondary);
	Root->HandleInput(EUiInput::Left);
	Root->HandleInput(EUiInput::Confirm);
	TestFalse(TEXT("Reset closes the dialog"), Root->IsDialogOpen());
	TestEqual(TEXT("Reset restores the default"), Screen->GetValues().Get(ESetting::MasterVolume), 80);
	TestEqual(TEXT("and applies it"), UPadelSettings::Get()->GetMenuValues().Get(ESetting::MasterVolume), 80);
	TestFalse(TEXT("The Reset hint is gone"), HasHint(Root, EUiInput::Secondary));
	TestEqual(TEXT("Focus stays on the row"), FocusOf(Root), RowIdString(ESetting::MasterVolume));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSettingsAccessRebuildTest, "Padel.UI.Settings.AccessRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSettingsAccessRebuildTest::RunTest(const FString& Parameters)
{
	using namespace PadelSettingsControlsTests;
	FSettingsTestWorld Test;
	FRestoreSettings Restore;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MainMenu);
	Show(Root, EPadelScreen::Settings);
	Root->HandleInput(EUiInput::TabRight);
	Root->HandleInput(EUiInput::Down);
	TestEqual(TEXT("On Reduce motion"), FocusOf(Root), RowIdString(ESetting::ReduceMotion));

	const UPadelScreenWidget* Before = Root->GetScreenWidget();
	Root->HandleInput(EUiInput::Confirm);
	TestTrue(TEXT("A turns Reduce motion on for every menu"), PadelUi::Options().bReduceMotion);
	TestTrue(TEXT("The root rebuilt the screen"), Root->GetScreenWidget() != Before);
	UPadelSettingsScreen* Screen = SettingsOf(Root);
	if (!TestNotNull(TEXT("Still Settings"), Screen))
	{
		return false;
	}
	TestEqual(TEXT("Still on Accessibility"), static_cast<int32>(Screen->GetTab()), static_cast<int32>(ESettingsTab::Accessibility));
	TestEqual(TEXT("Focus survives the rebuild"), FocusOf(Root), RowIdString(ESetting::ReduceMotion));
	TestTrue(TEXT("The rebuilt screen shows the new value"), Screen->GetValues().ReduceMotion());

	// Text size Largest: the screen still builds and keeps its place.
	Root->HandleInput(EUiInput::Up);
	Root->HandleInput(EUiInput::Right);
	Root->HandleInput(EUiInput::Right);
	TestEqual(TEXT("Text size Largest"), static_cast<int32>(PadelUi::Options().TextSize), static_cast<int32>(Padel::Ui::ETextSize::Largest));
	TestEqual(TEXT("Focus on Text size after two rebuilds"), FocusOf(Root), RowIdString(ESetting::TextSize));
	TestTrue(TEXT("X Reset hint, since the tab isn't default"), HasHint(Root, EUiInput::Secondary));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSettingsSliderTest, "Padel.UI.Settings.Slider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSettingsSliderTest::RunTest(const FString& Parameters)
{
	using namespace PadelSettingsControlsTests;
	FSettingsTestWorld Test;
	FRestoreSettings Restore;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MainMenu);
	Show(Root, EPadelScreen::Settings);
	UPadelSlider* Slider = Cast<UPadelSlider>(Root->GetScreenWidget()->GetFocusedElement());
	if (!TestNotNull(TEXT("Master volume is a slider"), Slider))
	{
		return false;
	}
	TestTrue(TEXT("It asks for the faster repeat (spec 5.3)"),
		FMath::IsNearlyEqual(Slider->GetHeldRepeatRate(), PadelUi::Theme().Input.SliderRepeatRate));
	Root->HandleInput(EUiInput::Right);
	TestEqual(TEXT("A press steps by 5"), Slider->GetValue(), 85);
	Root->HandleInput(EUiInput::Right, true);
	TestEqual(TEXT("A repeat steps by 1"), Slider->GetValue(), 86);
	Root->HandleInput(EUiInput::Confirm);
	TestEqual(TEXT("A does nothing on a slider"), Slider->GetValue(), 86);
	for (int32 Press = 0; Press < 10; ++Press)
	{
		Root->HandleInput(EUiInput::Right);
	}
	TestEqual(TEXT("Stops at 100"), Slider->GetValue(), 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSettingsPersistTest, "Padel.UI.Settings.Persist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSettingsPersistTest::RunTest(const FString& Parameters)
{
	using namespace PadelSettingsControlsTests;
	FRestoreSettings Restore;
	UPadelSettings* Settings = UPadelSettings::Get();
	if (!TestNotNull(TEXT("Settings object"), Settings))
	{
		return false;
	}
	TestTrue(TEXT("UPadelSettings is the engine's game user settings (DefaultEngine.ini)"),
		GEngine && Cast<UPadelSettings>(GEngine->GetGameUserSettings()) != nullptr);

	Padel::Ui::FSettingsValues Changed;
	Changed.Set(ESetting::CrowdVolume, 35);
	Changed.Set(ESetting::ReduceTransparency, 1);
	Settings->SetMenuValues(Changed);
	Padel::Ui::FMatchSetup Setup;
	Setup.Difficulty = Padel::Ui::EDifficulty::Hard;
	Setup.SetsToWin = 1;
	Setup.bGoldenPoint = false;
	Setup.HumanPlayerId = 0;
	Settings->StoreMatchSetup(Setup);
	Settings->SaveMenuSettings();

	// A fresh object reads GameUserSettings.ini back.
	UPadelSettings* Fresh = NewObject<UPadelSettings>(GetTransientPackage());
	Fresh->LoadConfig(UPadelSettings::StaticClass(), *GGameUserSettingsIni);
	TestEqual(TEXT("Crowd volume survives a reload"), Fresh->GetMenuValues().Get(ESetting::CrowdVolume), 35);
	TestTrue(TEXT("Reduce transparency survives a reload"), Fresh->GetMenuValues().ReduceTransparency());
	Padel::Ui::FMatchSetup Loaded;
	TestTrue(TEXT("A saved setup loads"), Fresh->LoadMatchSetup(Loaded));
	TestEqual(TEXT("Difficulty"), static_cast<int32>(Loaded.Difficulty), static_cast<int32>(Padel::Ui::EDifficulty::Hard));
	TestEqual(TEXT("Format"), Loaded.SetsToWin, 1);
	TestFalse(TEXT("Golden point"), Loaded.bGoldenPoint);
	TestEqual(TEXT("Player"), Loaded.HumanPlayerId, 0);

	// Put the file back as it was.
	Settings->SetMenuValues(Restore.Values);
	Settings->SaveMenuSettings();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelControlsTabsTest, "Padel.UI.Controls.Tabs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelControlsTabsTest::RunTest(const FString& Parameters)
{
	using namespace PadelSettingsControlsTests;
	FSettingsTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::MainMenu);
	Root->SetPadFamily(Padel::Ui::EPadFamily::KeyboardMouse);
	Show(Root, EPadelScreen::Controls);
	UPadelControlsScreen* Screen = Cast<UPadelControlsScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Controls is the real screen"), Screen))
	{
		return false;
	}
	TestEqual(TEXT("Keyboard last: opens on the Keyboard tab"), Screen->GetTabIndex(), 1);
	TestEqual(TEXT("The drawing holds focus"), FocusOf(Root), UPadelControlsScreen::SheetId.ToString());
	TestFalse(TEXT("No A hint: nothing to select"), HasHint(Root, EUiInput::Confirm));
	TestTrue(TEXT("LB RB hint"), HasHint(Root, EUiInput::TabLeft));
	TestTrue(TEXT("B hint"), HasHint(Root, EUiInput::Back));
	Root->HandleInput(EUiInput::TabLeft);
	TestEqual(TEXT("LB: Gamepad"), Screen->GetTabIndex(), 0);
	Root->HandleInput(EUiInput::TabLeft);
	TestEqual(TEXT("Tabs don't wrap"), Screen->GetTabIndex(), 0);
	Root->HandleInput(EUiInput::Back);
	TestEqual(TEXT("B returns to the opener"), static_cast<int32>(Root->GetCurrentScreen()), static_cast<int32>(EPadelScreen::MainMenu));

	Root->SetPadFamily(Padel::Ui::EPadFamily::PlayStation);
	Show(Root, EPadelScreen::Controls);
	Screen = Cast<UPadelControlsScreen>(Root->GetScreenWidget());
	TestTrue(TEXT("Pad last: opens on the Gamepad tab"), Screen && Screen->GetTabIndex() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelControlsPracticeTest, "Padel.UI.Controls.Practice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelControlsPracticeTest::RunTest(const FString& Parameters)
{
	using namespace PadelSettingsControlsTests;
	FSettingsTestWorld Test;
	UPadelMenuRoot* Root = MakeRoot(Test.World);
	if (!TestNotNull(TEXT("Menu root created"), Root))
	{
		return false;
	}
	Show(Root, EPadelScreen::Paused);
	Show(Root, EPadelScreen::Controls);
	UPadelControlsScreen* FromPause = Cast<UPadelControlsScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Controls from Pause"), FromPause))
	{
		return false;
	}
	TestTrue(TEXT("Over the match: the Pause scrim and blur"), FromPause->WantsBackgroundBlur());
	const FString MatchLabel = FromPause->GetActionLabel(EPadelInputAction::CmdNet).ToString();

	Show(Root, EPadelScreen::PracticePaused);
	Show(Root, EPadelScreen::Controls);
	UPadelControlsScreen* FromPractice = Cast<UPadelControlsScreen>(Root->GetScreenWidget());
	if (!TestNotNull(TEXT("Controls from Practice pause"), FromPractice))
	{
		return false;
	}
	TestNotEqual(TEXT("D-pad rows show the practice controls (spec 7.6)"),
		FromPractice->GetActionLabel(EPadelInputAction::CmdNet).ToString(), MatchLabel);
	TestEqual(TEXT("Shots keep their labels"), FromPractice->GetActionLabel(EPadelInputAction::ShotLob).ToString(),
		FromPause->GetActionLabel(EPadelInputAction::ShotLob).ToString());
	return true;
}

#endif
