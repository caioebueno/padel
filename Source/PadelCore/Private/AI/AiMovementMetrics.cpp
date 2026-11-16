#include "AI/AiMovementMetrics.h"

#include "PadelTypes.h"
#include "VecMath.h"

#include <algorithm>
#include <cmath>

namespace Padel::AI
{
	namespace
	{
		// Nearest-rank percentile (P in 0..1) of a copy, so the order samples arrived in
		// doesn't matter and the result is always a value that was measured.
		double Percentile(std::vector<double> Values, double P)
		{
			if (Values.empty())
			{
				return 0.0;
			}
			std::sort(Values.begin(), Values.end());
			const double Rank = std::ceil(P * static_cast<double>(Values.size()));
			const size_t Index = static_cast<size_t>(std::clamp(Rank, 1.0, static_cast<double>(Values.size()))) - 1;
			return Values[Index];
		}

		double Median(const std::vector<double>& Values) { return Percentile(Values, 0.5); }

		double GroundSpeed(const FAiMovementSample& S) { return SizeXY(S.Velocity); }
	}

	FAiMovementMetrics::FAiMovementMetrics(const FAiMovementMetricsConfig& InConfig)
		: Config(InConfig)
		, AccelWindowTicks(std::max(1, static_cast<int>(std::lround(InConfig.AccelSmoothingS / InConfig.TickSeconds))))
		, AccelMinTicks(std::max(1, static_cast<int>(std::lround(InConfig.AccelMinDurationS / InConfig.TickSeconds))))
	{
	}

	void FAiMovementMetrics::BeginMatch()
	{
		State = {};
	}

	void FAiMovementMetrics::ResetHistory(FPlayerState& S) const
	{
		S.History.assign(static_cast<size_t>(AccelWindowTicks) + 1, FVec3{});
		S.Filled = 0;
		S.bEdgeStopInWindow = false;
		S.TicksSinceEdgeStop = 0;
		S.AboveTicks = 0;
		S.bEventCounted = false;
	}

	void FAiMovementMetrics::TrackAcceleration(FPlayerState& S, const FAiMovementSample& Now)
	{
		const int Size = AccelWindowTicks + 1;
		S.History[static_cast<size_t>(S.Filled % Size)] = Now.Velocity;
		++S.Filled;
		if (Now.bEdgeStop)
		{
			++EdgeStops;
			S.bEdgeStopInWindow = true;
			S.TicksSinceEdgeStop = 0;
		}
		else if (S.bEdgeStopInWindow && ++S.TicksSinceEdgeStop >= AccelWindowTicks)
		{
			S.bEdgeStopInWindow = false;
		}
		if (S.Filled < Size || S.bEdgeStopInWindow)
		{
			// No full window yet, or the clamp's dead stop is inside it: unknown, so
			// it neither extends nor ends a stretch above the threshold.
			S.AboveTicks = 0;
			return;
		}

		// The trailing average over N ticks moves by (v_t - v_t-N) / N each tick, so its
		// rate of change is that difference over the window's length.
		const FVec3& Oldest = S.History[static_cast<size_t>(S.Filled % Size)];
		const double Accel = SizeXY(Now.Velocity - Oldest) / (AccelWindowTicks * Config.TickSeconds);
		S.MatchPeakAccel = std::max(S.MatchPeakAccel, Accel);
		if (Accel > Config.AccelEventMps2)
		{
			++S.AboveTicks;
			if (S.AboveTicks >= AccelMinTicks && !S.bEventCounted)
			{
				++AccelEvents;
				S.bEventCounted = true;
			}
		}
		else
		{
			S.AboveTicks = 0;
			S.bEventCounted = false;
		}
	}

	void FAiMovementMetrics::Tick(bool bBallInPlay, const std::array<FAiMovementSample, NumTracked>& Players, int HitterId)
	{
		if (!bBallInPlay)
		{
			// Between points players are placed for the serve; that isn't movement.
			for (FPlayerState& S : State)
			{
				S.bHasPrevious = false;
				S.bReacting = false;
			}
			return;
		}

		++PlayTicks;
		const double Dt = Config.TickSeconds;
		for (int Id = 0; Id < NumTracked; ++Id)
		{
			FPlayerState& S = State[Id];
			const FAiMovementSample& Now = Players[Id];
			const double Speed = GroundSpeed(Now);
			S.bMatchHadPlay = true;
			SpeedSum += Speed;
			SlowTicks += Speed < Config.SlowSpeedMps ? 1 : 0;
			FastTicks += Speed > Config.FastSpeedMps ? 1 : 0;
			S.MatchPeakSpeed = std::max(S.MatchPeakSpeed, Speed);

			if (!S.bHasPrevious)
			{
				ResetHistory(S);
			}
			else
			{
				S.PointDistance += SizeXY(Now.Position - S.Previous.Position);
			}
			TrackAcceleration(S, Now);

			if (S.bReacting)
			{
				++S.TicksSinceContact;
				if (Speed > S.ContactSpeed + Config.ReactionSpeedRiseMps)
				{
					ReactionSeconds.push_back(S.TicksSinceContact * Dt);
					S.bReacting = false;
				}
			}

			S.Previous = Now;
			S.LastSpeed = Speed;
			S.bHasPrevious = true;
		}

		if (IsValidPlayer(HitterId))
		{
			const ETeam HitterTeam = TeamOfPlayer(HitterId);
			for (int Id = 0; Id < NumTracked; ++Id)
			{
				FPlayerState& S = State[Id];
				// A new contact supersedes any reaction still pending (the ball came back
				// before that player moved); only the hitter's opponents start a new one.
				S.bReacting = TeamOfPlayer(Id) != HitterTeam;
				S.ContactSpeed = S.LastSpeed;
				S.TicksSinceContact = 0;
			}
		}
	}

