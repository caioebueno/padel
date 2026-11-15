#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/UniquePtr.h"

#include "MatchEventListener.h"
#include "MatchScore.h"
#include "PadelTypes.h"

#include "PadelMatchEvents.generated.h"

namespace Padel
{
	class FMatchSimulation;
}

// GDD 3.4 events as native multicast delegates, one per event, with the GDD's parameters
// in the GDD's order. They are native rather than dynamic so they can carry PadelCore's
// own enums and score snapshot unchanged: listeners are C++ (CLAUDE.md, logic in C++).
// Positions are world positions in centimetres, converted once in this subsystem; spin
// is in rad/s and speed in m/s, as the simulation reports them.
DECLARE_MULTICAST_DELEGATE_OneParam(FPadelServeStartedEvent, int32 /*ServerId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPadelServeFaultEvent, Padel::EServeFault /*Reason*/);
DECLARE_MULTICAST_DELEGATE(FPadelServeLetEvent);
DECLARE_MULTICAST_DELEGATE_FiveParams(FPadelBallHitEvent, int32 /*PlayerId*/, Padel::EShotType /*ShotType*/,
	Padel::ETimingGrade /*TimingGrade*/, double /*SpeedMps*/, const FVector& /*Spin*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FPadelBallBouncedEvent, Padel::Court::ESurface /*Surface*/,
	Padel::Court::EHalf /*Half*/, const FVector& /*WorldPosition*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPadelBallLeftCourtEvent, const FVector& /*WorldPosition*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FPadelPointEndedEvent, Padel::ETeam /*WinningTeam*/,
	Padel::EPointReason /*Reason*/, int32 /*LastHitterId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPadelScoreChangedEvent, const Padel::FScoreSnapshot& /*Snapshot*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPadelGameWonEvent, Padel::ETeam /*Team*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPadelSetWonEvent, Padel::ETeam /*Team*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPadelMatchWonEvent, Padel::ETeam /*Team*/);
DECLARE_MULTICAST_DELEGATE(FPadelChangeOfEndsEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FPadelGoldenPointStartedEvent, Padel::ETeam /*ReceivingTeam*/);

/**
 * The one place where the match simulation's events enter the Unreal world (GDD 3.4).
 * UI, audio, camera, replays and stats bind to these delegates and never touch the
 * simulation (TEST_PLAN U-03).
 *
 * Whoever owns the FMatchSimulation calls Attach once it exists and Detach before it is
 * destroyed. The simulation keeps a raw pointer to this subsystem, and this subsystem
 * keeps one to the simulation, so the order matters. Deinitialize detaches too, for the
 * case where the world goes away first.
 *
 * The simulation's listener is a small forwarder object (in the .cpp) rather than this
 * class, because the listener's methods and these delegates share the GDD names.
 */
UCLASS()
class PADEL_API UPadelMatchEvents : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Starts forwarding Match's events. Detaches from any earlier match first. */
	void Attach(Padel::FMatchSimulation& Match);

	/** Stops forwarding. Safe to call when nothing is attached. */
	void Detach();

	bool IsAttached() const { return AttachedMatch != nullptr; }

	virtual void Deinitialize() override;

	FPadelServeStartedEvent OnServeStarted;
	FPadelServeFaultEvent OnServeFault;
	FPadelServeLetEvent OnServeLet;
	FPadelBallHitEvent OnBallHit;
	FPadelBallBouncedEvent OnBallBounced;
	FPadelBallLeftCourtEvent OnBallLeftCourt;
	FPadelPointEndedEvent OnPointEnded;
	FPadelScoreChangedEvent OnScoreChanged;
	FPadelGameWonEvent OnGameWon;
	FPadelSetWonEvent OnSetWon;
	FPadelMatchWonEvent OnMatchWon;
	FPadelChangeOfEndsEvent OnChangeOfEnds;
	FPadelGoldenPointStartedEvent OnGoldenPointStarted;

private:
	// Converts each event to Unreal types and broadcasts it; created on first Attach.
	TUniquePtr<Padel::IMatchEventListener> Forwarder;
	Padel::FMatchSimulation* AttachedMatch = nullptr;
};
