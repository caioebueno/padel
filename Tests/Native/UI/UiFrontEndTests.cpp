// The front end's engine-free rules (docs/ui/MENU_SPEC.md 7.1, 7.2, 7.9): which keys leave
// the title, the prompt's wording and pulse, the main menu grid and focus map, the entry
// stagger and the practice control rows.
#include "doctest.h"

#include "UI/Model/PadelUiFrontEnd.h"
#include "UI/Model/PadelUiTheme.h"

#include <cmath>
#include <set>
#include <string>

using namespace Padel::Ui;

TEST_CASE("UI.Title: any key or button continues, but not the sticks or mouse movement (spec 7.1)")
{
	for (const char* Key : {"Gamepad_FaceButton_Bottom", "Gamepad_FaceButton_Right", "Gamepad_DPad_Up", "Gamepad_Special_Right",
			 "Gamepad_LeftShoulder", "Gamepad_LeftTrigger", "Gamepad_LeftThumbstick", "Enter", "SpaceBar", "Escape", "A", "F",
			 "LeftMouseButton", "RightMouseButton"})
	{
		CAPTURE(Key);
		CHECK(CountsAsAnyButton(Key));
	}
	for (const char* Key : {"Gamepad_LeftStick_Up", "Gamepad_LeftStick_Right", "Gamepad_RightStick_Down", "Gamepad_LeftX",
			 "Gamepad_RightY", "Gamepad_LeftTriggerAxis", "MouseX", "MouseY", "MouseWheelAxis", "LeftAlt", "LeftCommand", ""})
	{
		CAPTURE(Key);
		CHECK_FALSE(CountsAsAnyButton(Key));
	}
}

TEST_CASE("UI.Title: the prompt says button for a pad and key for a keyboard (spec 7.1)")
{
	CHECK(TitleShowsPadPrompt(EPadFamily::Xbox, false));
	CHECK(TitleShowsPadPrompt(EPadFamily::PlayStation, true));
	CHECK(TitleShowsPadPrompt(EPadFamily::KeyboardMouse, true));
	CHECK_FALSE(TitleShowsPadPrompt(EPadFamily::KeyboardMouse, false));
}

TEST_CASE("UI.Title: the prompt pulses 1.0 to 0.5 over 1.6 s, steady with Reduce motion (spec 7.1, 9)")
{
	const FUiMotion Motion = FUiTheme::Defaults().Motion;
	CHECK(Motion.Pulse == doctest::Approx(1.6f));
	CHECK(Motion.PulseMinAlpha == doctest::Approx(0.5f));
	CHECK(PulseAlpha(0.0f, Motion.Pulse, Motion.PulseMinAlpha, false) == doctest::Approx(1.0f));
	CHECK(PulseAlpha(0.8f, Motion.Pulse, Motion.PulseMinAlpha, false) == doctest::Approx(0.5f));
	CHECK(PulseAlpha(1.6f, Motion.Pulse, Motion.PulseMinAlpha, false) == doctest::Approx(1.0f));
	const float Quarter = PulseAlpha(0.4f, Motion.Pulse, Motion.PulseMinAlpha, false);
	CHECK(Quarter == doctest::Approx(0.75f));
	for (float T = 0.0f; T < 4.0f; T += 0.05f)
	{
		const float Alpha = PulseAlpha(T, Motion.Pulse, Motion.PulseMinAlpha, false);
		CHECK(Alpha >= 0.5f - 1e-4f);
		CHECK(Alpha <= 1.0f + 1e-4f);
		CHECK(PulseAlpha(T, Motion.Pulse, Motion.PulseMinAlpha, true) == doctest::Approx(1.0f));
	}
}

