// The post-match stats screen and the Pause screen without the engine
// (docs/ui/MENU_SPEC.md 7.5, 7.8; W8): the new stats-model rows, tie-break points per
// set, the comparison bars and highlights, the entry timeline, the stray-press lock and
// the Pause score line.
#include "doctest.h"

#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiDemoMatch.h"
#include "UI/Model/PadelUiMatchStats.h"
#include "UI/Model/PadelUiPauseView.h"
#include "UI/Model/PadelUiScore.h"
#include "UI/Model/PadelUiStatsView.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace Padel::Ui;

namespace
{
	// A score at a given set state, as score_changed would carry it.
	FScoreView StatsTestScore(std::vector<std::array<int, 2>> Sets, std::array<int, 2> Games, bool bTiebreak)
	{
		FScoreView View;
		View.Sets = std::move(Sets);
		View.Games = Games;
		View.bInTiebreak = bTiebreak;
		return View;
	}

	// Feeds a demo match's events to FMatchStats, as APadelHUD does.
	struct FStatsTestSink final : IUiEventSink
	{
		FMatchStats Stats;
		FScoreView LastScore;

		void OnServeStarted(int) override { Stats.OnServeStarted(); }
		void OnServeFault(std::string_view) override {}
		void OnServeLet() override {}
		void OnBallHit(int Player, std::string_view Shot, std::string_view Grade, double) override { Stats.OnBallHit(Player, Shot, Grade); }
		void OnBallBounced(std::string_view Surface, int, const FUiVec3&) override { Stats.OnBallBounced(Surface); }
		void OnBallLeftCourt(const FUiVec3&) override {}
		void OnPointEnded(int Team, std::string_view Reason, int) override { Stats.OnPointEnded(Team, Reason); }
		void OnScoreChanged(const FScoreView& Score) override
		{
			LastScore = Score;
			Stats.OnScoreChanged(Score);
			if (Score.bMatchOver)
			{
				Stats.OnMatchWon(Score.Winner, Score.Sets);
			}
		}
		void OnGameWon(int) override {}
		void OnSetWon(int) override {}
		void OnMatchWon(int) override {}
		void OnChangeOfEnds() override {}
		void OnGoldenPointStarted(int) override {}
	};
}

TEST_CASE("UI.Stats: points won off the glass go to the winning team that played off the glass (spec 7.8, W8)")
{
	FMatchStats Stats;
	// Team B plays off the glass, but Team A wins the point: no glass point for anyone.
	Stats.OnServeStarted();
	Stats.OnBallHit(0, "SERVE_FLAT", "GOOD");
	Stats.OnBallBounced("glass");
	Stats.OnBallHit(2, "LOB", "GOOD");
	Stats.OnBallHit(1, "SMASH", "PERFECT");
	Stats.OnPointEnded(TeamA, "OUT_OF_COURT_WINNER");

	// Team A plays off the glass and wins.
	Stats.OnServeStarted();
	Stats.OnBallHit(0, "SERVE_FLAT", "GOOD");
	Stats.OnBallHit(3, "DRIVE", "GOOD");
	Stats.OnBallBounced("glass");
	Stats.OnBallHit(1, "BANDEJA", "GOOD");
	Stats.OnPointEnded(TeamA, "DOUBLE_BOUNCE");

	// Both teams play off the glass; Team B wins.
	Stats.OnServeStarted();
	Stats.OnBallHit(2, "SERVE_KICK", "GOOD");
	Stats.OnBallBounced("glass");
	Stats.OnBallHit(0, "LOB", "GOOD");
	Stats.OnBallBounced("glass");
	Stats.OnBallHit(3, "VIBORA", "GOOD");
	Stats.OnPointEnded(TeamB, "DOUBLE_BOUNCE");

	CHECK(Stats.GlassPointsWon == FTeamCounts{1, 1});
	CHECK(Stats.OffGlassShots == FTeamCounts{2, 2});
	CHECK(Stats.PointsWithGlass == 3);
	CHECK(Stats.Smashes() == FTeamCounts{1, 0});
}

