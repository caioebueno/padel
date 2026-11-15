// A scripted stand-in for the match simulation, so the menus, HUD, camera and stats
// screen can be played from the main menu to the stats screen before the referee and
// players exist. It is NOT the rules engine (that's PadelCore, Agent 2): it keeps a
// simple score so the UI sees realistic event sequences, and is deleted once
// UPadelMatchEvents is wired to the real simulation.
//
// Deterministic for a seed (its own small generator, the same on every platform).
//
// UI model: plain C++20, no Unreal headers.

#pragma once

#include "UI/Model/PadelUiEventSink.h"
#include "UI/Model/PadelUiScore.h"

#include <cstdint>

namespace Padel::Ui
{
	struct FDemoMatchOptions
	{
		// 2 = best of 3 (GDD 13 decision 5 default), 1 = one set.
		int SetsToWin = 2;
		bool bGoldenPoint = true;
		std::uint32_t Seed = 1;
		// Seconds between points as the UI plays them.
		double SecondsPerPoint = 2.5;
	};

	class FDemoMatch
	{
	public:
		FDemoMatch(const FDemoMatchOptions& InOptions, IUiEventSink& InSink);

		// Sends the opening score. Call once before Step.
		void Start();

		// Plays the next point when its time comes. Returns false once the match is over.
		bool Step(double DeltaSeconds);

		// Plays one whole point now: serve, a rally, the ending and the score events.
		void PlayPoint();

		// GDD 3.1: the receiving pair picks who receives the golden point. The demo only
		// records it so the prompt can be tested end to end.
		void ChooseGoldenPointReceiver(int PlayerId) { GoldenReceiver = PlayerId; }

		bool IsOver() const { return bOver; }
		const FScoreView& GetScore() const { return Score; }
		int GetGoldenReceiver() const { return GoldenReceiver; }

	private:
		std::uint32_t Next();
		int Pick(int Count);
		void AwardPoint(int Team);
		void AwardGame(int Team);
		void Publish();
		int ServerFor(int GameIndex) const;

		FDemoMatchOptions Options;
		IUiEventSink& Sink;
		FScoreView Score;
		std::uint32_t RngState;
		std::array<int, 2> PointCount{0, 0};
		int GamesPlayedInSet = 0;
		int GamesPlayedInMatch = 0;
		std::array<int, 2> SetsWon{0, 0};
		double Clock = 0.0;
		int GoldenReceiver = -1;
		bool bOver = false;
	};
}
