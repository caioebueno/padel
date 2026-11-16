#include "Shots/HardEffortCounter.h"

#include "BallState.h"

#include <algorithm>
#include <cmath>

namespace Padel::Shots
{
	FHardEffortCounter::FHardEffortCounter()
		: WindowTicks(std::max(1, static_cast<int>(std::lround(SmoothingS / SimTickSeconds))))
		, MinTicks(std::max(1, static_cast<int>(std::lround(MinDurationS / SimTickSeconds))))
	{
		Reset();
	}

	void FHardEffortCounter::Reset()
	{
		History.assign(static_cast<size_t>(WindowTicks) + 1, FVec3{});
		Filled = 0;
		bEdgeStopInWindow = false;
		TicksSinceEdgeStop = 0;
		AboveTicks = 0;
		bCounted = false;
	}

	bool FHardEffortCounter::Tick(const FVec3& Velocity, bool bEdgeStop, double ThresholdMps2)
	{
		// Mirrors FAiMovementMetrics::TrackAcceleration step for step.
		const int Size = WindowTicks + 1;
		History[static_cast<size_t>(Filled % Size)] = Velocity;
		++Filled;
		if (bEdgeStop)
		{
			bEdgeStopInWindow = true;
			TicksSinceEdgeStop = 0;
		}
		else if (bEdgeStopInWindow && ++TicksSinceEdgeStop >= WindowTicks)
		{
			bEdgeStopInWindow = false;
		}
		if (Filled < Size || bEdgeStopInWindow)
		{
			AboveTicks = 0;
			return false;
		}

		// The trailing average over N ticks moves by (v_t - v_t-N) / N each tick.
		const FVec3& Oldest = History[static_cast<size_t>(Filled % Size)];
		const double Accel = SizeXY(Velocity - Oldest) / (WindowTicks * SimTickSeconds);
		if (Accel > ThresholdMps2)
		{
			++AboveTicks;
			if (AboveTicks >= MinTicks && !bCounted)
			{
				bCounted = true;
				return true;
			}
			return false;
		}
		AboveTicks = 0;
		bCounted = false;
		return false;
	}
}
