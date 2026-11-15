// GDD 5.1: grades the release against the ideal contact moment.

#pragma once

#include "Shots/ShotMath.h"

#include "PadelTypes.h"
#include "Shots/ShotModel.h"
#include "Shots/ShotParams.h"

namespace Padel::Shots
{
	struct PADELCORE_API FTimingGrader
	{
		// x(1 - WindowNarrow) at rating 1 up to x(1 + WindowWiden) at 99.
		static double WindowScale(double RatingUnit, const FShotModel& Model);

		// OffsetS = release minus ideal moment (negative = early).
		static ETimingGrade Grade(double OffsetS, const FShotParams& Shot, double RatingUnit, const FShotModel& Model);

		// The late edge, seconds after the ideal moment: a held button fires here (GDD 5.1).
		static double LateEdgeS(const FShotParams& Shot, double RatingUnit, const FShotModel& Model);

		static double ErrorMult(ETimingGrade Grade, const FShotModel& Model);
	};
}
