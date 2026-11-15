// GDD 12 item 2: AI-vs-AI soak, fixed seeds, zero stuck points, zero invariant failures.
// Runs on the test-only stand-in match (Support/StandInMatch.h) until the engine lands.
//
// CI runs a short soak on every PR. The full 100 matches:
//   Tools/RunNativeTests.sh -tc="Ai.Soak*" -- or set PADEL_SOAK_MATCHES=100
#include "doctest.h"
#include "Support/AiTestData.h"
#include "Support/StandInMatch.h"

#include <cstdio>
#include <cstdlib>

using namespace Padel::AI;

namespace
{
	int SoakMatches(int Default)
	{
		const char* Env = std::getenv("PADEL_SOAK_MATCHES");
		return Env != nullptr ? std::max(1, std::atoi(Env)) : Default;
	}

	Test::FSoakStats Soak(const char* Level, int Matches, uint64_t FirstSeed, Test::FMatchOptions Options = {})
	{
		Test::FSoakStats Total;
		const FAiDifficulty& D = TestData::Difficulty(Level);
		for (int Match = 0; Match < Matches; ++Match)
		{
			Test::FStandInMatch Game(TestData::Tactics(), D, D, FirstSeed + static_cast<uint64_t>(Match), Options);
			Total.Add(Game.Play());
		}
		return Total;
	}

	void Report(const char* Label, const Test::FSoakStats& S)
	{
		std::printf("[soak] %s: %d matches, %d points, %.2f shots/point, %.0f%% points with an off-the-glass shot, "
					"%d stuck, %d invariant failures, %d whiffs\n",
			Label, S.Matches, S.Points, S.ShotsPerPoint(), 100.0 * S.GlassShare(), S.StuckPoints, S.InvariantFailures, S.Whiffs);
		std::printf("[soak]   endings:");
		for (const auto& [Reason, Count] : S.Reasons) std::printf(" %s=%d", Reason.c_str(), Count);
		std::printf("\n[soak]   shots:");
		for (const auto& [Shot, Count] : S.ShotCounts) std::printf(" %s=%d", Shot.c_str(), Count);
		std::printf("\n");
	}
}

TEST_CASE("Ai.Soak: Medium AI-vs-AI matches finish with no stuck points")
{
	const Test::FSoakStats S = Soak("Medium", SoakMatches(10), 1000);
	Report("Medium", S);
	CHECK(S.StuckPoints == 0);
	CHECK(S.InvariantFailures == 0);
	CHECK(S.Points > 0);
}

TEST_CASE("Ai.Soak: every difficulty plays without getting stuck")
{
	for (const char* Level : { "Easy", "Hard" })
	{
		const Test::FSoakStats S = Soak(Level, 3, 2000);
		Report(Level, S);
		CHECK(S.StuckPoints == 0);
		CHECK(S.InvariantFailures == 0);
	}
}

TEST_CASE("Ai.Soak: a match replays identically from its seed (GDD 8 determinism)")
{
	Test::FStandInMatch A(TestData::Tactics(), TestData::Difficulty("Medium"), TestData::Difficulty("Medium"), 77);
	Test::FStandInMatch B(TestData::Tactics(), TestData::Difficulty("Medium"), TestData::Difficulty("Medium"), 77);
	const Test::FSoakStats SA = A.Play();
	const Test::FSoakStats SB = B.Play();
	CHECK(A.Winner == B.Winner);
	CHECK(SA.Points == SB.Points);
	CHECK(SA.Shots == SB.Shots);
	CHECK(SA.Reasons == SB.Reasons);
}
