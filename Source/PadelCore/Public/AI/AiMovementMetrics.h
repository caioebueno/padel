// Movement realism metrics for AI-vs-AI runs (realism plan phase 0, item A1;
// docs/animation/BIOMECHANICS_MOVEMENT.md section 4). Measures how far and how fast
// the players move while the ball is in play, so later movement and AI changes can be
// compared against real padel ranges. Measurement only: nothing here feeds back into
// the match.
//
// Engine-free and deterministic: it reads positions and velocities once per fixed
// tick and keeps raw samples (point distances, per-match peaks, reaction times), so
// two runs with the same seed give the same numbers and runs can be merged.

#pragma once

#include "PadelCoreApi.h"
#include "BallState.h"
#include "CourtLayout.h"

#include <array>
#include <vector>

namespace Padel::AI
{
	// Thresholds for the event-style metrics. These define what is counted, not how
	// anyone plays, so they are measurement constants rather than tuning data.
	struct FAiMovementMetricsConfig
	{
		double TickSeconds = SimTickSeconds;
		// Speed bands for the "slow" and "fast" shares of rally time.
		double SlowSpeedMps = 2.0;
		double FastSpeedMps = 4.0;
		// One acceleration event per upward crossing of this (BIOMECHANICS 4).
		double AccelEventMps2 = 1.12;
		// First foot movement: speed rises this much over the speed at the opponent's contact.
		double ReactionSpeedRiseMps = 0.2;
	};

	// One player's state for one tick (metres, m/s). Only the ground plane (X, Y) counts.
	struct FAiMovementSample
	{
		FVec3 Position;
		FVec3 Velocity;
	};

	// The summary a report prints. Speeds in m/s, distances in m, accelerations in m/s².
	struct FAiMovementSummary
	{
		int Points = 0;
		int PlayerMatches = 0;
		double BallInPlaySeconds = 0.0;
		double DistancePerPointMedian = 0.0;
		double DistancePerPointP95 = 0.0;
		double AverageSpeed = 0.0;
		double SlowShare = 0.0;
		double FastShare = 0.0;
		double PeakSpeedMedian = 0.0;
		double PeakSpeedMax = 0.0;
		double PeakAccelMedian = 0.0;
		double AccelEventsPerMinute = 0.0;
		int ReactionSamples = 0;
		double ReactionMedianMs = 0.0;
		double ServerDistancePerPoint = 0.0;
		double ServerPartnerDistancePerPoint = 0.0;
		// Points where the server covered more ground than their partner.
		double ServerFartherShare = 0.0;
	};

	class PADELCORE_API FAiMovementMetrics
	{
	public:
		static constexpr int NumTracked = 4;

		explicit FAiMovementMetrics(const FAiMovementMetricsConfig& InConfig = {});

		// Starts a match: per-match peaks reset, and no previous tick is carried over.
		void BeginMatch();
		// One fixed tick. bBallInPlay is true from the serve's contact to the point's end;
		// HitterId is the player whose contact the match took this tick, or -1.
		void Tick(bool bBallInPlay, const std::array<FAiMovementSample, NumTracked>& Players, int HitterId);
		// The point ended; ServerId served it (-1 leaves it out of the server comparison).
		void EndPoint(int ServerId);
		// Closes the match's peaks. A match without ball-in-play time adds nothing.
		void EndMatch();

		void Merge(const FAiMovementMetrics& Other);
		FAiMovementSummary Summarize() const;

	private:
		struct FPlayerState
		{
			bool bHasPrevious = false;
			FAiMovementSample Previous;
			double LastSpeed = 0.0;
			bool bAboveAccel = false;
			double PointDistance = 0.0;
			double MatchPeakSpeed = 0.0;
			double MatchPeakAccel = 0.0;
			bool bMatchHadPlay = false;
			// Waiting for the first foot movement after an opponent's contact.
			bool bReacting = false;
			double ContactSpeed = 0.0;
			int TicksSinceContact = 0;
		};

		FAiMovementMetricsConfig Config;
		std::array<FPlayerState, NumTracked> State;

		// Raw samples, merged across matches.
		std::vector<double> PointDistances;
		std::vector<double> PeakSpeeds;
		std::vector<double> PeakAccels;
		std::vector<double> ReactionSeconds;
		long long PlayTicks = 0;      // ticks with the ball in play (all players share them)
		double SpeedSum = 0.0;        // over players and ticks
		long long SlowTicks = 0;      // player-ticks
		long long FastTicks = 0;
		long long AccelEvents = 0;
		int Points = 0;
		int ServerPoints = 0;
		int ServerFarther = 0;
		double ServerDistance = 0.0;
		double ServerPartnerDistance = 0.0;
	};
}