TEST_CASE("UI.Stats: the summary tab is spec 7.8's six rows, with errors and double faults lower-is-better")
{
	FMatchStats Stats;
	const std::vector<FStatRow> Rows = Stats.SummaryRows();
	REQUIRE(Rows.size() == 6);
	const std::vector<std::string> Keys{"PointsWon", "Winners", "Errors", "Smashes", "GlassPoints", "DoubleFaults"};
	for (size_t Index = 0; Index < Rows.size(); ++Index)
	{
		CAPTURE(Index);
		CHECK(Rows[Index].Key == Keys[Index]);
		CHECK(Rows[Index].bLowerIsBetter == (Keys[Index] == "Errors" || Keys[Index] == "DoubleFaults"));
	}
	CHECK(Rows[4].Label == "Points won off the glass");
}

TEST_CASE("UI.Stats: the shots tab lists played shots in GDD 4 order, both serves as one, then shots off the glass")
{
	FMatchStats Stats;
	Stats.OnBallHit(0, "SERVE_FLAT", "GOOD");
	Stats.OnBallHit(2, "SERVE_KICK", "GOOD");
	Stats.OnBallHit(3, "SMASH", "GOOD");
	Stats.OnBallHit(1, "DRIVE", "GOOD");
	Stats.OnBallHit(1, "VIBORA", "GOOD");
	Stats.OnBallHit(0, "MISHIT", "MISS");

	const std::vector<FStatRow> Rows = StatsTabRows(Stats, EStatsTab::Shots);
	std::vector<std::string> Keys;
	for (const FStatRow& Row : Rows)
	{
		Keys.push_back(Row.Key);
	}
	CHECK(Keys == std::vector<std::string>{"DRIVE", "VIBORA", "SMASH", "SERVE", "MISHIT", "GlassShots"});
	CHECK(Rows[1].Label == "V\xC3\xAD" "bora");
	CHECK(Rows[3].Label == "Serve");
	CHECK(Rows[3].TeamA == 1);
	CHECK(Rows[3].TeamB == 1);
	// No lob was played: no lob row (spec 7.8 "one row per shot type that was played").
	CHECK(std::find(Keys.begin(), Keys.end(), "LOB") == Keys.end());
}

TEST_CASE("UI.Stats: tie-break points are kept per set, nothing for a set without one (W8)")
{
	FMatchStats Stats;
	// Set 1 ends 6-3 without a tie-break.
	Stats.OnScoreChanged(StatsTestScore({}, {5, 3}, false));
	Stats.OnPointEnded(TeamA, "DOUBLE_BOUNCE");
	Stats.OnScoreChanged(StatsTestScore({{6, 3}}, {0, 0}, false));
	// Set 2 reaches 6-6 and Team B wins the tie-break 7-5.
	Stats.OnScoreChanged(StatsTestScore({{6, 3}}, {6, 6}, true));
	for (int Point = 0; Point < 5; ++Point)
	{
		Stats.OnPointEnded(TeamA, "DOUBLE_BOUNCE");
		Stats.OnScoreChanged(StatsTestScore({{6, 3}}, {6, 6}, true));
	}
	for (int Point = 0; Point < 7; ++Point)
	{
		Stats.OnPointEnded(TeamB, "INTO_NET");
		const bool bSetOver = Point == 6;
		Stats.OnScoreChanged(bSetOver ? StatsTestScore({{6, 3}, {6, 7}}, {0, 0}, false) : StatsTestScore({{6, 3}}, {6, 6}, true));
	}
	Stats.OnMatchWon(TeamB, {{6, 3}, {6, 7}});

	REQUIRE(Stats.SetTiebreaks.size() == 2);
	CHECK_FALSE(Stats.TiebreakOfSet(0).has_value());
	REQUIRE(Stats.TiebreakOfSet(1).has_value());
	CHECK(*Stats.TiebreakOfSet(1) == FTeamCounts{5, 7});
	CHECK_FALSE(Stats.TiebreakOfSet(2).has_value());

	const FSetCell A = SetCell(Stats, 1, TeamA);
	const FSetCell B = SetCell(Stats, 1, TeamB);
	CHECK(A.Games == 6);
	CHECK_FALSE(A.bWonSet);
	CHECK(A.TiebreakPoints == 5);
	CHECK(B.Games == 7);
	CHECK(B.bWonSet);
	CHECK(B.TiebreakPoints == 7);
	CHECK_FALSE(SetCell(Stats, 0, TeamA).TiebreakPoints.has_value());
	CHECK(SetCell(Stats, 0, TeamA).bWonSet);
}

