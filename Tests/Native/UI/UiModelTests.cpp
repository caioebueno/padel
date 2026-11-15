// The engine-free UI model: labels, the score bug's text, stats, roster and button names.
#include "doctest.h"

#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiMatchStats.h"
#include "UI/Model/PadelUiRoster.h"
#include "UI/Model/PadelUiScore.h"

#include <set>
#include <string>

using namespace Padel::Ui;

TEST_CASE("UI.Codes: every GDD 3.3 reason has a readable label")
{
	for (const char* Code : {"DOUBLE_BOUNCE", "INTO_NET", "WALL_BEFORE_BOUNCE", "OUT_DIRECT", "OWN_SIDE",
		"BODY_HIT", "DOUBLE_HIT", "NET_TOUCH", "CROSSED", "RACKET_THROWN", "DOUBLE_FAULT", "VOLLEYED_SERVE",
		"OUT_OF_COURT_WINNER", "CAME_BACK_OVER", "STUCK_IN_FENCE"})
	{
		const std::string Label = ReasonLabel(Code);
		CAPTURE(Code);
		CHECK_FALSE(Label.empty());
		CHECK(Label.find('_') == std::string::npos);
	}
	CHECK(ReasonLabel("WALL_BEFORE_BOUNCE") == "Glass on the fly");
	CHECK(ReasonLabel("SOMETHING_NEW") == "Something new");
}

TEST_CASE("UI.Codes: winners belong to the hitter, everything else is an error")
{
	CHECK(IsWinnerReason("DOUBLE_BOUNCE"));
	CHECK(IsWinnerReason("OUT_OF_COURT_WINNER"));
	CHECK(IsWinnerReason("CAME_BACK_OVER"));
	CHECK_FALSE(IsWinnerReason("INTO_NET"));
	CHECK_FALSE(IsWinnerReason("OUT_DIRECT"));
	CHECK_FALSE(IsWinnerReason("DOUBLE_FAULT"));
	// Codes compare without case, since ShotTypeName() returns lower-case ids.
	CHECK(IsWinnerReason("double_bounce"));
}

TEST_CASE("UI.Codes: shot, timing and fault labels")
{
	CHECK(ShotLabel("bandeja") == "Bandeja");
	CHECK(ShotLabel("VIBORA") == "V\xC3\xAD" "bora");
	CHECK(ShotLabel("SERVE_KICK") == "Kick serve");
	CHECK(TimingLabel("PERFECT") == "Perfect");
	CHECK(TimingLabel("late") == "Late");
	CHECK(ServeFaultLabel("MESH_AFTER_BOUNCE") == "Fault: mesh after bounce");
	CHECK(ServeFaultLabel("") == "Fault");
}

TEST_CASE("UI.Codes: players 0-1 are Team A, 2-3 Team B")
{
	CHECK(TeamOfPlayer(0) == TeamA);
	CHECK(TeamOfPlayer(1) == TeamA);
	CHECK(TeamOfPlayer(2) == TeamB);
	CHECK(TeamOfPlayer(3) == TeamB);
	CHECK(TeamOfPlayer(4) == -1);
	CHECK(TeamOfPlayer(-1) == -1);
	CHECK(OtherTeam(TeamA) == TeamB);
	CHECK(OtherTeam(TeamB) == TeamA);
}

TEST_CASE("UI.Score: set columns list finished sets then the live one")
{
	FScoreView View;
	View.Sets = {{6, 4}, {3, 6}};
	View.Games = {2, 1};
	View.Points = {"30", "15"};
	CHECK(SetColumns(View, 0) == std::vector<int>{6, 3, 2});
	CHECK(SetColumns(View, 1) == std::vector<int>{4, 6, 1});
	CHECK(SummaryLine(View) == "6-4 3-6 2-1 30-15");

	View.bMatchOver = true;
	View.Sets.push_back({6, 2});
	CHECK(SetColumns(View, 0) == std::vector<int>{6, 3, 6});
	CHECK(SummaryLine(View) == "6-4 3-6 6-2");
	CHECK(FinalLine(View.Sets) == "6-4 3-6 6-2");
}

