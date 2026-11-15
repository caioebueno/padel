#include "Shots/PracticeBall.h"

#include "CourtSpec.h"

#include <cmath>

namespace Padel::Shots
{
	FBallView FPracticeBall::Current() const
	{
		FBallView View;
		View.Position = Position;
		View.Velocity = Velocity;
		View.GroundBounces = GroundBounces;
		View.bOffGlass = bOffGlass;
		View.bLive = bLive;
		return View;
	}

	std::vector<FBallView> FPracticeBall::Predict(double HorizonS, double StepS) const
	{
		std::vector<FBallView> Out;
		if (!bLive)
		{
			return Out;
		}
		FPracticeBall Copy = *this;
		Copy.Accumulator = 0.0;
		const int Count = static_cast<int>(HorizonS / StepS);
		Out.reserve(static_cast<std::size_t>(Count));
		for (int Index = 1; Index <= Count && Copy.bLive; ++Index)
		{
			Copy.Advance(StepS);
			FBallView View = Copy.Current();
			View.T = Index * StepS;
			Out.push_back(View);
		}
		return Out;
	}

	FVec3 FPracticeBall::SolveLaunch(const FVec3& Origin, const FVec3& Target, double SpeedMps, double ApexM, double Gravity)
	{
		const FVec3 Flat = Vec(Target.X - Origin.X, Target.Y - Origin.Y, 0.0);
		const double D = LengthXY(Flat);
		const FVec3 Dir = D > 1e-6 ? Scale(Flat, 1.0 / D) : Vec(1.0, 0.0, 0.0);
		const double Drop = Target.Z - Origin.Z;
		if (ApexM > 0.0)
		{
			const double Apex = ApexM > Origin.Z ? ApexM : Origin.Z + 0.1;
			const double Vz = std::sqrt(2.0 * Gravity * (Apex - Origin.Z));
			const double TimeUp = Vz / Gravity;
			const double TimeDown = std::sqrt(2.0 * (Apex - Target.Z) / Gravity);
			return Add(Scale(Dir, D / (TimeUp + TimeDown)), Vec(0.0, 0.0, Vz));
		}
		// d tan(t) - g d^2 / (2 v^2 cos^2 t) = drop, as a quadratic in tan(t); the smaller
		// root is the flatter arc.
		const double A = Gravity * D * D / (2.0 * SpeedMps * SpeedMps);
		const double Disc = D * D - 4.0 * A * (Drop + A);
		const double TanTheta = Disc >= 0.0 && A > 0.0 ? (D - std::sqrt(Disc)) / (2.0 * A) : 1.0;
		const double CosTheta = 1.0 / std::sqrt(1.0 + TanTheta * TanTheta);
		return Add(Scale(Dir, SpeedMps * CosTheta), Vec(0.0, 0.0, SpeedMps * CosTheta * TanTheta));
	}

	void FPracticeBall::ApplyShot(const FShotIntent& Intent)
	{
		Position = Intent.Origin;
		Velocity = SolveLaunch(Intent.Origin, Intent.Target, Intent.SpeedMps, Intent.ApexHeightM, Params->GravityMps2);
		// Players 0 and 1 are Team A (+X), 2 and 3 Team B.
		LastHitter = Intent.PlayerId < 2 ? 1 : -1;
		GroundBounces = 0;
		bOffGlass = false;
		bLive = true;
		bEverLaunched = true;
		DeadForS = 0.0;
		++HitCount;
	}

	void FPracticeBall::ServeToss(const FVec3& At, int TeamSign)
	{
		(void)TeamSign;
		Position = At;
		Velocity = FVec3{};
		LastHitter = 0;
		GroundBounces = 0;
		bOffGlass = false;
		bLive = true;
		bEverLaunched = true;
		DeadForS = 0.0;
	}

