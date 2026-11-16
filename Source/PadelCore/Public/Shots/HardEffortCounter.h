// Counts hard efforts (GDD 6.1 Stamina: sharp starts and stops) for fatigue, GDD 6.3
// (BIOMECHANICS_MOVEMENT 2.13, 5.5). It counts the way the soak's realism metrics count
// acceleration events (AI/AiMovementMetrics, realism plan A1), so a hard effort in play
// is the same thing the soak measures: the change of the velocity's 0.2 s trailing
// average, above the threshold for at least 0.3 s, re-armed once it drops back below.
// A tick whose window holds a wall or net dead stop is left out, like the metrics do.
//
// Fixed-step and engine-free (one Tick per FPlayerMotion step at SimTickSeconds).

#pragma once

#include "PadelCoreApi.h"
#include "VecMath.h"

#include <vector>

namespace Padel::Shots
{
	class PADELCORE_API FHardEffortCounter
	{
	public:
		// The A1 measurement's window and duration (AiMovementMetricsConfig's defaults;
		// a test keeps the two equal). They define what counts, not how anyone plays.
		static constexpr double SmoothingS = 0.2;
		static constexpr double MinDurationS = 0.3;

		FHardEffortCounter();

		// Forgets the window: the next stretch starts from scratch (a teleport, a new rally).
		void Reset();
		// One tick at ThresholdMps2. Returns true on the tick a hard effort is counted.
		bool Tick(const FVec3& Velocity, bool bEdgeStop, double ThresholdMps2);

	private:
		int WindowTicks = 1;
		int MinTicks = 1;
		std::vector<FVec3> History;
		int Filled = 0;
		bool bEdgeStopInWindow = false;
		int TicksSinceEdgeStop = 0;
		int AboveTicks = 0;
		bool bCounted = false;
	};
}
