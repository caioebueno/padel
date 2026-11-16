// The menu foundation's engine-free parts (docs/ui/MENU_SPEC.md 4, 5, 6, 9, 11.5): theme
// tokens and Data/UiTheme.json, key mapping, focus maps, repeat, stick dead zone, input
// locks, control value rules, hint order and the screen flow.
#include "doctest.h"

#include "../JsonLite.h"
#include "UI/Model/PadelUiControls.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/Model/PadelUiScreenFlow.h"
#include "UI/Model/PadelUiTheme.h"

#include <cmath>
#include <set>
#include <string>

using namespace Padel::Ui;

namespace
{
	std::string UiThemePath()
	{
		return std::string(PADEL_REPO_ROOT) + "/Data/UiTheme.json";
	}

	bool SameColour(const FUiColour& A, const FUiColour& B)
	{
		return std::fabs(A.R - B.R) < 1e-4f && std::fabs(A.G - B.G) < 1e-4f && std::fabs(A.B - B.B) < 1e-4f
			&& std::fabs(A.A - B.A) < 1e-4f;
	}
}

TEST_CASE("UI.Theme: hex colours parse, with or without #")
{
	FUiColour Colour;
	REQUIRE(ParseHexColour("#C8F03C", Colour));
	CHECK(Colour.R == doctest::Approx(200.0 / 255.0));
	CHECK(Colour.G == doctest::Approx(240.0 / 255.0));
	CHECK(Colour.B == doctest::Approx(60.0 / 255.0));
	CHECK(ParseHexColour("070b14", Colour));
	CHECK_FALSE(ParseHexColour("#C8F03", Colour));
	CHECK_FALSE(ParseHexColour("#C8F0ZZ", Colour));
}

TEST_CASE("UI.Theme: defaults are the spec's tokens")
{
	const FUiTheme Theme = FUiTheme::Defaults();
	FUiColour Lime;
	ParseHexColour("#C8F03C", Lime);
	CHECK(SameColour(Theme.Colour(EUiColour::Accent), Lime));
	CHECK(Theme.Colour(EUiColour::GlassFill).A == doctest::Approx(0.72));
	CHECK(Theme.Colour(EUiColour::BgScrimPause).A == doctest::Approx(0.70));
	CHECK(Theme.Type(EUiType::Hero).Size == 120.0f);
	CHECK(Theme.Type(EUiType::Hero).Face == EUiFontFace::CondensedExtraBoldItalic);
	// Spec 4.2: numbers use tabular figures wherever scores or stats line up.
	CHECK(Theme.Type(EUiType::Score).Face == EUiFontFace::CondensedBoldTabular);
	CHECK(Theme.Type(EUiType::Stat).Face == EUiFontFace::CondensedBoldTabular);
	CHECK(Theme.Type(EUiType::Overall).Face == EUiFontFace::CondensedExtraBoldTabular);
	CHECK(Theme.PlayerCardStats.size() == 4);
}

TEST_CASE("UI.Theme: no text is smaller than 26 px (spec 1.5)")
{
	const FUiTheme Theme = FUiTheme::Defaults();
	for (int Index = 0; Index < static_cast<int>(EUiType::Count); ++Index)
	{
		CAPTURE(TypeKey(static_cast<EUiType>(Index)));
		CHECK(Theme.FontSize(static_cast<EUiType>(Index), ETextSize::Default) >= 26.0f);
	}
}

TEST_CASE("UI.Theme: Text size scales Body, BodySmall and Label only (spec 4.2)")
{
	const FUiTheme Theme = FUiTheme::Defaults();
	CHECK(Theme.FontSize(EUiType::Body, ETextSize::Default) == 28.0f);
	CHECK(Theme.FontSize(EUiType::Body, ETextSize::Large) == 32.0f);
	CHECK(Theme.FontSize(EUiType::Body, ETextSize::Largest) == 36.0f);
	CHECK(Theme.FontSize(EUiType::BodySmall, ETextSize::Largest) == 34.0f);
	CHECK(Theme.FontSize(EUiType::Label, ETextSize::Largest) == 34.0f);
	CHECK(Theme.FontSize(EUiType::Display, ETextSize::Largest) == 72.0f);
	CHECK(Theme.FontSize(EUiType::Heading, ETextSize::Large) == 32.0f);
}

