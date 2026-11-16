// The bridge from the match simulation's own events (PadelCore's IMatchEventListener) to
// the UI's IUiEventSink: enums become GDD 3.4 codes, the score snapshot becomes an
// FScoreView, and player and team ids pass through unchanged (GDD 2: 0 = A, 1 = B).
//
// UI model: plain C++20, no Unreal headers. It sees PadelCore, which is plain C++ too, so
// the native suite plays real matches through it.

#pragma once

#include "MatchConfig.h"
#include "MatchEventListener.h"
#include "MatchScore.h"
#include "UI/Model/PadelUiEventSink.h"
#include "UI/Model/PadelUiMatchSetup.h"

namespace Padel::Ui
{
	// bGoldenPointRule is the setup's rule; the snapshot only says whether this point is one.
	FScoreView ToScoreView(const FScoreSnapshot& Snapshot, bool bGoldenPointRule);

	// The court half as the team that starts on it (EHalf::A is +X, Team A's first end).
	int HalfIndex(Court::EHalf Half);

	// The setup screen's choices as the simulation's rules.
	FMatchConfig ToMatchConfig(const FMatchSetup& Setup);

	// Listens to one match and forwards every event to Sink. Positions stay in metres.
	class FCoreEventAdapter final : public IMatchEventListener
	{
	public:
		FCoreEventAdapter(IUiEventSink& InSink, bool bInGoldenPointRule) : Sink(InSink), bGoldenPointRule(bInGoldenPointRule) {}

		void SetGoldenPointRule(bool bOn) { bGoldenPointRule = bOn; }

		void OnServeStarted(int ServerId) override;
		void OnServeFault(EServeFault Reason) override;
		void OnServeLet() override;
		void OnBallHit(int PlayerId, EShotType ShotType, ETimingGrade TimingGrade, double Speed, const FVec3& Spin) override;
		void OnBallBounced(Court::ESurface Surface, Court::EHalf Half, const FVec3& Position) override;
		void OnBallLeftCourt(const FVec3& Position) override;
		void OnPointEnded(ETeam WinningTeam, EPointReason Reason, int LastHitterId) override;
		void OnScoreChanged(const FScoreSnapshot& Snapshot) override;
		void OnGameWon(ETeam Team) override;
		void OnSetWon(ETeam Team) override;
		void OnMatchWon(ETeam Team) override;
		void OnChangeOfEnds() override;
		void OnGoldenPointStarted(ETeam ReceivingTeam) override;

	private:
		IUiEventSink& Sink;
		bool bGoldenPointRule = true;
	};
}
