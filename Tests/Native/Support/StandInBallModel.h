// TEST-ONLY stand-in for the engine's ball sim (Agent 2), so the AI's prediction and
// the soak run can be tested before the real sim lands. Gravity and simple bounces on
// the real court geometry (CourtSpec.h); no spin, no air drag. The numbers are rough
// GDD 2 surface values, kept here because this file is deleted once the AI runs on
// the real sim (see the PR and docs/decisions/0003-ai-in-padelcore.md).
#pragma once

#include "AI/AiBallModel.h"
#include "AI/AiRandom.h"
#include "CourtSpec.h"

#include <cmath>

namespace Padel::AI::Test
{
	class FStandInBallModel : public IAiBallModel
	{
	public:
		static constexpr double Gravity = 9.81;
		static constexpr double Radius = 0.0325;

		std::vector<FAiContact> Step(FAiBall& Ball, double Dt) const override
		{
			return StepWith(Ball, Dt, nullptr);
		}

		bool IsDead(const FAiBall& Ball) const override
		{
			const FAiVec& P = Ball.Position;
			if (std::abs(P.X) > Court::HalfLength + 0.3 || std::abs(P.Y) > Court::HalfWidth + 0.3 || P.Z < -0.5)
			{
				return true;
			}
			const FAiVec& V = Ball.Velocity;
			return P.Z <= Radius + 1e-3 && std::abs(V.Z) < 0.3 && V.Length2D() < 0.3;
		}

		// Rng adds the mesh's randomness (GDD 2: +-15 degrees, +-20% speed) in live play.
		std::vector<FAiContact> StepWith(FAiBall& Ball, double Dt, FPadelRng* Rng) const
		{
			using namespace Court;
			std::vector<FAiContact> Out;
			FAiVec& P = Ball.Position;
			FAiVec& V = Ball.Velocity;
			const double PrevX = P.X;
			V.Z -= Gravity * Dt;
			P += V * Dt;

			// Net: crossing X = 0 below the tape.
			if ((PrevX >= 0.0) != (P.X >= 0.0) && std::abs(P.Y) <= HalfWidth && P.Z < NetHeightAt(P.Y))
			{
				P.X = PrevX >= 0.0 ? 0.04 : -0.04;
				V = { -V.X * 0.1, V.Y * 0.3, V.Z * 0.5 };
				Out.push_back({ ESurface::Net, P });
			}
			if (P.Z < Radius && V.Z < 0.0)
			{
				P.Z = Radius;
				V = { V.X * 0.8, V.Y * 0.8, -V.Z * 0.72 };
				Out.push_back({ ESurface::Ground, P });
			}
			if (P.Z > CeilingHeight - Radius && V.Z > 0.0)
			{
				V.Z = -V.Z * 0.5;
				Out.push_back({ ESurface::Ceiling, P });
			}
			if (std::abs(P.X) > HalfLength - Radius && V.X * P.X > 0.0 && std::abs(P.Y) <= HalfWidth)
			{
				const ESurface S = BackWallSurfaceAt(P.Z);
				if (S != ESurface::None)
				{
					P.X = (HalfLength - Radius) * (P.X > 0.0 ? 1.0 : -1.0);
					Bounce(V.X, V.Y, V.Z, S, Rng);
					Out.push_back({ S, P });
				}
			}
			if (std::abs(P.Y) > HalfWidth - Radius && V.Y * P.Y > 0.0 && std::abs(P.X) <= HalfLength)
			{
				const ESurface S = SideWallSurfaceAt(P.X, P.Z);
				if (S != ESurface::None)
				{
					P.Y = (HalfWidth - Radius) * (P.Y > 0.0 ? 1.0 : -1.0);
					Bounce(V.Y, V.X, V.Z, S, Rng);
					Out.push_back({ S, P });
				}
			}
			return Out;
		}

	private:
		static void Bounce(double& Normal, double& TangentA, double& TangentB, Court::ESurface S, FPadelRng* Rng)
		{
			const bool bMesh = S == Court::ESurface::Mesh;
			Normal = -Normal * (bMesh ? 0.30 : 0.68);
			const double Keep = bMesh ? 0.5 : 0.9;
			TangentA *= Keep;
			TangentB *= Keep;
			if (bMesh && Rng != nullptr)
			{
				TangentA += Rng->Range(-0.27, 0.27) * std::abs(Normal);
				const double Speed = Rng->Range(0.8, 1.2);
				Normal *= Speed;
				TangentA *= Speed;
				TangentB *= Speed;
			}
		}
	};
}