TEST_CASE("UI.Theme: the 12-column grid fills the 1728 px safe width (spec 4.3)")
{
	const FUiTheme Theme = FUiTheme::Defaults();
	CHECK(Theme.SpanWidth(12) == 1728.0f);
	CHECK(Theme.SpanWidth(8) == 1144.0f);
	CHECK(Theme.SpanWidth(6) == 852.0f);
	CHECK(Theme.SpanWidth(4) == 560.0f);
	CHECK(Theme.SpanWidth(3) == 414.0f);
	CHECK(Theme.ColumnX(1) == 96.0f);
	CHECK(Theme.ColumnX(9) == 96.0f + 8 * 146.0f);
	CHECK(Theme.ColumnX(12) + Theme.SpanWidth(1) == 1920.0f - 96.0f);
}

TEST_CASE("UI.Theme: unknown keys are refused so the loader can warn")
{
	FUiTheme Theme = FUiTheme::Defaults();
	CHECK_FALSE(Theme.SetColour("Accent.Lime", "#FFFFFF", 1.0f));
	CHECK_FALSE(Theme.SetColour("Accent", "#XYZ", 1.0f));
	CHECK_FALSE(Theme.SetNumber("Layout", "Safe.Z", 1.0f));
	CHECK_FALSE(Theme.SetType("Huge", FUiTypeStyle{}));
	CHECK(Theme.SetNumber("Motion", "Focus", 0.2f));
	CHECK(Theme.Motion.Focus == doctest::Approx(0.2));
}

TEST_CASE("UI.Theme: Data/UiTheme.json names every token and matches the built-in defaults")
{
	// The file is the source of truth (CLAUDE.md); the defaults are the fallback when it's
	// missing (spec 7.12). They must agree, or a missing file would change the look.
	const JsonLite::FValue Root = JsonLite::ParseFile(UiThemePath());
	const FUiTheme Defaults = FUiTheme::Defaults();
	FUiTheme Loaded = FUiTheme::Defaults();

	std::set<std::string> ColourKeysSeen;
	for (const auto& [Key, Value] : Root["Colour"].Object)
	{
		CAPTURE(Key);
		CHECK(Loaded.SetColour(Key, Value["Hex"].String, static_cast<float>(Value["Alpha"].Number)));
		ColourKeysSeen.insert(Key);
	}
	for (int Index = 0; Index < static_cast<int>(EUiColour::Count); ++Index)
	{
		const EUiColour Token = static_cast<EUiColour>(Index);
		CAPTURE(ColourKey(Token));
		CHECK(ColourKeysSeen.count(ColourKey(Token)) == 1);
		CHECK(SameColour(Loaded.Colour(Token), Defaults.Colour(Token)));
	}

	std::set<std::string> TypesSeen;
	for (const auto& [Key, Value] : Root["Type"].Object)
	{
		CAPTURE(Key);
		FUiTypeStyle Style;
		REQUIRE(FontFaceFromKey(Value["Face"].String, Style.Face));
		Style.Size = static_cast<float>(Value["Size"].Number);
		Style.LineHeight = static_cast<float>(Value["LineHeight"].Number);
		Style.TrackingPercent = static_cast<float>(Value["Tracking"].Number);
		Style.bUpper = Value["Upper"].Bool;
		Style.bScales = Value["Scales"].Bool;
		CHECK(Loaded.SetType(Key, Style));
		TypesSeen.insert(Key);
	}
	for (int Index = 0; Index < static_cast<int>(EUiType::Count); ++Index)
	{
		const EUiType Token = static_cast<EUiType>(Index);
		CAPTURE(TypeKey(Token));
		CHECK(TypesSeen.count(TypeKey(Token)) == 1);
		const FUiTypeStyle& A = Loaded.Type(Token);
		const FUiTypeStyle& B = Defaults.Type(Token);
		CHECK(A.Face == B.Face);
		CHECK(A.Size == B.Size);
		CHECK(A.LineHeight == B.LineHeight);
		CHECK(A.TrackingPercent == B.TrackingPercent);
		CHECK(A.bUpper == B.bUpper);
		CHECK(A.bScales == B.bScales);
	}

	std::set<std::string> NumbersSeen;
	for (const char* Group : {"Layout", "Motion", "Input"})
	{
		for (const auto& [Key, Value] : Root[Group].Object)
		{
			const std::string Full = std::string(Group) + "." + Key;
			CAPTURE(Full);
			float Default = 0.0f;
			REQUIRE(GetNumber(Defaults, Group, Key, Default));
			CHECK(static_cast<float>(Value.Number) == doctest::Approx(Default));
			NumbersSeen.insert(Full);
		}
	}
	for (const std::string& Key : NumberKeys())
	{
		CAPTURE(Key);
		CHECK(NumbersSeen.count(Key) == 1);
	}

	for (const char* Key : {"Default", "Large", "Largest"})
	{
		CHECK(Loaded.SetTextScale(Key, static_cast<float>(Root["TextScale"][Key].Number)));
	}
	CHECK(Loaded.TextScale == Defaults.TextScale);

	const JsonLite::FValue& Card = Root["PlayerCardStats"];
	REQUIRE(Card.Object.size() == Defaults.PlayerCardStats.size());
	for (const auto& [Label, Attributes] : Defaults.PlayerCardStats)
	{
		CAPTURE(Label);
		REQUIRE(Card.Has(Label));
		REQUIRE(Card[Label].Array.size() == Attributes.size());
		for (size_t Index = 0; Index < Attributes.size(); ++Index)
		{
			CHECK(Card[Label].Array[Index].String == Attributes[Index]);
		}
	}
}

