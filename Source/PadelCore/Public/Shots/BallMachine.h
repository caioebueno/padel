// The practice ball machine (GDD 11 "practice mode with a ball machine", brought forward
// so the shot system can be played on purpose): feeds balls from the far half in drills
// (groundstrokes, volleys, overheads, off the glass, serves, mixed), seeded.

#pragma once

#include "Shots/ShotMath.h"

#include "Shots/BallMachineFeed.h"
#include "Shots/PracticeBall.h"
#include "Shots/ShotRng.h"

#include <string>
#include <vector>

namespace Padel::Shots
{
	class PADELCORE_API FBallMachine
	{
	public:
		FBallMachine(std::vector<FBallMachineFeed> InFeeds, double InPlayerTeamSign, std::uint64_t InSeed);

		// Drill names in data order, plus "mixed" (every feed) and "serve" (no feeds:
		// the player tosses and serves).
		const std::vector<std::string>& GetDrills() const { return Drills; }
		const std::string& GetDrill() const { return Drills[DrillIndex]; }
		void NextDrill(int Direction);
		bool IsServeDrill() const { return GetDrill() == ServeDrill; }
		void SetPaused(bool bInPaused) { bPaused = bInPaused; }
		bool IsPaused() const { return bPaused; }

		// Feeds the next ball once the last one is done. Returns true on a feed.
		bool Tick(double Dt, FPracticeBall& Ball);
		// Feed one ball of the current drill now.
		void FeedNow(FPracticeBall& Ball);

		const FBallMachineFeed* GetLastFeed() const { return LastFeed; }

		static inline const std::string MixedDrill = "mixed";
		static inline const std::string ServeDrill = "serve";

	private:
		std::vector<FBallMachineFeed> Feeds;
		std::vector<std::string> Drills;
		double PlayerTeamSign = 1.0;
		FShotRng Rng;
		std::size_t DrillIndex = 0;
		double WaitS = 0.0;
		bool bPaused = false;
		const FBallMachineFeed* LastFeed = nullptr;
	};
}
