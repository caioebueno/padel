// The GDD 3.4 referee events, as a plain C++ interface. UPadelMatchEvents (in Padel)
// implements it and rebroadcasts each call as its On* multicast delegate, so the UI,
// audio, replays and stats never touch the simulation. Every method has an empty
// default so a listener overrides only what it needs.

#pragma once

#include "MatchScore.h"
#include "VecMath.h"

namespace Padel
{
	class IMatchEventListener
	{
	public:
		virtual ~IMatchEventListener() = default;

		virtual void OnServeStarted(int /*ServerId*/) {}
		virtual void OnServeFault(EServeFault /*Reason*/) {}
		virtual void OnServeLet() {}
		virtual void OnBallHit(int /*PlayerId*/, EShotType /*ShotType*/, ETimingGrade /*TimingGrade*/, double /*Speed*/, const FVec3& /*Spin*/) {}
		virtual void OnBallBounced(Court::ESurface /*Surface*/, Court::EHalf /*Half*/, const FVec3& /*Position*/) {}
		virtual void OnBallLeftCourt(const FVec3& /*Position*/) {}
		virtual void OnPointEnded(ETeam /*WinningTeam*/, EPointReason /*Reason*/, int /*LastHitterId*/) {}
		virtual void OnScoreChanged(const FScoreSnapshot& /*Snapshot*/) {}
		virtual void OnGameWon(ETeam /*Team*/) {}
		virtual void OnSetWon(ETeam /*Team*/) {}
		virtual void OnMatchWon(ETeam /*Team*/) {}
		virtual void OnChangeOfEnds() {}
		virtual void OnGoldenPointStarted(ETeam /*ReceivingTeam*/) {}
	};
}