TEST_CASE("UI.Theme: every card stat names a real Data/Players.json attribute (spec 6.6)")
{
	const JsonLite::FValue Players = JsonLite::ParseFile(std::string(PADEL_REPO_ROOT) + "/Data/Players.json");
	REQUIRE_FALSE(Players.Array.empty());
	for (const auto& [Label, Attributes] : FUiTheme::Defaults().PlayerCardStats)
	{
		for (const std::string& Attribute : Attributes)
		{
			CAPTURE(Attribute);
			CHECK(Players.Array[0].Has(Attribute));
		}
	}
}

TEST_CASE("UI.Input: keys map to menu inputs per spec 5.1")
{
	CHECK(InputForKey("Gamepad_FaceButton_Bottom") == EUiInput::Confirm);
	CHECK(InputForKey("Enter") == EUiInput::Confirm);
	CHECK(InputForKey("SpaceBar") == EUiInput::Confirm);
	CHECK(InputForKey("J") == EUiInput::Confirm);
	CHECK(InputForKey("Gamepad_FaceButton_Right") == EUiInput::Back);
	CHECK(InputForKey("Escape") == EUiInput::Back);
	CHECK(InputForKey("BackSpace") == EUiInput::Back);
	CHECK(InputForKey("K") == EUiInput::Back);
	CHECK(InputForKey("W") == EUiInput::Up);
	CHECK(InputForKey("A") == EUiInput::Left);
	CHECK(InputForKey("Gamepad_DPad_Down") == EUiInput::Down);
	CHECK(InputForKey("Gamepad_LeftShoulder") == EUiInput::TabLeft);
	CHECK(InputForKey("E") == EUiInput::TabRight);
	CHECK(InputForKey("Gamepad_FaceButton_Left") == EUiInput::Secondary);
	CHECK(InputForKey("R") == EUiInput::Secondary);
	CHECK(InputForKey("Gamepad_FaceButton_Top") == EUiInput::Tertiary);
	// G13: Start is its own input, never Back.
	CHECK(InputForKey("Gamepad_Special_Right") == EUiInput::Pause);
	// The View button does nothing in menus; the stick goes through FStickDirection.
	CHECK(InputForKey("Gamepad_Special_Left") == EUiInput::None);
	CHECK(InputForKey("Gamepad_LeftStick_Up") == EUiInput::None);
}

