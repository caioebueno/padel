// The shot system's view of a real match (Shots/BallPort.h over FMatchSimulation), so
// FPlayerShotController plays the match ball the same way it plays the practice ball.
// AI players use it here; gameplay can use it for the human in a match as well.
//
// The serve (GDD 3.2): while the match holds the ball, ServeToss drops a local copy
// that bounces under the port's own fixed step. The serve swing hits that copy, and
// ApplyShot hands the contact to FMatchSimulation::PlayServe, where the referee judges
// the bounce, the waist height and the stance. Letting the toss bounce twice is a
// missed serve.
#pragma once

#include "PadelCoreApi.h"
#include "MatchSimulation.h"
#include "Shots/BallPort.h"

namespace Padel::AI
{
	class PADELCORE_API FAiMatchBallPort : public Shots::IBallPort
	{
	public:
		explicit FAiMatchBallPort(FMatchSimulation& InMatch) : Match(InMatch) {}

		// IBallPort
		Shots::FBallView Current() const override;
		int LastHitterTeam() const override;
		std::vector<Shots::FBallView> Predict(double HorizonS, double StepS) const override;
		void ApplyShot(const Shots::FShotIntent& Intent) override;
		void ServeToss(const FVec3& At, int TeamSign) override;

		// Where the server stands and their waist height, for the serve's stance and
		// height rules. Set before the serve swing makes contact.
		void SetServerStance(const FVec3& Feet, double WaistHeightM);
		// Advances a toss in progress by one match tick. Call once per tick, after the
		// players and before FMatchSimulation::Tick.
		void Tick();
		// Drops any toss (a new serve, or the point was replayed).
		void ResetToss() { bTossing = false; }
		bool IsTossing() const { return bTossing; }
		// True when the last ApplyShot was taken by the match.
		bool LastShotAccepted() const { return bLastAccepted; }

	private:
		// What the shot system needs to know about the ball's bounces (FBallView).
		struct FTrack
		{
			int Half = 1;
			int GroundBounces = 0;
			bool bOffGlass = false;
			bool bLive = true;
		};
		static bool SameTrack(const FTrack& A, const FTrack& B)
		{
			return A.Half == B.Half && A.GroundBounces == B.GroundBounces && A.bOffGlass == B.bOffGlass && A.bLive == B.bLive;
		}
		FTrack MatchTrack() const;
		static void Apply(FTrack& Track, const FBallContact& Contact);
		std::vector<Shots::FBallView> PredictFrom(long long Now, const FBallState& Start, const FTrack& Track, double HorizonS, double StepS) const;

		// Every swing controller predicts every tick (Shots/PlayerShotController.cpp). With
		// no RNG the path from the next tick's ball is the same path one tick on, so one
		// simulated path serves all four players until the ball leaves it (a hit, the
		// mesh's scatter, a new toss). The path is checked against the live ball on use.
		struct FPathCache
		{
			bool bValid = false;
			bool bToss = false;
			long long BaseTick = 0;
			// The path ends where the ball dies (second bounce, out), not at the horizon.
			bool bEndsDead = false;
			std::vector<FBallState> States;
			std::vector<FTrack> Tracks;
		};
		mutable FPathCache Cache;

		FMatchSimulation& Match;
		bool bTossing = false;
		FBallState Toss;
		int TossBounces = 0;
		long long TossTicks = 0;
		FVec3 ServerFeet;
		double ServerWaistM = 1.0;
		bool bLastAccepted = false;
	};
}
