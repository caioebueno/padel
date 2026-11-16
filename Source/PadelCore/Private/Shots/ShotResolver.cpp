#include "Shots/ShotResolver.h"
#include "PadelMath.h"
#include "CourtSpec.h"

#include "Shots/AimMapper.h"
#include "Shots/TimingGrader.h"

#include <cmath>

namespace Padel::Shots
{
	// Named, not anonymous: Unreal unity builds merge .cpp files.
	namespace ShotResolverDetail
	{
		FShotIntent BaseIntent(const FShotContext& C, EShotType Type, ETimingGrade Grade)
		{
			FShotIntent Intent;
			Intent.PlayerId = C.PlayerId;
			Intent.ShotType = Type;
			Intent.TimingGrade = Grade;
			Intent.TimingOffsetS = C.TimingOffsetS;
			Intent.Origin = C.ContactPosition;
			Intent.Charge = C.Charge;
			Intent.bWallShot = C.bOffGlass;
			Intent.bVolley = !C.bBallBounced;
			return Intent;
		}
		FVec3 AimedTarget(const FShotContext& C, const FShotParams& Shot)
		{
			if (C.bHasTargetOverride)
			{
				return C.TargetOverride;
			}
			if (C.bServe)
			{
				return FAimMapper::ServeTarget(C.Aim, Shot, C.TeamSign, C.ContactPosition.Y);
			}
			return FAimMapper::Target(C.Aim, Shot, C.TeamSign);
		}

		double SideMult(const FShotContext& C, const FShotModel& Model)
		{
			return C.bWrongSide ? Model.WrongSideMult : 1.0;
		}
	}
	using namespace ShotResolverDetail;

	FShotIntent FShotResolver::Resolve(const FShotContext& C, const FShotLibrary& Library, FPadelRng& Rng)
	{
		const FShotModel& Model = Library.Model;
		const FShotChoice Choice = FShotSelector::Select(C, Library);
		const FShotParams& Shot = Library.Get(Choice.ShotType);
		const double S = MainRating(C, Shot, Model);
		const ETimingGrade Grade = FTimingGrader::Grade(C.TimingOffsetS, Shot, TimingRating(C, Shot, Model), Model);
		if (Grade == ETimingGrade::Miss)
		{
			return Mishit(C, Model, Rng);
		}

		FShotIntent Intent = BaseIntent(C, Choice.ShotType, Grade);
		const double P = C.Player->Unit(ERating::Power);
		const double Charge = Clamp(C.Charge, 0.0, 1.0);

		// GDD 5.3 speed. "Max speed x charge" is read as a lerp across the shot's own
		// speed range, so a tap still has the shot's minimum pace.
		const double MaxSpeed = Shot.SpeedMaxMps() * Lerp(Model.SpeedPowerMin, Model.SpeedPowerMax, P) * StaminaFactor(C.Stamina, Model);
		const double MinSpeed = Shot.SpeedMinMps() < MaxSpeed ? Shot.SpeedMinMps() : MaxSpeed;
		double Speed = Lerp(MinSpeed, MaxSpeed, Charge) * PositionPowerLimit(C.PositionDistanceM, Model);
		if (Grade == ETimingGrade::Perfect)
		{
			Speed *= 1.0 + Model.PerfectSpeedBonus;
		}
		Intent.SpeedMps = Speed;

		// GDD 5.3 spin. Side spin follows the forehand, so a víbora curves the same way
		// relative to the body for left- and right-handers.
		const double SpinScale = Lerp(Model.SpinRatingMin, Model.SpinRatingMax, S);
		Intent.Spin = Vec(Shot.BaseSpinTop * SpinScale, Shot.BaseSpinSide * SpinScale * C.Player->ForehandSign(), 0.0);
		if (Shot.UsesApex())
		{
			Intent.ApexHeightM = Lerp(Shot.ApexMinM, Shot.ApexMaxM, Charge);
		}

		const FVec3 Aimed = AimedTarget(C, Shot);
		Intent.AimedTarget = Aimed;
		const double Radius = ScatterRadius(C, Shot, Choice, S, Grade, Model);
		Intent.ScatterRadiusM = Radius;

		// GDD 5.1: early pulls the ball across the body, late pushes it toward the contact side.
		double Pull = 0.0;
		if (Grade == ETimingGrade::Early)
		{
			Pull = -C.ContactSide * Model.EarlyLatePullM;
		}
		else if (Grade == ETimingGrade::Late)
		{
			Pull = C.ContactSide * Model.EarlyLatePullM;
		}
		const double Angle = Rng.NextDouble() * Tau;
		const double R = std::sqrt(Rng.NextDouble()) * Radius;
		Intent.Target = Add(Add(Aimed, Scale(RightAxis(C.TeamSign), Pull)), Vec(Padel::Math::Cos(Angle) * R, Padel::Math::Sin(Angle) * R, 0.0));

		// A low ball can't be driven flat over the tape: the player opens the face, and the
		// ball goes slower on a higher arc to the same spot.
		if (!Shot.UsesApex() && C.ContactPosition.Z < Shot.LowContactHeightM && Shot.LowContactNetClearanceM > 0.0)
		{
			Intent.SpeedMps = NetClearingSpeed(C.ContactPosition, Intent.Target, Intent.SpeedMps, Shot.LowContactNetClearanceM, Model.GravityMps2);
		}
		return Intent;
	}