TEST_CASE("UI.Input: hint glyphs name the pad button by position and a key cap")
{
	CHECK(std::string(PadKeyName(EUiInput::Confirm)) == "Gamepad_FaceButton_Bottom");
	CHECK(std::string(PadKeyName(EUiInput::Back)) == "Gamepad_FaceButton_Right");
	CHECK(std::string(PadKeyName(EUiInput::TabLeft)) == "Gamepad_LeftShoulder");
	CHECK(std::string(KeyboardHintLabel(EUiInput::Confirm)) == "Enter");
	CHECK(std::string(KeyboardHintLabel(EUiInput::Back)) == "Esc");
	// Every input a hint can show has a glyph on both devices.
	for (EUiInput Input : {EUiInput::Confirm, EUiInput::Back, EUiInput::Secondary, EUiInput::Tertiary,
		EUiInput::TabLeft, EUiInput::TabRight, EUiInput::Pause})
	{
		CHECK(std::string(PadKeyName(Input)).size() > 0);
		CHECK(std::string(KeyboardHintLabel(Input)).size() > 0);
	}
}

TEST_CASE("UI.Theme: easings start at 0 and end at 1")
{
	for (float (*Ease)(float) : {&EaseOutCubic, &EaseOutQuad, &EaseOutQuint, &EaseInQuad})
	{
		CHECK(Ease(0.0f) == doctest::Approx(0.0));
		CHECK(Ease(1.0f) == doctest::Approx(1.0));
		CHECK(Ease(2.0f) == doctest::Approx(1.0));
		CHECK(Ease(-1.0f) == doctest::Approx(0.0));
	}
}

TEST_CASE("UI.Focus: a vertical list wraps and a row doesn't (spec 5.3)")
{
	const FFocusMap List = FFocusMap::VerticalList({"Resume", "Restart", "Quit"});
	CHECK(List.Move("Resume", EFocusDir::Down) == std::optional<std::string>("Restart"));
	CHECK(List.Move("Quit", EFocusDir::Down) == std::optional<std::string>("Resume"));
	CHECK(List.Move("Resume", EFocusDir::Up) == std::optional<std::string>("Quit"));
	CHECK_FALSE(List.Move("Resume", EFocusDir::Left).has_value());
	CHECK(List.Validate().empty());

	const FFocusMap Row = FFocusMap::Row({"Rematch", "MainMenu"});
	CHECK(Row.Move("Rematch", EFocusDir::Right) == std::optional<std::string>("MainMenu"));
	CHECK_FALSE(Row.Move("MainMenu", EFocusDir::Right).has_value());
	CHECK(Row.Validate().empty());
}

TEST_CASE("UI.Focus: disabled elements are skipped, locked ones are not (spec 5.3)")
{
	FFocusMap Map = FFocusMap::VerticalList({"A", "B", "C"});
	Map.SetEnabled("B", false);
	CHECK(Map.Move("A", EFocusDir::Down) == std::optional<std::string>("C"));
	CHECK(Map.Move("C", EFocusDir::Up) == std::optional<std::string>("A"));
	Map.SetDefault("B");
	CHECK(Map.FirstFocus() == "A");

	// Every element disabled: no infinite loop, no move.
	Map.SetEnabled("A", false);
	Map.SetEnabled("C", false);
	CHECK_FALSE(Map.Move("A", EFocusDir::Down).has_value());
}

