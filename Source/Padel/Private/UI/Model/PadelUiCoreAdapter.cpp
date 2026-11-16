#include "UI/Model/PadelUiCoreAdapter.h"

#include "CourtSpec.h"
#include "PadelTypes.h"

namespace Padel::Ui
{
	namespace
	{
		FUiVec3 CoreToUiVec(const FVec3& Value)
		{
			return {Value.X, Value.Y, Value.Z};
		}
	}

	FScoreView ToScoreView(const FScoreSnapshot& Snapshot, bool bGoldenPointRule)
	{
		FScoreView View;
		View.Points = Snapshot.Points;
		View.Games = Snapshot.Games;
		View.Sets = Snapshot.Sets;
		View.ServingTeam = TeamIndex(Snapshot.ServingTeam);
		View.ServerId = Snapshot.ServerId;
		View.ReceiverId = Snapshot.ReceiverId;
		View.bServeFromRight = Snapshot.ServeSide == EServeSide::Right;
		View.bInTiebreak = Snapshot.bInTiebreak;
		View.bGoldenPoint = bGoldenPointRule || Snapshot.bGoldenPoint;
		View.TeamAEnd = Snapshot.TeamAEnd;
		View.bBreakPoint = Snapshot.bBreakPoint;
		View.bSetPoint = Snapshot.bSetPoint;
		View.bMatchPoint = Snapshot.bMatchPoint;
		View.bMatchOver = Snapshot.bMatchOver;
		// The snapshot's Winner defaults to A; the view says "nobody yet" with -1.
		View.Winner = Snapshot.bMatchOver ? TeamIndex(Snapshot.Winner) : -1;
		return View;
	}

	int HalfIndex(Court::EHalf Half)
	{
		return Half == Court::EHalf::A ? 0 : 1;
	}

	FMatchConfig ToMatchConfig(const FMatchSetup& Setup)
	{
		FMatchConfig Config;
		Config.SetsToWin = Setup.SetsToWin;
		Config.bGoldenPoint = Setup.bGoldenPoint;
		return Config;
	}

	void FCoreEventAdapter::OnServeStarted(int ServerId)
	{
		Sink.OnServeStarted(ServerId);
	}

	void FCoreEventAdapter::OnServeFault(EServeFault Reason)
	{
		Sink.OnServeFault(ServeFaultCode(Reason));
	}

	void FCoreEventAdapter::OnServeLet()
	{
		Sink.OnServeLet();
	}

	void FCoreEventAdapter::OnBallHit(int PlayerId, EShotType ShotType, ETimingGrade TimingGrade, double Speed, const FVec3&)
	{
		Sink.OnBallHit(PlayerId, ShotTypeName(ShotType), TimingGradeName(TimingGrade), Speed);
	}

	void FCoreEventAdapter::OnBallBounced(Court::ESurface Surface, Court::EHalf Half, const FVec3& Position)
	{
		Sink.OnBallBounced(Court::SurfaceName(Surface), HalfIndex(Half), CoreToUiVec(Position));
	}

	void FCoreEventAdapter::OnBallLeftCourt(const FVec3& Position)
	{
		Sink.OnBallLeftCourt(CoreToUiVec(Position));
	}

	void FCoreEventAdapter::OnPointEnded(ETeam WinningTeam, EPointReason Reason, int LastHitterId)
	{
		Sink.OnPointEnded(TeamIndex(WinningTeam), PointReasonCode(Reason), LastHitterId);
	}

	void FCoreEventAdapter::OnScoreChanged(const FScoreSnapshot& Snapshot)
	{
		Sink.OnScoreChanged(ToScoreView(Snapshot, bGoldenPointRule));
	}

	void FCoreEventAdapter::OnGameWon(ETeam Team)
	{
		Sink.OnGameWon(TeamIndex(Team));
	}

	void FCoreEventAdapter::OnSetWon(ETeam Team)
	{
		Sink.OnSetWon(TeamIndex(Team));
	}

	void FCoreEventAdapter::OnMatchWon(ETeam Team)
	{
		Sink.OnMatchWon(TeamIndex(Team));
	}

	void FCoreEventAdapter::OnChangeOfEnds()
	{
		Sink.OnChangeOfEnds();
	}

	void FCoreEventAdapter::OnGoldenPointStarted(ETeam ReceivingTeam)
	{
		Sink.OnGoldenPointStarted(TeamIndex(ReceivingTeam));
	}
}
