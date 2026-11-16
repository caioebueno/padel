// The one place the AI's types meet the engine's (MatchSimulation, BallSim) and the
// shot system's (Shots/). Both sides are metres on the same axes (GDD 2), so these are
// field copies and enum maps; the centimetre conversion stays at the Padel boundary.
#pragma once

#include "PadelCoreApi.h"
#include "AI/AiBallModel.h"
#include "AI/AiPlayerProfile.h"
#include "AI/AiTuning.h"
#include "AI/AiTypes.h"
#include "BallState.h"
#include "PadelTypes.h"
#include "Shots/PlayerInputFrame.h"
#include "Shots/PlayerRatings.h"
#include "Shots/ShotLibrary.h"

namespace Padel::AI::Bridge
{
	inline FAiVec ToAi(const FVec3& V) { return { V.X, V.Y, V.Z }; }
	inline FVec3 ToEngine(const FAiVec& V) { return { V.X, V.Y, V.Z }; }

	inline FAiBall ToAi(const FBallState& S)
	{
		FAiBall B;
		B.Position = ToAi(S.Position);
		B.Velocity = ToAi(S.Velocity);
		B.Spin = ToAi(S.Spin);
		B.bRolling = S.bRolling;
		B.bInCourt = S.bInCourt;
		B.bTouchingNet = S.bTouchingNet;
		return B;
	}

	inline FBallState ToEngine(const FAiBall& B)
	{
		FBallState S;
		S.Position = ToEngine(B.Position);
		S.Velocity = ToEngine(B.Velocity);
		S.Spin = ToEngine(B.Spin);
		S.bRolling = B.bRolling;
		S.bInCourt = B.bInCourt;
		S.bTouchingNet = B.bTouchingNet;
		return S;
	}

	PADELCORE_API EShotType ToEngine(EAiShot Shot);
	// A mishit reads as the drive it was meant to be: the AI only uses the type to
	// guess what is coming (a lob or not).
	PADELCORE_API EAiShot ToAi(EShotType Shot);

	// The ratings the AI plans with, from the same player row the shot system uses.
	PADELCORE_API FAiPlayerProfile ProfileFromRatings(const Shots::FPlayerRatings& Ratings);
	// GDD 8: the difficulty's attribute scale, on every rating, clamped to 1..99.
	PADELCORE_API Shots::FPlayerRatings WithOffset(const Shots::FPlayerRatings& Ratings, int Offset);

	// GDD 5.2: the tactics with each contact kind's stand point taken from the shot it
	// usually plays (groundstroke: drive, volley: volley, overhead: bandeja), so the AI
	// stands where the shot model grades the ball.
	PADELCORE_API FAiTactics WithShotContactPoints(const FAiTactics& Tactics, const Shots::FShotLibrary& Library);

	// The AI's move as a stick frame. Both are team-relative (X right, Y toward the net).
	inline Shots::FPlayerInputFrame InputFrom(const FAiIntent& Intent)
	{
		Shots::FPlayerInputFrame Frame;
		Frame.Move = { Intent.Move.X, Intent.Move.Y };
		Frame.bSprint = Intent.bSprint;
		return Frame;
	}
}
