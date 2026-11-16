#include "UI/PadelMatchEventBinder.h"

#include "Court/PadelCourtUnits.h"
#include "Presentation/PadelMatchEvents.h"
#include "UI/Model/PadelUiCoreAdapter.h"

namespace
{
	// One handle per GDD 3.4 event.
	constexpr int32 NumBinderEvents = 13;

	Padel::FVec3 BinderToVec3(const FVector& Value)
	{
		return Padel::FVec3{Value.X, Value.Y, Value.Z};
	}

	Padel::FVec3 BinderWorldToMetres(const FVector& World)
	{
		return BinderToVec3(Padel::WorldToMetres(World));
	}
}

FPadelMatchEventBinder::FPadelMatchEventBinder() = default;

FPadelMatchEventBinder::~FPadelMatchEventBinder()
{
	Unbind();
}

void FPadelMatchEventBinder::Bind(UPadelMatchEvents& InEvents, Padel::Ui::IUiEventSink& Sink, bool bGoldenPointRule)
{
	using namespace Padel;
	Unbind();
	Events = &InEvents;
	Adapter = MakeUnique<Ui::FCoreEventAdapter>(Sink, bGoldenPointRule);
	Ui::FCoreEventAdapter* A = Adapter.Get();

	Handles.Add(InEvents.OnServeStarted.AddLambda([A](int32 ServerId) { A->OnServeStarted(ServerId); }));
	Handles.Add(InEvents.OnServeFault.AddLambda([A](EServeFault Reason) { A->OnServeFault(Reason); }));
	Handles.Add(InEvents.OnServeLet.AddLambda([A]() { A->OnServeLet(); }));
	Handles.Add(InEvents.OnBallHit.AddLambda(
		[A](int32 PlayerId, EShotType Shot, ETimingGrade Grade, double SpeedMps, const FVector& Spin)
		{
			// Spin is rad/s, not a position, so it keeps its units.
			A->OnBallHit(PlayerId, Shot, Grade, SpeedMps, BinderToVec3(Spin));
		}));
	Handles.Add(InEvents.OnBallBounced.AddLambda(
		[A](Court::ESurface Surface, Court::EHalf Half, const FVector& World) { A->OnBallBounced(Surface, Half, BinderWorldToMetres(World)); }));
	Handles.Add(InEvents.OnBallLeftCourt.AddLambda([A](const FVector& World) { A->OnBallLeftCourt(BinderWorldToMetres(World)); }));
	Handles.Add(InEvents.OnPointEnded.AddLambda(
		[A](ETeam Team, EPointReason Reason, int32 LastHitterId) { A->OnPointEnded(Team, Reason, LastHitterId); }));
	Handles.Add(InEvents.OnScoreChanged.AddLambda([A](const FScoreSnapshot& Snapshot) { A->OnScoreChanged(Snapshot); }));
	Handles.Add(InEvents.OnGameWon.AddLambda([A](ETeam Team) { A->OnGameWon(Team); }));
	Handles.Add(InEvents.OnSetWon.AddLambda([A](ETeam Team) { A->OnSetWon(Team); }));
	Handles.Add(InEvents.OnMatchWon.AddLambda([A](ETeam Team) { A->OnMatchWon(Team); }));
	Handles.Add(InEvents.OnChangeOfEnds.AddLambda([A]() { A->OnChangeOfEnds(); }));
	Handles.Add(InEvents.OnGoldenPointStarted.AddLambda([A](ETeam Team) { A->OnGoldenPointStarted(Team); }));
}

void FPadelMatchEventBinder::Unbind()
{
	UPadelMatchEvents* Bound = Events.Get();
	if (Bound && Handles.Num() == NumBinderEvents)
	{
		// Same order as Bind.
		int32 I = 0;
		Bound->OnServeStarted.Remove(Handles[I++]);
		Bound->OnServeFault.Remove(Handles[I++]);
		Bound->OnServeLet.Remove(Handles[I++]);
		Bound->OnBallHit.Remove(Handles[I++]);
		Bound->OnBallBounced.Remove(Handles[I++]);
		Bound->OnBallLeftCourt.Remove(Handles[I++]);
		Bound->OnPointEnded.Remove(Handles[I++]);
		Bound->OnScoreChanged.Remove(Handles[I++]);
		Bound->OnGameWon.Remove(Handles[I++]);
		Bound->OnSetWon.Remove(Handles[I++]);
		Bound->OnMatchWon.Remove(Handles[I++]);
		Bound->OnChangeOfEnds.Remove(Handles[I++]);
		Bound->OnGoldenPointStarted.Remove(Handles[I++]);
	}
	Handles.Reset();
	Events.Reset();
	Adapter.Reset();
}

void FPadelMatchEventBinder::SetGoldenPointRule(bool bOn)
{
	if (Adapter)
	{
		Adapter->SetGoldenPointRule(bOn);
	}
}

bool FPadelMatchEventBinder::HasLiveMatch() const
{
	const UPadelMatchEvents* Bound = Events.Get();
	return Bound && Bound->IsAttached();
}