TEST_CASE("UI.Focus: the main menu map from spec 7.2 is sound")
{
	FFocusMap Map;
	Map.Add("Main.QuickMatch", {"", "Main.Career", "", "Main.Practice"});
	Map.Add("Main.Practice", {"", "Main.Settings", "Main.QuickMatch", ""});
	Map.Add("Main.Settings", {"Main.Practice", "Main.Controls", "Main.QuickMatch", ""});
	Map.Add("Main.Career", {"Main.QuickMatch", "", "", "Main.TeamBuilder"});
	Map.Add("Main.TeamBuilder", {"Main.QuickMatch", "", "Main.Career", "Main.Controls"});
	Map.Add("Main.Controls", {"Main.Settings", "", "Main.TeamBuilder", ""});
	Map.SetDefault("Main.QuickMatch");
	CHECK(Map.Validate().empty());
	// Grids don't wrap: at an edge nothing happens.
	CHECK_FALSE(Map.Move("Main.QuickMatch", EFocusDir::Up).has_value());
	CHECK_FALSE(Map.Move("Main.Practice", EFocusDir::Right).has_value());
}

TEST_CASE("UI.Focus: validation finds dangling links and unreachable elements")
{
	FFocusMap Map;
	Map.Add("One", {"", "Two", "", ""});
	Map.Add("Two", {"One", "Ghost", "", ""});
	Map.Add("Island", {"", "", "", ""});
	Map.SetDefault("One");
	const std::vector<std::string> Problems = Map.Validate();
	CHECK(Problems.size() == 2);
}

TEST_CASE("UI.Input: a held direction repeats after 400 ms, then every 110 ms (spec 5.3)")
{
	FDirectionRepeat Repeat;
	Repeat.Configure(0.4f, 0.11f);
	Repeat.Press(EUiInput::Down);
	CHECK(Repeat.Tick(0.39f) == 0);
	CHECK(Repeat.Tick(0.02f) == 1);
	CHECK(Repeat.Tick(0.10f) == 1);
	CHECK(Repeat.Tick(0.05f) == 0);
	CHECK(Repeat.Tick(0.30f) == 3);
	Repeat.Release(EUiInput::Up);
	CHECK(Repeat.Held() == EUiInput::Down);
	Repeat.Release(EUiInput::Down);
	CHECK(Repeat.Tick(1.0f) == 0);
}

TEST_CASE("UI.Input: the stick presses past the 0.5 dead zone, once (spec 5.2)")
{
	FStickDirection Stick;
	Stick.SetDeadZone(0.5f);
	CHECK(Stick.Update(0.3f, 0.0f) == EUiInput::None);
	CHECK(Stick.Update(0.6f, 0.1f) == EUiInput::Right);
	CHECK(Stick.Update(0.9f, 0.1f) == EUiInput::None);
	// Still above 80% of the dead zone: held, no chatter.
	CHECK(Stick.Update(0.45f, 0.0f) == EUiInput::None);
	CHECK(Stick.Current() == EUiInput::Right);
	CHECK(Stick.Update(0.1f, 0.0f) == EUiInput::None);
	CHECK(Stick.Released() == EUiInput::Right);
	CHECK(Stick.Update(0.2f, -0.7f) == EUiInput::Down);
}

TEST_CASE("UI.Input: the Stats lock refuses Confirm for 1 s, other input after 120 ms (spec 7.8)")
{
	FInputLock Lock;
	Lock.Begin(0.12f, 1.0f);
	CHECK_FALSE(Lock.AcceptsInput());
	Lock.Tick(0.13f);
	CHECK(Lock.AcceptsInput());
	CHECK_FALSE(Lock.AcceptConfirmPress());
	Lock.Tick(0.9f);
	CHECK(Lock.AcceptConfirmPress());

	FInputLock Entry;
	Entry.Begin(0.12f);
	Entry.Tick(0.12f);
	CHECK(Entry.AcceptsInput());
	CHECK(Entry.AcceptConfirmPress());
}

