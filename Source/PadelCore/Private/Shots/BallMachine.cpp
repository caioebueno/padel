#include "Shots/BallMachine.h"

#include <algorithm>

namespace Padel::Shots
{
	FBallMachine::FBallMachine(std::vector<FBallMachineFeed> InFeeds, double InPlayerTeamSign, std::uint64_t InSeed)
		: Feeds(std::move(InFeeds)), PlayerTeamSign(InPlayerTeamSign), Rng(InSeed)
	{
		for (const FBallMachineFeed& Feed : Feeds)
		{
			if (std::find(Drills.begin(), Drills.end(), Feed.Drill) == Drills.end())
			{
				Drills.push_back(Feed.Drill);
			}
		}
		Drills.push_back(MixedDrill);
		Drills.push_back(ServeDrill);
	}

	void FBallMachine::NextDrill(int Direction)
	{
		const int Count = static_cast<int>(Drills.size());
		DrillIndex = static_cast<std::size_t>(((static_cast<int>(DrillIndex) + Direction) % Count + Count) % Count);
		WaitS = 0.0;
	}

	bool FBallMachine::Tick(double Dt, FPracticeBall& Ball)
	{
		if (bPaused || IsServeDrill() || Ball.IsLive())
		{
			WaitS = 0.0;
			return false;
		}
		// A dead ball lingers a moment so the player sees where it ended.
		if (LastFeed && !Ball.IsDone())
		{
			return false;
		}
		WaitS += Dt;
		if (WaitS < (LastFeed ? LastFeed->IntervalS : 0.0))
		{
			return false;
		}
		FeedNow(Ball);
		return true;
	}

	void FBallMachine::FeedNow(FPracticeBall& Ball)
	{
		std::vector<const FBallMachineFeed*> Pool;
		for (const FBallMachineFeed& Feed : Feeds)
		{
			if (GetDrill() == MixedDrill || Feed.Drill == GetDrill())
			{
				Pool.push_back(&Feed);
			}
		}
		if (Pool.empty())
		{
			return;
		}
		const std::size_t Pick = static_cast<std::size_t>(Rng.NextDouble() * static_cast<double>(Pool.size()));
		const FBallMachineFeed& Feed = *Pool[Pick < Pool.size() ? Pick : Pool.size() - 1];
		// The data is written for a player on +X; mirror for Team B.
		const double S = PlayerTeamSign;
		const FVec3 From = Vec(Feed.OriginX * S, Feed.OriginY * S, Feed.OriginZ);
		const FVec3 Target = Vec(Feed.TargetX * S + Rng.Range(-Feed.TargetJitterM, Feed.TargetJitterM),
			Feed.TargetY * S + Rng.Range(-Feed.TargetJitterM, Feed.TargetJitterM), 0.0);
		Ball.Feed(From, Target, Feed.SpeedKmh / 3.6, Feed.ApexM, static_cast<int>(-S));
		LastFeed = &Feed;
		WaitS = 0.0;
	}
}
