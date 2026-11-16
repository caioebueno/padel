#include "Presentation/PadelMatchEvents.h"

#include "Court/PadelCourtUnits.h"
#include "MatchSimulation.h"
#include "PadelLog.h"

namespace
{
	// PadelCore calls these listener methods; each converts its arguments to Unreal types
	// and broadcasts the matching delegate. Event positions turn from metres into
	// centimetres here and nowhere else.
	class FPadelMatchEventForwarder final : public Padel::IMatchEventListener
	{
	public:
		explicit FPadelMatchEventForwarder(UPadelMatchEvents& InOwner) : Owner(InOwner) {}

		virtual void OnServeStarted(int ServerId) override;
		virtual void OnServeFault(Padel::EServeFault Reason) override;
		virtual void OnServeLet() override;
		virtual void OnBallHit(int PlayerId, Padel::EShotType ShotType, Padel::ETimingGrade TimingGrade, double Speed,
			const Padel::FVec3& Spin) override;
		virtual void OnBallBounced(Padel::Court::ESurface Surface, Padel::Court::EHalf Half,
			const Padel::FVec3& Position) override;
		virtual void OnBallLeftCourt(const Padel::FVec3& Position) override;
		virtual void OnPointEnded(Padel::ETeam WinningTeam, Padel::EPointReason Reason, int LastHitterId) override;
		virtual void OnScoreChanged(const Padel::FScoreSnapshot& Snapshot) override;
		virtual void OnGameWon(Padel::ETeam Team) override;
		virtual void OnSetWon(Padel::ETeam Team) override;
		virtual void OnMatchWon(Padel::ETeam Team) override;
		virtual void OnChangeOfEnds() override;
		virtual void OnGoldenPointStarted(Padel::ETeam ReceivingTeam) override;

		// Spin and other unscaled vectors keep their units.
		static FVector ToVector(const Padel::FVec3& Value);
		static FVector ToWorld(const Padel::FVec3& Metres);

	private:
		UPadelMatchEvents& Owner;
	};

	FVector FPadelMatchEventForwarder::ToVector(const Padel::FVec3& Value)
	{
		return FVector(Value.X, Value.Y, Value.Z);
	}

	FVector FPadelMatchEventForwarder::ToWorld(const Padel::FVec3& Metres)
	{
		return Padel::MetresToWorld(ToVector(Metres));
	}

	void FPadelMatchEventForwarder::OnServeStarted(int ServerId)
	{
		Owner.OnServeStarted.Broadcast(ServerId);
	}

	void FPadelMatchEventForwarder::OnServeFault(Padel::EServeFault Reason)
	{
		Owner.OnServeFault.Broadcast(Reason);
	}

	void FPadelMatchEventForwarder::OnServeLet()
	{
		Owner.OnServeLet.Broadcast();
	}

	void FPadelMatchEventForwarder::OnBallHit(int PlayerId, Padel::EShotType ShotType, Padel::ETimingGrade TimingGrade,
		double Speed, const Padel::FVec3& Spin)
	{
		Owner.OnBallHit.Broadcast(PlayerId, ShotType, TimingGrade, Speed, ToVector(Spin));
	}

	void FPadelMatchEventForwarder::OnBallBounced(Padel::Court::ESurface Surface, Padel::Court::EHalf Half,
		const Padel::FVec3& Position)
	{
		Owner.OnBallBounced.Broadcast(Surface, Half, ToWorld(Position));
	}

	void FPadelMatchEventForwarder::OnBallLeftCourt(const Padel::FVec3& Position)
	{
		Owner.OnBallLeftCourt.Broadcast(ToWorld(Position));
	}

	void FPadelMatchEventForwarder::OnPointEnded(Padel::ETeam WinningTeam, Padel::EPointReason Reason, int LastHitterId)
	{
		Owner.OnPointEnded.Broadcast(WinningTeam, Reason, LastHitterId);
	}

	void FPadelMatchEventForwarder::OnScoreChanged(const Padel::FScoreSnapshot& Snapshot)
	{
		Owner.OnScoreChanged.Broadcast(Snapshot);
	}

	void FPadelMatchEventForwarder::OnGameWon(Padel::ETeam Team)
	{
		Owner.OnGameWon.Broadcast(Team);
	}

	void FPadelMatchEventForwarder::OnSetWon(Padel::ETeam Team)
	{
		Owner.OnSetWon.Broadcast(Team);
	}

	void FPadelMatchEventForwarder::OnMatchWon(Padel::ETeam Team)
	{
		Owner.OnMatchWon.Broadcast(Team);
	}

	void FPadelMatchEventForwarder::OnChangeOfEnds()
	{
		Owner.OnChangeOfEnds.Broadcast();
	}

	void FPadelMatchEventForwarder::OnGoldenPointStarted(Padel::ETeam ReceivingTeam)
	{
		Owner.OnGoldenPointStarted.Broadcast(ReceivingTeam);
	}
}

void UPadelMatchEvents::Attach(Padel::FMatchSimulation& Match)
{
	if (AttachedMatch == &Match)
	{
		return;
	}
	Detach();
	if (!Forwarder)
	{
		Forwarder = MakeUnique<FPadelMatchEventForwarder>(*this);
	}
	AttachedMatch = &Match;
	Match.AddListener(Forwarder.Get());
	UE_LOG(LogPadel, Verbose, TEXT("Match events attached"));
}

void UPadelMatchEvents::Detach()
{
	if (AttachedMatch)
	{
		AttachedMatch->RemoveListener(Forwarder.Get());
		AttachedMatch = nullptr;
		UE_LOG(LogPadel, Verbose, TEXT("Match events detached"));
	}
}

void UPadelMatchEvents::Deinitialize()
{
	Detach();
	Super::Deinitialize();
}