TEST_CASE("UI.Controls: the selector doesn't wrap on left/right but A wraps (spec 6.8)")
{
	FSelectorModel Difficulty{3, 1};
	CHECK(Difficulty.Step(+1));
	CHECK(Difficulty.AtEnd());
	CHECK_FALSE(Difficulty.Step(+1));
	CHECK(Difficulty.Index == 2);
	Difficulty.StepForwardWrapping();
	CHECK(Difficulty.Index == 0);
	CHECK(Difficulty.AtStart());
	CHECK_FALSE(Difficulty.Step(-1));
	CHECK(Difficulty.ShowsDots());
	CHECK_FALSE((FSelectorModel{6, 0}).ShowsDots());
}

TEST_CASE("UI.Controls: toggle left is off, right is on, edges report (spec 6.7)")
{
	FToggleModel Toggle{true};
	CHECK_FALSE(Toggle.Push(+1));
	CHECK(Toggle.Push(-1));
	CHECK_FALSE(Toggle.bOn);
	Toggle.Toggle();
	CHECK(Toggle.bOn);
}

TEST_CASE("UI.Controls: segmented and tabs select directly with no wrap (spec 6.9, 6.11)")
{
	FSegmentedModel Format{2, 1};
	CHECK_FALSE(Format.Step(+1));
	CHECK(Format.Step(-1));
	CHECK(Format.Index == 0);
	FTabModel Tabs{3, 0};
	CHECK_FALSE(Tabs.Step(-1));
	CHECK(Tabs.Step(+1));
	CHECK(Tabs.Step(+1));
	CHECK_FALSE(Tabs.Step(+1));
}

TEST_CASE("UI.Controls: slider steps by 5, repeats by 1, then by 5 after 1 s (spec 6.10, 5.3)")
{
	FSliderModel Volume;
	Volume.Value = 80;
	CHECK(Volume.Press(+1));
	CHECK(Volume.Value == 85);
	CHECK(Volume.Repeat(+1, 0.5f));
	CHECK(Volume.Value == 86);
	CHECK(Volume.Repeat(+1, 1.2f));
	CHECK(Volume.Value == 91);
	Volume.Set(100);
	CHECK_FALSE(Volume.Press(+1));
	CHECK(Volume.Fraction() == doctest::Approx(1.0));
	Volume.Set(-5);
	CHECK(Volume.Value == 0);
}

TEST_CASE("UI.Hints: A leads, B sits at the right edge, LB RB is one pair (spec 6.1)")
{
	const std::vector<FHint> Ordered = OrderHints({
		{EUiInput::Back, "Back"},
		{EUiInput::TabRight, "Change tab"},
		{EUiInput::Secondary, "Reset"},
		{EUiInput::Confirm, "Change"},
		{EUiInput::TabLeft, "Change tab"},
	});
	REQUIRE(Ordered.size() == 4);
	CHECK(Ordered[0].Input == EUiInput::Confirm);
	CHECK(Ordered[1].Input == EUiInput::Secondary);
	CHECK(Ordered[2].Input == EUiInput::TabLeft);
	CHECK(Ordered[3].Input == EUiInput::Back);
}

TEST_CASE("UI.Flow: Back on every screen follows the spec 3 table")
{
	CHECK(BackAction(EUiScreen::Title) == EUiBackAction::None);
	CHECK(BackAction(EUiScreen::MainMenu) == EUiBackAction::QuitGameDialog);
	CHECK(BackAction(EUiScreen::MatchSetup) == EUiBackAction::MainMenu);
	CHECK(BackAction(EUiScreen::Versus) == EUiBackAction::None);
	CHECK(BackAction(EUiScreen::Paused) == EUiBackAction::Resume);
	CHECK(BackAction(EUiScreen::Controls) == EUiBackAction::Opener);
	CHECK(BackAction(EUiScreen::Settings) == EUiBackAction::Opener);
	// Stats ignores B so a stray press can't throw the stats away.
	CHECK(BackAction(EUiScreen::Stats) == EUiBackAction::None);
	CHECK(BackAction(EUiScreen::PracticePaused) == EUiBackAction::Resume);
	for (int Index = 0; Index < static_cast<int>(EUiScreen::Count); ++Index)
	{
		CHECK(std::string(ScreenName(static_cast<EUiScreen>(Index))).size() > 0);
	}
}

