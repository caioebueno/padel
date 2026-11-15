// What the UI and presentation hear from a match: the GDD 3.4 events, with codes
// already turned into GDD names (see PadelUiCodes.h) and the score as an FScoreView.
//
// Mirrors PadelCore's Padel::IMatchEventListener one-to-one (Agent 2), so the adapter
// between them is a list of one-line conversions. The demo driver also speaks it.
//
// UI model: plain C++20, no Unreal headers.

#pragma once

#include "UI/Model/PadelUiScore.h"

#include <string_view>

namespace Padel::Ui
{
	struct FUiVec3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	class IUiEventSink
	{
	public:
		virtual ~IUiEventSink() = default;

		virtual void OnServeStarted(int ServerId) = 0;
		virtual void OnServeFault(std::string_view Fault) = 0;
		virtual void OnServeLet() = 0;
		virtual void OnBallHit(int PlayerId, std::string_view ShotType, std::string_view TimingGrade, double SpeedMps) = 0;
		virtual void OnBallBounced(std::string_view Surface, int Half, const FUiVec3& PositionM) = 0;
		virtual void OnBallLeftCourt(const FUiVec3& PositionM) = 0;
		virtual void OnPointEnded(int WinningTeam, std::string_view Reason, int LastHitterId) = 0;
		virtual void OnScoreChanged(const FScoreView& Score) = 0;
		virtual void OnGameWon(int Team) = 0;
		virtual void OnSetWon(int Team) = 0;
		virtual void OnMatchWon(int Team) = 0;
		virtual void OnChangeOfEnds() = 0;
		virtual void OnGoldenPointStarted(int ReceivingTeam) = 0;
	};
}
