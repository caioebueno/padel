#include "Shots/TimingGrader.h"

#include "Shots/ShotMath.h"

#include <cmath>

namespace Padel::Shots
{
	double FTimingGrader::WindowScale(double RatingUnit, const FShotModel& Model)
	{
		const double T = Clamp(InverseLerp(1.0 / 99.0, 1.0, RatingUnit), 0.0, 1.0);
		return Lerp(1.0 - Model.WindowNarrow, 1.0 + Model.WindowWiden, T);
	}

	ETimingGrade FTimingGrader::Grade(double OffsetS, const FShotParams& Shot, double RatingUnit, const FShotModel& Model)
	{
		const double Scale = WindowScale(RatingUnit, Model);
		const double Ms = std::abs(OffsetS) * 1000.0;
		if (Ms <= Shot.PerfectWindowMs * Scale)
		{
			return ETimingGrade::Perfect;
		}
		if (Ms <= Shot.GoodWindowMs * Scale)
		{
			return ETimingGrade::Good;
		}
		if (Ms <= Shot.EarlyLateWindowMs * Scale)
		{
			return OffsetS < 0.0 ? ETimingGrade::Early : ETimingGrade::Late;
		}
		return ETimingGrade::Miss;
	}

	double FTimingGrader::LateEdgeS(const FShotParams& Shot, double RatingUnit, const FShotModel& Model)
	{
		return Shot.EarlyLateWindowMs * WindowScale(RatingUnit, Model) / 1000.0;
	}

	double FTimingGrader::ErrorMult(ETimingGrade Grade, const FShotModel& Model)
	{
		switch (Grade)
		{
		case ETimingGrade::Perfect: return Model.PerfectErrorMult;
		case ETimingGrade::Good: return Model.GoodErrorMult;
		default: return Model.EarlyLateErrorMult;
		}
	}
}
