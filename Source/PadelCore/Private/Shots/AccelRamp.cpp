#include "Shots/AccelRamp.h"

#include <algorithm>
#include <cmath>

namespace Padel::Shots::AccelRamp
{
	void Step(double& VX, double& VY, FState& A, double DX, double DY, double MaxAccel, double RiseS, double JerkMax, double Dt)
	{
		const double EX = DX - VX;
		const double EY = DY - VY;
		const double Error = std::sqrt(EX * EX + EY * EY);
		if (RiseS <= 0.0 || JerkMax <= 0.0)
		{
			// The old rule: the change, capped at MaxAccel * Dt, in one tick.
			const double MaxChange = MaxAccel * Dt;
			const double S = Error <= MaxChange || Error <= 0.0 ? 1.0 : MaxChange / Error;
			A.AX = EX * S / Dt;
			A.AY = EY * S / Dt;
			VX += EX * S;
			VY += EY * S;
			return;
		}
		// Settled: at the wanted velocity with the push already eased off. A push still under
		// way runs on (and eases off below), or zeroing it here would be a jolt.
		if (Error <= SettledMps && std::sqrt(A.AX * A.AX + A.AY * A.AY) <= JerkMax * Dt)
		{
			VX = DX;
			VY = DY;
			A = FState{};
			return;
		}
		// The target: no more than MaxAccel, and no more than the acceleration that can still
		// fall to zero at the jerk limit by the time the change is done, so the last of a stop
		// or a start eases off and ends, instead of trailing on or running past. In whole
		// ticks: a tick at A leaves e - A Dt, and A^2 <= 2 J (e - A Dt) solves to the root below.
		const double JDt = JerkMax * Dt;
		const double Wanted = std::min(-JDt + std::sqrt(JDt * JDt + 2.0 * JerkMax * Error), MaxAccel);
		const double TX = Error > 0.0 ? EX / Error * Wanted : 0.0;
		const double TY = Error > 0.0 ? EY / Error * Wanted : 0.0;
		// The acceleration moves toward it at the jerk limit.
		double CX = TX - A.AX;
		double CY = TY - A.AY;
		const double Change = std::sqrt(CX * CX + CY * CY);
		const double MaxStep = JerkMax * Dt;
		if (Change > MaxStep)
		{
			CX *= MaxStep / Change;
			CY *= MaxStep / Change;
		}
		A.AX += CX;
		A.AY += CY;
		// No clamp at the wanted velocity: when it moves (the speed asked for drops while the
		// player is still pushing), the push has to ease off, so the velocity runs on a
		// little past it, as a body's would, and comes back.
		const double NX = VX + A.AX * Dt;
		const double NY = VY + A.AY * Dt;
		VX = NX;
		VY = NY;
	}

	double StopDistance(double V, double A, double MaxAccel, double RiseS, double JerkMax, double Dt)
	{
		// Standing still with no push under way: nothing to stop.
		if (V < 0.0 || (V == 0.0 && A <= 0.0))
		{
			return 0.0;
		}
		// Long enough for any stop from a human speed (the settling ends at SettledMps).
		constexpr int MaxSteps = 1200;
		double VX = V;
		double VY = 0.0;
		FState S{ A, 0.0 };
		double Distance = 0.0;
		for (int I = 0; I < MaxSteps; ++I)
		{
			Step(VX, VY, S, 0.0, 0.0, MaxAccel, RiseS, JerkMax, Dt);
			if (VX <= 0.0)
			{
				break;
			}
			Distance += VX * Dt;
		}
		return Distance;
	}

	double StopSpeed(double DistanceM, double Decel, double RiseS)
	{
		const double D = std::max(0.0, DistanceM);
		if (Decel <= 0.0)
		{
			return 0.0;
		}
		if (RiseS <= 0.0)
		{
			return std::sqrt(2.0 * Decel * D);
		}
		// v^2 / (2 a) + v R + a R^2 / 24 = D, solved for v.
		const double Room = std::max(0.0, D - Decel * RiseS * RiseS / 24.0);
		const double Planned = Decel * (-RiseS + std::sqrt(RiseS * RiseS + 2.0 * Room / Decel));
		// Inside the last few centimetres only the settling is left, which covers v * RiseS.
		return std::max(Planned, std::min(D / RiseS, Decel * RiseS));
	}

	bool FPivot::Update(double VX, double VY, double DX, double DY, const FPivotParams& P, double Dt)
	{
		if (P.CosMin > 1.0)
		{
			Stage = EStage::None;
			return false;
		}
		const double Speed = std::sqrt(VX * VX + VY * VY);
		const double Wanted = std::sqrt(DX * DX + DY * DY);
		const auto Opposed = [DX, DY, Wanted, &P](double X, double Y)
		{
			return Wanted > 0.0 && (DX * X + DY * Y) / Wanted <= P.CosMin;
		};
		if (Stage == EStage::None)
		{
			if (Speed <= P.StartMps || !Opposed(VX / Speed, VY / Speed))
			{
				return false;
			}
			HX = VX / Speed;
			HY = VY / Speed;
			Stage = EStage::Braking;
		}
		else if (!Opposed(HX, HY))
		{
			// The stick let go, or swung back toward the old way: no pivot to finish.
			Stage = EStage::None;
			return false;
		}
		if (Stage == EStage::Braking && Speed <= P.PlantMps)
		{
			Stage = EStage::Planted;
			LeftS = P.PlantS;
		}
		if (Stage == EStage::Planted)
		{
			if (LeftS <= 0.0)
			{
				Stage = EStage::None;
				return false;
			}
			LeftS -= Dt;
		}
		return true;
	}
}