TEST_CASE("UI.Score: golden point shows at 40-40 only with golden point on (GDD 3.1)")
{
	FScoreView View;
	View.Points = {"40", "40"};
	CHECK(IsGoldenPointNow(View));
	CHECK(PressureTag(View) == GoldenPointTag);

	View.bGoldenPoint = false;
	CHECK_FALSE(IsGoldenPointNow(View));
	CHECK(PressureTag(View).empty());

	View.bGoldenPoint = true;
	View.bInTiebreak = true;
	View.Points = {"5", "5"};
	CHECK_FALSE(IsGoldenPointNow(View));
	CHECK(PressureTag(View) == TiebreakTag);
}

TEST_CASE("UI.Score: the biggest point at stake wins the tag")
{
	FScoreView View;
	View.Points = {"40", "40"};
	View.bBreakPoint = true;
	CHECK(PressureTag(View) == "BREAK POINT");
	View.bSetPoint = true;
	CHECK(PressureTag(View) == "SET POINT");
	View.bMatchPoint = true;
	CHECK(PressureTag(View) == "MATCH POINT");
	View.bMatchOver = true;
	CHECK(PressureTag(View).empty());
}

TEST_CASE("UI.Score: point labels from counts")
{
	CHECK(PointLabelFromCount(0) == "0");
	CHECK(PointLabelFromCount(1) == "15");
	CHECK(PointLabelFromCount(2) == "30");
	CHECK(PointLabelFromCount(3) == "40");
	CHECK(PointLabelFromCount(9) == "40");
}

TEST_CASE("UI.Stats: winners go to the point winner, errors to the loser")
{
	FMatchStats Stats;
	Stats.OnServeStarted();
	Stats.OnBallHit(0, "SERVE_FLAT", "GOOD");
	Stats.OnBallHit(2, "drive", "EARLY");
	Stats.OnPointEnded(TeamB, "DOUBLE_BOUNCE");

	Stats.OnServeStarted();
	Stats.OnBallHit(0, "SERVE_FLAT", "GOOD");
	Stats.OnPointEnded(TeamB, "INTO_NET");

	Stats.OnServeStarted();
	Stats.OnPointEnded(TeamA, "DOUBLE_FAULT");

	CHECK(Stats.PointsWon == FTeamCounts{1, 2});
	CHECK(Stats.Winners == FTeamCounts{0, 1});
	CHECK(Stats.Errors == FTeamCounts{1, 1});
	CHECK(Stats.DoubleFaults == FTeamCounts{0, 1});
	CHECK(Stats.TotalPoints() == 3);
	CHECK(Stats.AverageRally() == doctest::Approx(1.0));
	CHECK(Stats.Shots["DRIVE"] == FTeamCounts{0, 1});
	CHECK(Stats.TimingGrades["GOOD"] == 2);
}

TEST_CASE("UI.Stats: a shot after the glass counts as off the glass (GDD 5.3, GDD 12 item 4)")
{
	FMatchStats Stats;
	Stats.OnServeStarted();
	Stats.OnBallHit(0, "SERVE_FLAT", "GOOD");
	Stats.OnBallBounced("ground");
	Stats.OnBallBounced("glass");
	Stats.OnBallHit(2, "LOB", "GOOD");
	// A ground bounce after the hit doesn't make the next shot an off-glass one.
	Stats.OnBallBounced("ground");
	Stats.OnBallHit(1, "BANDEJA", "PERFECT");
	Stats.OnPointEnded(TeamA, "DOUBLE_BOUNCE");

	Stats.OnServeStarted();
	Stats.OnBallHit(1, "SERVE_FLAT", "GOOD");
	Stats.OnPointEnded(TeamB, "OUT_DIRECT");

	CHECK(Stats.OffGlassShots == FTeamCounts{0, 1});
	CHECK(Stats.PointsWithGlass == 1);
	CHECK(Stats.GlassPointShare() == doctest::Approx(0.5));
	CHECK(Stats.RallyLengths == std::vector<int>{3, 1});
}