TEST_CASE("UI.Title: the underline draws in over 400 ms, at once with Reduce motion (spec 7.1)")
{
	const float Duration = FUiTheme::Defaults().Motion.TitleUnderline;
	CHECK(Duration == doctest::Approx(0.4f));
	CHECK(UnderlineProgress(0.0f, Duration, false) == doctest::Approx(0.0f));
	CHECK(UnderlineProgress(0.2f, Duration, false) > 0.5f);
	CHECK(UnderlineProgress(0.4f, Duration, false) == doctest::Approx(1.0f));
	CHECK(UnderlineProgress(5.0f, Duration, false) == doctest::Approx(1.0f));
	CHECK(UnderlineProgress(0.0f, Duration, true) == doctest::Approx(1.0f));
}

TEST_CASE("UI.MainMenu: six tiles on the spec 7.2 grid, inside the content area")
{
	const FUiTheme Theme = FUiTheme::Defaults();
	const std::vector<FMainMenuTile>& Tiles = MainMenuTiles();
	REQUIRE(Tiles.size() == 6);
	CHECK(std::string(Tiles[0].Id) == "Main.QuickMatch");
	CHECK(Tiles[0].bHero);
	// Hero 8 columns x 520 px (1144 x 520), small 4 columns (560 wide).
	CHECK(Theme.SpanWidth(Tiles[0].Span) == doctest::Approx(1144.0f));
	CHECK(Tiles[0].Height == doctest::Approx(520.0f));
	int Locked = 0;
	std::set<std::string> Ids;
	for (const FMainMenuTile& Tile : Tiles)
	{
		CAPTURE(Tile.Id);
		Ids.insert(Tile.Id);
		CHECK(Tile.Top >= 184.0f);
		CHECK(Tile.Top + Tile.Height <= 936.0f);
		CHECK(Tile.Column + Tile.Span - 1 <= 12);
		if (!Tile.bHero)
		{
			CHECK(Theme.SpanWidth(Tile.Span) == doctest::Approx(560.0f));
		}
		// Locked tiles go nowhere; every other tile has a screen.
		CHECK(Tile.bLocked == (Tile.Target == EUiScreen::Count));
		Locked += Tile.bLocked ? 1 : 0;
		// The bottom row anchors to the bottom edge (spec 4.4).
		CHECK(Tile.bBottomRow == (Tile.Top >= 728.0f));
	}
	CHECK(Locked == 2);
	CHECK(Ids.count("Main.Career") == 1);
	CHECK(Ids.count("Main.TeamBuilder") == 1);
	// No two tiles overlap.
	for (size_t A = 0; A < Tiles.size(); ++A)
	{
		for (size_t B = A + 1; B < Tiles.size(); ++B)
		{
			const float AX = Theme.ColumnX(Tiles[A].Column);
			const float BX = Theme.ColumnX(Tiles[B].Column);
			const bool bApartX = AX + Theme.SpanWidth(Tiles[A].Span) <= BX || BX + Theme.SpanWidth(Tiles[B].Span) <= AX;
			const bool bApartY = Tiles[A].Top + Tiles[A].Height <= Tiles[B].Top || Tiles[B].Top + Tiles[B].Height <= Tiles[A].Top;
			CHECK((bApartX || bApartY));
		}
	}
}