TEST_CASE("UI.Stats: across demo matches, exactly the 7-6 sets carry a tie-break, won by two from 7")
{
	int TiebreakSets = 0;
	for (std::uint32_t Seed = 1; Seed <= 60; ++Seed)
	{
		FStatsTestSink Sink;
		FDemoMatchOptions Options;
		Options.Seed = Seed;
		FDemoMatch Match(Options, Sink);
		Match.Start();
		int Guard = 0;
		while (!Match.IsOver() && Guard++ < 2000)
		{
			Match.PlayPoint();
		}
		REQUIRE(Match.IsOver());
		CAPTURE(Seed);
		REQUIRE(Sink.Stats.SetTiebreaks.size() == Sink.Stats.FinalSets.size());
		for (int Set = 0; Set < SetCount(Sink.Stats); ++Set)
		{
			const std::array<int, 2>& Games = Sink.Stats.FinalSets[static_cast<size_t>(Set)];
			const bool bSevenSix = (Games[0] == 7 && Games[1] == 6) || (Games[0] == 6 && Games[1] == 7);
			const std::optional<FTeamCounts> Tiebreak = Sink.Stats.TiebreakOfSet(Set);
			CHECK(Tiebreak.has_value() == bSevenSix);
			if (Tiebreak)
			{
				++TiebreakSets;
				const int Winner = Games[0] > Games[1] ? 0 : 1;
				CHECK((*Tiebreak)[static_cast<size_t>(Winner)] >= 7);
				CHECK((*Tiebreak)[static_cast<size_t>(Winner)] - (*Tiebreak)[static_cast<size_t>(1 - Winner)] >= 2);
			}
		}
	}
	// The seeds above must reach at least one tie-break, or this test checks nothing.
	CHECK(TiebreakSets > 0);
}

TEST_CASE("UI.Stats: the higher number leads, except where lower is better; ties lead neither")
{
	CHECK(LeadingTeam({"Points won", 68, 57, "PointsWon"}) == TeamA);
	CHECK(LeadingTeam({"Errors", 14, 19, "Errors", true}) == TeamA);
	CHECK(LeadingTeam({"Double faults", 4, 2, "DoubleFaults", true}) == TeamB);
	CHECK(LeadingTeam({"Smashes", 3, 3, "Smashes"}) == -1);
}

TEST_CASE("UI.Stats: bars scale to the larger number, and both are empty at 0-0 (spec 7.8, 7.12)")
{
	const FStatRow Row{"Points won", 68, 57, "PointsWon"};
	CHECK(BarFraction(Row, TeamA) == doctest::Approx(1.0));
	CHECK(BarFraction(Row, TeamB) == doctest::Approx(57.0 / 68.0));
	const FStatRow Empty{"Points won off the glass", 0, 0, "GlassPoints"};
	CHECK(BarFraction(Empty, TeamA) == doctest::Approx(0.0));
	CHECK(BarFraction(Empty, TeamB) == doctest::Approx(0.0));
}

TEST_CASE("UI.Stats: the footer numbers are one decimal and a whole percent")
{
	FMatchStats Stats;
	CHECK(RallyText(Stats) == "0.0");
	CHECK(GlassPercent(Stats) == 0);
	Stats.RallyLengths = {8, 9, 8};
	Stats.PointsWon = {2, 1};
	Stats.PointsWithGlass = 1;
	CHECK(RallyText(Stats) == "8.3");
	CHECK(GlassPercent(Stats) == 33);
}