	void FAiMovementMetrics::EndPoint(int ServerId)
	{
		++Points;
		for (FPlayerState& S : State)
		{
			PointDistances.push_back(S.PointDistance);
		}
		if (IsValidPlayer(ServerId))
		{
			const double Server = State[ServerId].PointDistance;
			const double Partner = State[PartnerOf(ServerId)].PointDistance;
			++ServerPoints;
			ServerDistance += Server;
			ServerPartnerDistance += Partner;
			ServerFarther += Server > Partner ? 1 : 0;
		}
		for (FPlayerState& S : State)
		{
			S.PointDistance = 0.0;
			S.bReacting = false;
		}
	}

	void FAiMovementMetrics::EndMatch()
	{
		bool bHadPlay = false;
		for (const FPlayerState& S : State)
		{
			bHadPlay = bHadPlay || S.bMatchHadPlay;
		}
		Matches += bHadPlay ? 1 : 0;
		for (FPlayerState& S : State)
		{
			if (S.bMatchHadPlay)
			{
				PeakSpeeds.push_back(S.MatchPeakSpeed);
				PeakAccels.push_back(S.MatchPeakAccel);
			}
		}
		State = {};
	}

	void FAiMovementMetrics::Merge(const FAiMovementMetrics& Other)
	{
		const auto Append = [](std::vector<double>& To, const std::vector<double>& From) { To.insert(To.end(), From.begin(), From.end()); };
		Append(PointDistances, Other.PointDistances);
		Append(PeakSpeeds, Other.PeakSpeeds);
		Append(PeakAccels, Other.PeakAccels);
		Append(ReactionSeconds, Other.ReactionSeconds);
		PlayTicks += Other.PlayTicks;
		SpeedSum += Other.SpeedSum;
		SlowTicks += Other.SlowTicks;
		FastTicks += Other.FastTicks;
		AccelEvents += Other.AccelEvents;
		EdgeStops += Other.EdgeStops;
		Matches += Other.Matches;
		Points += Other.Points;
		ServerPoints += Other.ServerPoints;
		ServerFarther += Other.ServerFarther;
		ServerDistance += Other.ServerDistance;
		ServerPartnerDistance += Other.ServerPartnerDistance;
	}

	FAiMovementSummary FAiMovementMetrics::Summarize() const
	{
		FAiMovementSummary R;
		R.Points = Points;
		R.PlayerMatches = static_cast<int>(PeakSpeeds.size());
		R.BallInPlaySeconds = static_cast<double>(PlayTicks) * Config.TickSeconds;
		R.DistancePerPointMedian = Median(PointDistances);
		R.DistancePerPointP95 = Percentile(PointDistances, 0.95);
		const double PlayerTicks = static_cast<double>(PlayTicks) * NumTracked;
		if (PlayerTicks > 0.0)
		{
			R.AverageSpeed = SpeedSum / PlayerTicks;
			R.SlowShare = static_cast<double>(SlowTicks) / PlayerTicks;
			R.FastShare = static_cast<double>(FastTicks) / PlayerTicks;
			// Per player: each player's events over the minutes they were on court.
			const double PlayerMinutes = PlayerTicks * Config.TickSeconds / 60.0;
			R.AccelEventsPerMinute = static_cast<double>(AccelEvents) / PlayerMinutes;
		}
		R.PeakSpeedMedian = Median(PeakSpeeds);
		R.PeakSpeedMax = PeakSpeeds.empty() ? 0.0 : *std::max_element(PeakSpeeds.begin(), PeakSpeeds.end());
		R.PeakAccelMedian = Median(PeakAccels);
		R.PeakAccelMax = PeakAccels.empty() ? 0.0 : *std::max_element(PeakAccels.begin(), PeakAccels.end());
		R.EdgeStopsPerMatch = Matches > 0 ? static_cast<double>(EdgeStops) / Matches : 0.0;
		R.ReactionSamples = static_cast<int>(ReactionSeconds.size());
		R.ReactionMedianMs = 1000.0 * Median(ReactionSeconds);
		if (ServerPoints > 0)
		{
			R.ServerDistancePerPoint = ServerDistance / ServerPoints;
			R.ServerPartnerDistancePerPoint = ServerPartnerDistance / ServerPoints;
			R.ServerFartherShare = static_cast<double>(ServerFarther) / ServerPoints;
		}
		return R;
	}
}
