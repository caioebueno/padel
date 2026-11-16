// GDD 8 difficulty table and the AI tuning files.
#include "doctest.h"
#include "Support/AiTestData.h"

using namespace Padel::AI;
using doctest::Approx;

TEST_CASE("Ai.Tuning: Difficulty.json matches the GDD 8 table")
{
	const FAiDifficulty& Easy = TestData::Difficulty("Easy");
	const FAiDifficulty& Medium = TestData::Difficulty("Medium");
	const FAiDifficulty& Hard = TestData::Difficulty("Hard");
	CHECK(Easy.ReadDelayS == Approx(0.28));
	CHECK(Medium.ReadDelayS == Approx(0.18));
	CHECK(Hard.ReadDelayS == Approx(0.11));
	CHECK(Easy.AimScatterMult == Approx(1.4));
	CHECK(Medium.AimScatterMult == Approx(1.0));
	CHECK(Hard.AimScatterMult == Approx(0.8));
	CHECK(Easy.AttributeOffset == -10);
	CHECK(Medium.AttributeOffset == 0);
	CHECK(Hard.AttributeOffset == 5);
	// Shot choice: Easy often safe, Hard tactical.
	CHECK(Easy.SafeShotBias > Medium.SafeShotBias);
	CHECK(Medium.SafeShotBias > Hard.SafeShotBias);
	CHECK(Hard.TacticalWeight > Medium.TacticalWeight);
	CHECK(Medium.TacticalWeight > Easy.TacticalWeight);
}

TEST_CASE("Ai.Tuning: timing distributions follow the GDD 8 descriptions")
{
	const FAiDifficulty& Easy = TestData::Difficulty("Easy");
	const FAiDifficulty& Medium = TestData::Difficulty("Medium");
	const FAiDifficulty& Hard = TestData::Difficulty("Hard");
	// Easy: mostly good, often early/late.
	CHECK(Easy.TimingWeightGood > Easy.TimingWeightEarlyLate);
	CHECK(Easy.TimingWeightEarlyLate > Easy.TimingWeightPerfect);
	// Medium: mostly good, some perfect.
	CHECK(Medium.TimingWeightGood > Medium.TimingWeightPerfect);
	CHECK(Medium.TimingWeightPerfect > Medium.TimingWeightEarlyLate);
	// Hard: mostly perfect.
	CHECK(Hard.TimingWeightPerfect > Hard.TimingWeightGood);
	CHECK(Hard.TimingWeightPerfect > 0.5 * (Hard.TimingWeightPerfect + Hard.TimingWeightGood + Hard.TimingWeightEarlyLate + Hard.TimingWeightMiss));
}

TEST_CASE("Ai.Tuning: AiTactics.json loads every field and keeps GDD 8 distances")
{
	const FAiTactics& T = TestData::Tactics();
	CHECK(T.NetDepthM >= 2.0);
	CHECK(T.NetDepthM <= 3.0);
	CHECK(Padel::Court::HalfLength - T.BackDepthM >= 1.0);
	CHECK(Padel::Court::HalfLength - T.BackDepthM <= 2.0);
	CHECK(T.PartnerSpacingMinM >= 4.0);
	CHECK(T.PartnerSpacingMaxM <= 5.0);
	CHECK(T.PartnerSpacingMinM <= T.PartnerSpacingMaxM);
	// GDD 6.1 physical ranges.
	CHECK(T.SpeedAt1Mps == Approx(4.5));
	CHECK(T.SpeedAt99Mps == Approx(6.5));
	CHECK(T.AccelTimeAt1S == Approx(0.9));
	CHECK(T.AccelTimeAt99S == Approx(0.73)); // M1: matches Movement.json TimeToTopAt99S
	CHECK(T.ReachSideAt1M == Approx(0.9));
	CHECK(T.ReachSideAt99M == Approx(1.2));
	// GDD 5.1 base timing windows.
	CHECK(T.WindowPerfectS == Approx(0.04));
	CHECK(T.WindowGoodS == Approx(0.09));
	CHECK(T.WindowEarlyLateS == Approx(0.16));
}

TEST_CASE("Ai.Tuning: the loader rejects missing and unknown keys")
{
	std::vector<FAiDifficulty> Rows;
	TuningJson::FLoadResult Missing = TuningJson::ParseDifficulties(R"([{"Name":"X","ReadDelayS":0.1}])", Rows);
	CHECK_FALSE(Missing.bOk);
	CHECK(Missing.Errors.size() >= 9);

	std::string Text = TestData::ReadText("Data/Difficulty.json");
	Text.insert(Text.find("\"ReadDelayS\""), "\"Typo\": 1, ");
	TuningJson::FLoadResult Unknown = TuningJson::ParseDifficulties(Text, Rows);
	CHECK_FALSE(Unknown.bOk);

	FAiTactics T;
	CHECK_FALSE(TuningJson::ParseTactics("not json", T).bOk);
	CHECK_FALSE(TuningJson::ParseTactics("[]", T).bOk);
}

TEST_CASE("Ai.Tuning: attribute offset clamps ratings to 1..99")
{
	FAiPlayerProfile P = TestData::Player(ESide::Right);
	P.Speed = 96;
	P.Reach = 5;
	const FAiPlayerProfile Up = P.WithOffset(5);
	const FAiPlayerProfile Down = P.WithOffset(-10);
	CHECK(Up.Speed == 99);
	CHECK(Down.Reach == 1);
	CHECK(Down.Speed == 86);
}
