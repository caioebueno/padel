#include "AI/AiBallPredictor.h"
#include "PadelMath.h"

#include <cmath>

namespace Padel::AI
{
namespace
{
	// The wall nearest the contact: a corner ball counts for the wall it is closer to.
	EGlassWall WallAt(const FAiVec& Position)
	{
		const double ToBack = Court::HalfLength - std::abs(Position.X);
		const double ToSide = Court::HalfWidth - std::abs(Position.Y);
		if (ToBack <= ToSide)
		{
			return EGlassWall::Back;
		}
		return Position.Y > 0.0 ? EGlassWall::SidePlusY : EGlassWall::SideMinusY;
	}
}

FAiBallPredictor::FAiBallPredictor(const IAiBallModel& InModel, const FAiTactics& InTactics)
	: Model(InModel)
	, Tactics(InTactics)
{
}

std::vector<FTrajectorySample> FAiBallPredictor::Predict(const FAiBall& Start, double NoiseM, FPadelRng& Rng) const
{
	std::vector<FTrajectorySample> Samples;
	const double Dt = Tactics.PredictDtS;
	if (Dt <= 0.0)
	{
		return Samples;
	}
	const int Steps = static_cast<int>(std::ceil(Tactics.PredictHorizonS / Dt));
	const int Every = Tactics.SampleEverySteps > 0 ? Tactics.SampleEverySteps : 1;

	FAiBall Ball = Start;
	int Half = static_cast<int>(Court::HalfOf(Ball.Position.X));
	int Bounces = 0;
	bool bOffGlass = false;
	EGlassWall GlassWall = EGlassWall::None;
	bool bNoiseDrawn = false;
	FAiVec Offset;
	Samples.reserve(static_cast<size_t>(Steps / Every + 8));

	for (int Step = 1; Step <= Steps; ++Step)
	{
		const std::vector<FAiContact> Contacts = Model.Step(Ball, Dt);
		const int NewHalf = static_cast<int>(Court::HalfOf(Ball.Position.X));
		if (NewHalf != Half)
		{
			Half = NewHalf;
			Bounces = 0;
			bOffGlass = false;
			GlassWall = EGlassWall::None;
		}
		for (const FAiContact& Contact : Contacts)
		{
			if (Contact.Surface == Court::ESurface::Ground)
			{
				++Bounces;
			}
			else if (Contact.Surface == Court::ESurface::Glass || Contact.Surface == Court::ESurface::Mesh)
			{
				if (!bNoiseDrawn && NoiseM > 0.0)
				{
					const double Angle = Rng.Range(0.0, 6.283185307179586);
					const double Radius = NoiseM * std::sqrt(Rng.NextDouble());
					Offset = { Padel::Math::Cos(Angle) * Radius, Padel::Math::Sin(Angle) * Radius, 0.0 };
				}
				bNoiseDrawn = true;
				if (Bounces >= 1)
				{
					bOffGlass = true;
					GlassWall = WallAt(Contact.Position);
				}
			}
		}
		const bool bDead = Bounces >= 2 || Model.IsDead(Ball);
		if (Step % Every == 0 || !Contacts.empty() || bDead)
		{
			FTrajectorySample Sample;
			Sample.T = Step * Dt;
			Sample.Position = Ball.Position + Offset;
			Sample.Velocity = Ball.Velocity;
			Sample.Half = Half;
			Sample.GroundBounces = Bounces;
			Sample.bOffGlass = bOffGlass;
			Sample.GlassWall = GlassWall;
			Sample.bContact = !Contacts.empty();
			Samples.push_back(Sample);
		}
		if (bDead)
		{
			break;
		}
	}
	return Samples;
}
}