	void FPracticeBall::Feed(const FVec3& From, const FVec3& Target, double SpeedMps, double ApexM, int FromTeam)
	{
		Position = From;
		Velocity = SolveLaunch(From, Target, SpeedMps, ApexM, Params->GravityMps2);
		LastHitter = FromTeam;
		GroundBounces = 0;
		bOffGlass = false;
		bLive = true;
		bEverLaunched = true;
		DeadForS = 0.0;
	}

	void FPracticeBall::Advance(double Dt)
	{
		Accumulator += Dt;
		while (Accumulator >= Params->StepS - 1e-12)
		{
			Accumulator -= Params->StepS;
			SubStep(Params->StepS);
		}
	}

	void FPracticeBall::SubStep(double Dt)
	{
		if (!bLive)
		{
			DeadForS += Dt;
		}
		const double R = Params->RadiusM;
		const double PreviousX = Position.X;
		Velocity.Z -= Params->GravityMps2 * Dt;
		Position = Add(Position, Scale(Velocity, Dt));

		// Net: crossing the plane below the tape stops the ball on that side.
		if ((PreviousX > 0.0) != (Position.X > 0.0) && std::abs(Position.Y) <= Court::HalfWidth)
		{
			if (Position.Z - R < Court::NetHeightAt(Position.Y))
			{
				Position.X = PreviousX > 0.0 ? R : -R;
				Velocity.X = -Velocity.X * Params->NetRestitution;
				Velocity.Y *= Params->NetRestitution;
				Kill();
			}
			else
			{
				GroundBounces = 0;
				bOffGlass = false;
			}
		}

		if (Position.Z > Court::CeilingHeight)
		{
			Kill();
		}

		// Back walls.
		if (std::abs(Position.X) > Court::HalfLength - R)
		{
			const Court::ESurface Surface = Court::BackWallSurfaceAt(Position.Z);
			if (Surface == Court::ESurface::None)
			{
				Kill();
			}
			else
			{
				const bool bGlass = Surface == Court::ESurface::Glass;
				Position.X = Sign(Position.X) * (Court::HalfLength - R);
				Velocity.X = -Velocity.X * (bGlass ? Params->GlassRestitution : Params->MeshRestitution);
				const double Keep = bGlass ? Params->GlassTangentKeep : Params->MeshTangentKeep;
				Velocity.Y *= Keep;
				Velocity.Z *= Keep;
				if (GroundBounces >= 1)
				{
					bOffGlass = true;
				}
			}
		}

		// Side walls.
		if (std::abs(Position.Y) > Court::HalfWidth - R && std::abs(Position.X) <= Court::HalfLength)
		{
			const Court::ESurface Surface = Court::SideWallSurfaceAt(Position.X, Position.Z);
			if (Surface == Court::ESurface::None)
			{
				if (std::abs(Position.Y) > Court::HalfWidth + R)
				{
					Kill();
				}
			}
			else
			{
				const bool bGlass = Surface == Court::ESurface::Glass;
				Position.Y = Sign(Position.Y) * (Court::HalfWidth - R);
				Velocity.Y = -Velocity.Y * (bGlass ? Params->GlassRestitution : Params->MeshRestitution);
				const double Keep = bGlass ? Params->GlassTangentKeep : Params->MeshTangentKeep;
				Velocity.X *= Keep;
				Velocity.Z *= Keep;
				if (GroundBounces >= 1)
				{
					bOffGlass = true;
				}
			}
		}

		// Ground.
		if (Position.Z < R && Velocity.Z < 0.0)
		{
			Position.Z = R;
			if (!Court::IsInsideCourt(Position.X, Position.Y))
			{
				Kill();
			}
			Velocity.Z = -Velocity.Z * Params->GroundRestitution;
			Velocity.X *= Params->GroundTangentKeep;
			Velocity.Y *= Params->GroundTangentKeep;
			if (bLive)
			{
				++GroundBounces;
				// GDD 3.3: the second bounce ends the ball for this practice.
				if (GroundBounces >= 2)
				{
					Kill();
				}
			}
		}
	}
}