	double FShotResolver::NetCrossingHeight(const FVec3& Origin, const FVec3& Target, double SpeedMps, double Gravity)
	{
		// The flatter of the two gravity-only arcs at this speed, as FPracticeBall launches
		// it; spin and drag in the match only make a drive cross higher.
		const double D = LengthXY(Sub(Target, Origin));
		if (D < 1e-6 || SpeedMps <= 0.0 || (Origin.X > 0.0) == (Target.X > 0.0))
		{
			return Origin.Z;
		}
		const double A = Gravity * D * D / (2.0 * SpeedMps * SpeedMps);
		const double Disc = D * D - 4.0 * A * (Target.Z - Origin.Z + A);
		if (Disc < 0.0)
		{
			return -1.0; // can't reach the target at this speed
		}
		const double TanTheta = (D - std::sqrt(Disc)) / (2.0 * A);
		const double Xn = D * Origin.X / (Origin.X - Target.X);
		return Origin.Z + Xn * TanTheta - Gravity * Xn * Xn * (1.0 + TanTheta * TanTheta) / (2.0 * SpeedMps * SpeedMps);
	}

	double FShotResolver::NetClearingSpeed(const FVec3& Origin, const FVec3& Target, double SpeedMps, double ClearanceM, double Gravity)
	{
		if (Gravity <= 0.0 || (Origin.X > 0.0) == (Target.X > 0.0))
		{
			return SpeedMps;
		}
		const double T = Origin.X / (Origin.X - Target.X);
		const double Need = Padel::Court::NetHeightAt(Origin.Y + T * (Target.Y - Origin.Y)) + ClearanceM;
		if (NetCrossingHeight(Origin, Target, SpeedMps, Gravity) >= Need)
		{
			return SpeedMps;
		}
		// The slowest ball that still reaches the target leaves at about 45 degrees and
		// crosses highest; between it and the asked speed, the arcs flatten as the speed
		// grows, so the fastest one that clears is a bisection away.
		const double D = LengthXY(Sub(Target, Origin));
		const double Drop = Target.Z - Origin.Z;
		double Lo = std::sqrt(Gravity * (Drop + std::sqrt(D * D + Drop * Drop))) * (1.0 + 1e-9);
		double Hi = SpeedMps;
		if (Lo >= Hi || NetCrossingHeight(Origin, Target, Lo, Gravity) < Need)
		{
			return Lo < Hi ? Lo : SpeedMps;
		}
		for (int Step = 0; Step < 40; ++Step)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(NetCrossingHeight(Origin, Target, Mid, Gravity) >= Need ? Lo : Hi) = Mid;
		}
		return Lo;
	}

	double FShotResolver::MainRating(const FShotContext& C, const FShotParams& Shot, const FShotModel& Model)
	{
		const double Mult = SideMult(C, Model);
		double S = C.Player->Unit(C.IsForehand() ? Shot.ForehandRating : Shot.BackhandRating, Mult);
		if (C.bReturn)
		{
			S = Lerp(S, C.Player->Unit(ERating::ReturnSkill, Mult), Model.ReturnWeight);
		}
		return S;
	}

	double FShotResolver::TimingRating(const FShotContext& C, const FShotParams& Shot, const FShotModel& Model)
	{
		if (C.bOffGlass)
		{
			return C.Player->Unit(ERating::WallPlay, SideMult(C, Model));
		}
		return MainRating(C, Shot, Model);
	}

	double FShotResolver::ScatterRadius(const FShotContext& C, const FShotParams& Shot, const FShotChoice& Choice,
		double MainRatingUnit, ETimingGrade Grade, const FShotModel& Model)
	{
		double Radius = Shot.BaseErrorM * (1.0 - Model.ScatterRatingReduction * MainRatingUnit);
		Radius *= FTimingGrader::ErrorMult(Grade, Model);
		Radius *= 1.0 + Model.PositionErrorPerM * (C.PositionDistanceM > 0.0 ? C.PositionDistanceM : 0.0);
		Radius *= PowerMult(C.Charge, Model);
		Radius *= PressureMult(C, Model);
		Radius *= RunningMult(C, Model);
		if (C.bOffGlass)
		{
			Radius *= Lerp(Model.WallScatterAt1, Model.WallScatterAt99, C.Player->Unit(ERating::WallPlay));
		}
		Radius *= Choice.ExtraErrorMult;
		Radius *= Lerp(Model.ConsistencyScatterAt1, Model.ConsistencyScatterAt99, C.Player->Unit(ERating::Consistency));
		return Radius;
	}

	double FShotResolver::PowerMult(double Charge, const FShotModel& Model)
	{
		// GDD 5.3: hitting hard is a risk above 80% charge.
		if (Charge <= Model.PowerMultStartCharge)
		{
			return 1.0;
		}
		const double T = Clamp(InverseLerp(Model.PowerMultStartCharge, 1.0, Charge), 0.0, 1.0);
		return Lerp(1.0, Model.PowerMultMax, T);
	}

	double FShotResolver::PressureMult(const FShotContext& C, const FShotModel& Model)
	{
		if (!C.bPressurePoint)
		{
			return 1.0;
		}
		return 1.0 + Model.PressureScale * (1.0 - C.Player->Unit(ERating::Composure));
	}

	double FShotResolver::RunningMult(const FShotContext& C, const FShotModel& Model)
	{
		// GDD 5.2: full speed through the shot costs accuracy unless split-stepped.
		if (C.bSplitStepped)
		{
			return 1.0;
		}
		return LengthXY(C.PlayerVelocity) >= C.TopSpeedMps * Model.RunningSpeedFraction ? Model.RunningErrorMult : 1.0;
	}

	double FShotResolver::PositionPowerLimit(double DistanceM, const FShotModel& Model)
	{
		const double Limit = 1.0 - Model.PositionPowerLossPerM * (DistanceM > 0.0 ? DistanceM : 0.0);
		return Limit > Model.PositionPowerMin ? Limit : Model.PositionPowerMin;
	}

	double FShotResolver::StaminaFactor(double Stamina, const FShotModel& Model)
	{
		// GDD 5.3: 1.0 above 50% stamina, falling linearly to 0.85 at 0%.
		if (Stamina >= Model.StaminaFullAbove)
		{
			return 1.0;
		}
		return Lerp(Model.StaminaFactorAtZero, 1.0, Clamp(Stamina / Model.StaminaFullAbove, 0.0, 1.0));
	}

	FShotIntent FShotResolver::Mishit(const FShotContext& C, const FShotModel& Model, FPadelRng& Rng)
	{
		FShotIntent Intent = BaseIntent(C, EShotType::Mishit, ETimingGrade::Miss);
		Intent.SpeedMps = Rng.Range(Model.MishitSpeedMinKmh, Model.MishitSpeedMaxKmh) / 3.6;
		const double Depth = Rng.Range(0.0, Model.MishitDepthMaxM);
		const double Across = Rng.Range(-Model.MishitLateralMaxM, Model.MishitLateralMaxM);
		Intent.Target = Vec(-C.TeamSign * Depth, Across, 0.0);
		Intent.AimedTarget = Intent.Target;
		if (Rng.NextDouble() < Model.MishitPopupChance)
		{
			Intent.ApexHeightM = Rng.Range(Model.MishitPopupApexMinM, Model.MishitPopupApexMaxM);
		}
		return Intent;
	}
}