TEST_CASE("UI.Stats: ending rows are labelled and most common first")
{
	FMatchStats Stats;
	Stats.OnPointEnded(TeamA, "INTO_NET");
	Stats.OnPointEnded(TeamB, "DOUBLE_BOUNCE");
	Stats.OnPointEnded(TeamA, "DOUBLE_BOUNCE");
	Stats.OnPointEnded(TeamA, "DOUBLE_BOUNCE");
	Stats.OnPointEnded(-1, "DOUBLE_BOUNCE");  // ignored: no winner

	const std::vector<FStatRow> Rows = Stats.EndingRows();
	REQUIRE(Rows.size() == 2);
	CHECK(Rows[0].Label == "Double bounce");
	CHECK(Rows[0].TeamA == 2);
	CHECK(Rows[0].TeamB == 1);
	CHECK(Rows[1].Label == "Into the net");
	CHECK(Stats.SummaryRows().size() == 5);
	CHECK(Stats.TotalPoints() == 4);
}

TEST_CASE("UI.Roster: four invented players in two pairs (GDD 6.4)")
{
	std::set<std::string> Names;
	for (int Id = 0; Id < 4; ++Id)
	{
		const FRosterPlayer* Player = FindPlayer(Id);
		REQUIRE(Player != nullptr);
		Names.insert(DisplayName(Id));
	}
	CHECK(Names.size() == 4);
	CHECK(FindPlayer(4) == nullptr);
	CHECK(PairLabel(0) == "ARUNDE / VENTARA");
	CHECK(PairLabel(1) == "CORVISA / HALVARO");
	CHECK(Surname(1) == "Ventara");
	CHECK(DisplayName(9) == "Player 9");
	// GDD 6.4 profiles: each pair has a right-side and a left-side player.
	CHECK(std::string(FindPlayer(0)->Side) == "Right");
	CHECK(std::string(FindPlayer(1)->Side) == "Left");
	CHECK(std::string(FindPlayer(2)->Side) == "Right");
	CHECK(std::string(FindPlayer(3)->Side) == "Left");
}

TEST_CASE("UI.Glyphs: the bottom face button is A, Cross or B by pad (GDD 7)")
{
	CHECK(GamepadKeyLabel("Gamepad_FaceButton_Bottom", EPadFamily::Xbox) == "A");
	CHECK(GamepadKeyLabel("Gamepad_FaceButton_Bottom", EPadFamily::PlayStation) == "Cross");
	CHECK(GamepadKeyLabel("Gamepad_FaceButton_Bottom", EPadFamily::Switch) == "B");
	CHECK(GamepadKeyLabel("Gamepad_FaceButton_Top", EPadFamily::Switch) == "X");
	CHECK(GamepadKeyLabel("Gamepad_RightShoulder", EPadFamily::PlayStation) == "R1");
	CHECK(GamepadKeyLabel("Gamepad_DPad_Up", EPadFamily::Xbox) == "D-pad up");
	CHECK(GamepadKeyLabel("J", EPadFamily::Xbox).empty());

	CHECK(FamilyFromDeviceName("") == EPadFamily::KeyboardMouse);
	CHECK(FamilyFromDeviceName("KBM") == EPadFamily::KeyboardMouse);
	CHECK(FamilyFromDeviceName("KeyboardAndMouse") == EPadFamily::KeyboardMouse);
	CHECK(FamilyFromDeviceName("XInputController") == EPadFamily::Xbox);
	CHECK(FamilyFromDeviceName("DualSense") == EPadFamily::PlayStation);
	CHECK(FamilyFromDeviceName("PS4Controller") == EPadFamily::PlayStation);
	CHECK(FamilyFromDeviceName("SwitchProController") == EPadFamily::Switch);
}