TEST_CASE("UI.MainMenu: the focus map follows spec 7.2, with every tile in it")
{
	const FFocusMap Map = MainMenuFocusMap();
	CHECK(Map.Validate().empty());
	CHECK(Map.Default() == "Main.QuickMatch");
	for (const FMainMenuTile& Tile : MainMenuTiles())
	{
		CHECK(Map.Contains(Tile.Id));
	}
	auto Move = [&Map](const char* From, EFocusDir Dir) { return Map.Move(From, Dir).value_or("-"); };
	CHECK(Move("Main.QuickMatch", EFocusDir::Right) == "Main.Practice");
	CHECK(Move("Main.QuickMatch", EFocusDir::Down) == "Main.Career");
	CHECK(Move("Main.Practice", EFocusDir::Down) == "Main.Settings");
	CHECK(Move("Main.Settings", EFocusDir::Left) == "Main.QuickMatch");
	CHECK(Move("Main.Settings", EFocusDir::Down) == "Main.Controls");
	CHECK(Move("Main.Career", EFocusDir::Right) == "Main.TeamBuilder");
	CHECK(Move("Main.TeamBuilder", EFocusDir::Up) == "Main.QuickMatch");
	CHECK(Move("Main.TeamBuilder", EFocusDir::Right) == "Main.Controls");
	CHECK(Move("Main.Controls", EFocusDir::Up) == "Main.Settings");
	CHECK(Move("Main.Controls", EFocusDir::Left) == "Main.TeamBuilder");
	// A grid doesn't wrap (spec 5.3).
	CHECK(Move("Main.QuickMatch", EFocusDir::Up) == "-");
	CHECK(Move("Main.QuickMatch", EFocusDir::Left) == "-");
	CHECK(Move("Main.Practice", EFocusDir::Right) == "-");
	CHECK(Move("Main.Career", EFocusDir::Down) == "-");
	CHECK(Move("Main.Controls", EFocusDir::Right) == "-");
}

TEST_CASE("UI.MainMenu: tiles enter 40 ms apart, at most 6 staggered (spec 9)")
{
	const FUiMotion Motion = FUiTheme::Defaults().Motion;
	const int Max = static_cast<int>(Motion.StaggerMax);
	CHECK(StaggerDelay(0, Motion.Stagger, Max) == doctest::Approx(0.0f));
	CHECK(StaggerDelay(1, Motion.Stagger, Max) == doctest::Approx(0.04f));
	CHECK(StaggerDelay(5, Motion.Stagger, Max) == doctest::Approx(0.20f));
	// The seventh and later enter with the sixth.
	CHECK(StaggerDelay(6, Motion.Stagger, Max) == doctest::Approx(0.20f));
	CHECK(StaggerDelay(9, Motion.Stagger, Max) == doctest::Approx(0.20f));
	CHECK(StaggerDelay(3, Motion.Stagger, 1) == doctest::Approx(0.0f));
}

TEST_CASE("UI.Practice: four control rows with a pad and a keyboard key each (spec 7.9)")
{
	const std::vector<FPracticeControlRow>& Rows = PracticeControlRows();
	REQUIRE(Rows.size() == 4);
	CHECK(Rows[0].Control == EPracticeControl::ChangeDrill);
	CHECK(Rows[0].PadKeys.size() == 2);
	CHECK(Rows[3].Control == EPracticeControl::Pause);
	for (const FPracticeControlRow& Row : Rows)
	{
		CHECK_FALSE(Row.PadKeys.empty());
		CHECK(Row.PadKeys.size() == Row.Keys.size());
		for (const char* Key : Row.PadKeys)
		{
			// Every pad key has a label on every family (GDD 7).
			CHECK_FALSE(GamepadKeyLabel(Key, EPadFamily::Xbox).empty());
			CHECK_FALSE(GamepadKeyLabel(Key, EPadFamily::PlayStation).empty());
			CHECK_FALSE(GamepadKeyLabel(Key, EPadFamily::Switch).empty());
		}
	}
}

TEST_CASE("UI.Practice: the pause list is Resume, Controls, Settings, Quit practice and wraps (spec 7.9)")
{
	const std::vector<std::string>& Items = PracticePauseItems();
	REQUIRE(Items.size() == 4);
	CHECK(Items.front() == "PracticePause.Resume");
	CHECK(Items.back() == "PracticePause.Quit");
	const FFocusMap Map = FFocusMap::VerticalList(Items);
	CHECK(Map.Validate().empty());
	CHECK(Map.FirstFocus() == "PracticePause.Resume");
	CHECK(Map.Move("PracticePause.Quit", EFocusDir::Down).value_or("") == "PracticePause.Resume");
}