TEST_CASE("UI.Stats: the entry animation runs columns, then staggered bars (spec 7.8, 9)")
{
	FStatsTimeline Timeline;
	Timeline.SetColumns = 3;
	Timeline.Rows = 8;
	CHECK(Timeline.ColumnAlpha(0, 0.0f) == doctest::Approx(0.0f));
	CHECK(Timeline.ColumnAlpha(0, 0.12f) == doctest::Approx(1.0f));
	CHECK(Timeline.ColumnAlpha(1, 0.12f) == doctest::Approx(0.0f));
	CHECK(Timeline.ColumnAlpha(2, 0.36f) == doctest::Approx(1.0f));
	// Bars wait for the columns.
	CHECK(Timeline.BarProgress(0, 0.36f) == doctest::Approx(0.0f));
	CHECK(Timeline.BarProgress(0, 0.36f + 0.4f) == doctest::Approx(1.0f));
	CHECK(Timeline.BarProgress(1, 0.36f + 0.2f) == doctest::Approx((0.2f - 0.04f) / 0.4f));
	// Only six rows stagger; rows 6 and 7 start with row 5.
	CHECK(Timeline.BarProgress(7, 0.5f) == doctest::Approx(Timeline.BarProgress(5, 0.5f)));
	CHECK(Timeline.Duration() == doctest::Approx(0.36f + 5 * 0.04f + 0.4f));
	CHECK(Timeline.BarProgress(7, Timeline.Duration()) == doctest::Approx(1.0f));

	FStatsTimeline Nothing;
	CHECK(Nothing.Duration() == doctest::Approx(0.0f));
}

TEST_CASE("UI.Stats: A during the animation only finishes it; A in the lock does nothing; a fresh A after acts")
{
	FStatsConfirmGate Gate;
	Gate.Begin(1.0f, 0.6f);
	CHECK(Gate.IsLocked());
	CHECK(Gate.IsAnimating());
	Gate.Tick(0.2f);
	CHECK(Gate.Press() == EStatsConfirm::FinishAnimation);
	CHECK_FALSE(Gate.IsAnimating());
	CHECK(Gate.AnimationTime() == doctest::Approx(0.6f));
	CHECK(Gate.IsLocked());
	CHECK(Gate.Press() == EStatsConfirm::Ignored);
	Gate.Tick(0.79f);
	CHECK(Gate.IsLocked());
	CHECK(Gate.Press() == EStatsConfirm::Ignored);
	Gate.Tick(0.02f);
	CHECK_FALSE(Gate.IsLocked());
	CHECK(Gate.Press() == EStatsConfirm::Accept);
}

TEST_CASE("UI.Stats: an animation longer than the lock keeps the buttons locked until it ends")
{
	FStatsConfirmGate Gate;
	Gate.Begin(1.0f, 1.2f);
	Gate.Tick(1.1f);
	CHECK(Gate.IsLocked());
	Gate.Tick(0.11f);
	CHECK_FALSE(Gate.IsLocked());

	// Reduce motion: no animation, the lock alone (spec 7.8).
	FStatsConfirmGate Still;
	Still.Begin(1.0f, 0.0f);
	CHECK_FALSE(Still.IsAnimating());
	CHECK(Still.Press() == EStatsConfirm::Ignored);
	Still.Tick(1.0f);
	CHECK(Still.Press() == EStatsConfirm::Accept);
}

TEST_CASE("UI.Pause: the header score line is sets, games and points with en dashes (spec 7.5)")
{
	FScoreView Score;
	Score.Sets = {{6, 4}};
	Score.Games = {3, 2};
	Score.Points = {"30", "15"};
	CHECK(PauseScoreParts(Score) == std::vector<std::string>{"6\xE2\x80\x93" "4", "3\xE2\x80\x93" "2", "30\xE2\x80\x93" "15"});

	FScoreView Advantage;
	Advantage.Points = {"AD", ""};
	CHECK(PauseScoreParts(Advantage).back() == "AD\xE2\x80\x93" "40");

	FScoreView Fresh;
	CHECK(PauseScoreParts(Fresh) == std::vector<std::string>{"0\xE2\x80\x93" "0", "0\xE2\x80\x93" "0"});
}

TEST_CASE("UI.Pause: the context panel follows the focused item (spec 7.5)")
{
	CHECK(PauseContextFor("Pause.Resume") == EPauseContext::Rules);
	CHECK(PauseContextFor("Pause.Restart") == EPauseContext::Rules);
	CHECK(PauseContextFor("Pause.Controls") == EPauseContext::Controls);
	CHECK(PauseContextFor("Pause.Settings") == EPauseContext::Settings);
	CHECK(PauseContextFor("Pause.Quit") == EPauseContext::Quit);
}