TEST_CASE("UI.Flow: Settings and Controls return to whichever screen opened them")
{
	FScreenFlow Flow;
	Flow.GoTo(EUiScreen::MainMenu, EUiNavKind::Forward);
	Flow.GoTo(EUiScreen::Settings, EUiNavKind::Forward, "Main.Settings");
	CHECK(Flow.Opener() == EUiScreen::MainMenu);
	CHECK(Flow.BackTarget() == EUiScreen::MainMenu);
	CHECK(Flow.ReturnToOpener("Set.Vol.Master") == EUiScreen::MainMenu);
	// Spec 5.4: focus returns to the element that opened the child.
	CHECK(Flow.FocusOnArrival() == "Main.Settings");

	Flow.GoTo(EUiScreen::Match, EUiNavKind::Forward);
	Flow.GoTo(EUiScreen::Paused, EUiNavKind::Forward);
	Flow.GoTo(EUiScreen::Controls, EUiNavKind::Forward, "Pause.Controls");
	CHECK(Flow.Opener() == EUiScreen::Paused);
	CHECK(Flow.ReturnToOpener() == EUiScreen::Paused);
	CHECK(Flow.FocusOnArrival() == "Pause.Controls");
	CHECK(Flow.BackTarget() == EUiScreen::Match);
}

TEST_CASE("UI.Flow: a fresh open uses the default focus, coming back restores it (spec 5.4)")
{
	FScreenFlow Flow;
	Flow.GoTo(EUiScreen::MainMenu, EUiNavKind::Forward);
	Flow.GoTo(EUiScreen::MatchSetup, EUiNavKind::Forward, "Main.QuickMatch");
	CHECK(Flow.FocusOnArrival().empty());
	Flow.GoTo(EUiScreen::MainMenu, EUiNavKind::Back, "Setup.Difficulty");
	CHECK(Flow.FocusOnArrival() == "Main.QuickMatch");
	// Setup always opens on Start match: forward never restores.
	Flow.GoTo(EUiScreen::MatchSetup, EUiNavKind::Forward, "Main.QuickMatch");
	CHECK(Flow.FocusOnArrival().empty());
}

TEST_CASE("UI.Flow: going deeper slides from the right, back from the left, Pause fades (spec 9)")
{
	FScreenFlow Flow;
	CHECK(Flow.GoTo(EUiScreen::MainMenu, EUiNavKind::Forward) == EUiTransition::SlideFromRight);
	CHECK(Flow.GoTo(EUiScreen::MatchSetup, EUiNavKind::Forward) == EUiTransition::SlideFromRight);
	CHECK(Flow.GoTo(EUiScreen::MainMenu, EUiNavKind::Back) == EUiTransition::SlideFromLeft);
	CHECK(Flow.GoTo(EUiScreen::Match, EUiNavKind::Forward) == EUiTransition::Fade);
	CHECK(Flow.GoTo(EUiScreen::Paused, EUiNavKind::Forward) == EUiTransition::Fade);
}

TEST_CASE("UI.Flow: Pause opens only during play, not over the prompt or end card (spec 7.5)")
{
	FScreenFlow Flow;
	Flow.GoTo(EUiScreen::Versus, EUiNavKind::Forward);
	CHECK_FALSE(Flow.CanOpenPause(false, false));
	Flow.GoTo(EUiScreen::Match, EUiNavKind::Forward);
	CHECK(Flow.CanOpenPause(false, false));
	CHECK_FALSE(Flow.CanOpenPause(true, false));
	CHECK_FALSE(Flow.CanOpenPause(false, true));
	Flow.GoTo(EUiScreen::Practice, EUiNavKind::Forward);
	CHECK(Flow.CanOpenPause(false, false));
}
