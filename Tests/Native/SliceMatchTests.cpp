// The four slice players (GDD 6.4) as the game loads them from Data/Players.json, and a
// match with them played to the end through the same pieces the game mode drives:
// FMatchSimulation, FAiMatchRunner and the shot system (APadelMatchDirector, Agent 2).
#include "doctest.h"

#include "AI/AiMatchRunner.h"
#include "ShotTestData.h"
#include "Support/AiTestData.h"
#include "TestSupport.h"

#include <array>
#include <string>

namespace
{
	// APadelMatchDirector's default rows, by player id.
	const std::array<std::string, Padel::NumPlayers> SliceRows = { "selva_arunde", "mikko_ventara", "ilan_corvisa", "ruben_halvaro" };

	double MeanRating(const Padel::Shots::FPlayerRatings& Player)
	{
		using Padel::Shots::ERating;
		double Sum = 0.0;
		for (int Index = 0; Index < static_cast<int>(ERating::Count); ++Index)
		{
			Sum += Player.Get(static_cast<ERating>(Index));
		}
		return Sum / static_cast<double>(ERating::Count);
	}
}

TEST_CASE("Match.SliceRoster: four players, around 75 to 82, on the sides GDD 6.4 gives them")
{
	using Padel::Shots::ECourtSide;
	for (int Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		CAPTURE(SliceRows[static_cast<std::size_t>(Id)]);
		const Padel::Shots::FPlayerRatings Player = ShotTestData::LoadPlayer(SliceRows[static_cast<std::size_t>(Id)]);
		const double Mean = MeanRating(Player);
		CHECK(Mean >= 74.0);
		CHECK(Mean <= 83.0);
		// Players 0 and 2 start on the right (FMatchScore's default), 1 and 3 on the left.
		CHECK(Player.PreferredSide == (Id % 2 == 0 ? ECourtSide::Right : ECourtSide::Left));
	}
}

TEST_CASE("Match.SliceRoster: AI vs AI with the slice players plays a set to the end")
{
	using namespace Padel;
	using namespace Padel::AI;

	struct FEndListener : IMatchEventListener
	{
		int Points = 0;
		bool bOver = false;
		ETeam Winner = ETeam::A;
		void OnPointEnded(ETeam, EPointReason, int) override { ++Points; }
		void OnMatchWon(ETeam Team) override { bOver = true; Winner = Team; }
	};

	const Shots::FShotLibrary Library = ShotTestData::LoadLibrary();
	const Shots::FMovementTuning Movement = ShotTestData::LoadMovement();
	FAiMatchSetup Setup;
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		Setup.Players[static_cast<std::size_t>(Id)] = ShotTestData::LoadPlayer(SliceRows[static_cast<std::size_t>(Id)]);
	}
	const FAiDifficulty& Medium = TestData::Difficulty("Medium");
	Setup.Difficulties = { &Medium, &Medium };
	Setup.Seed = 7;

	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), Setup.Seed);
	FEndListener Listener;
	Match.AddListener(&Listener);
	FAiMatchRunner Runner(Match, Library, Movement, TestData::Tactics(), Setup);
	Runner.Start();

	// A set is a few hundred points at most; two simulated hours is far past any real one.
	const long long MaxTicks = static_cast<long long>(SimTickRate) * 60 * 60 * 2;
	for (long long Tick = 0; Tick < MaxTicks && !Listener.bOver; ++Tick)
	{
		Runner.Tick();
	}
	REQUIRE(Listener.bOver);
	const FScoreSnapshot Score = Match.Score().Snapshot();
	CHECK(Score.bMatchOver);
	CHECK(Score.Winner == Listener.Winner);
	// GDD 3.1: at least 6 games, so at least 24 points.
	CHECK(Listener.Points >= 24);
	Match.RemoveListener(&Listener);
}
