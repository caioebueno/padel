#include "BallSim.h"

#include <algorithm>
#include <cmath>

namespace Padel
{
	using namespace Court;

	namespace
	{
		FBallContact MakeContact(EContactKind Kind, ESurface Surface, const FBallState& State, const FVec3& Normal, double Time)
		{
			FBallContact C;
			C.Kind = Kind;
			C.Surface = Surface;
			C.Position = State.Position;
			C.Normal = Normal;
			C.Half = HalfOf(State.Position.X);
			C.bInCourt = State.bInCourt;
			C.Time = Time;
			return C;
		}

		double Sign(double Value)
		{
			return Value < 0.0 ? -1.0 : 1.0;
		}
	}

	FVec3 FBallSim::Acceleration(const FVec3& Velocity, const FVec3& Spin) const
	{
		const FVec3 Gravity{ 0.0, 0.0, -Params.Gravity };
		const FVec3 Drag = Velocity * (-Params.DragCoefficient * Size(Velocity));
		const FVec3 Magnus = Cross(Spin, Velocity) * Params.MagnusCoefficient;
		return Gravity + Drag + Magnus;
	}

	void FBallSim::FlightSubStep(FBallState& State, double H) const
	{
		// Semi-implicit Euler: stable for bouncing and cheap; energy error is far below
		// what restitution removes at each contact.
		State.Velocity += Acceleration(State.Velocity, State.Spin) * H;
		State.Position += State.Velocity * H;
		State.Spin *= std::max(0.0, 1.0 - Params.SpinDecayPerSecond * H);
	}

	void FBallSim::Step(FBallState& State, double Dt, FPadelRng* Rng, std::vector<FBallContact>& Out) const
	{
		const double Nominal = SubStepSeconds();
		const int Count = std::max(1, static_cast<int>(std::ceil(Dt / Nominal - 1e-9)));
		const double H = Dt / Count;
		for (int Index = 0; Index < Count; ++Index)
		{
			SubStep(State, H, H * (Index + 1), Rng, Out);
		}
	}

	void FBallSim::SubStep(FBallState& State, double H, double TimeOffset, FPadelRng* Rng, std::vector<FBallContact>& Out) const
	{
		const FVec3 Previous = State.Position;
		if (State.bRolling)
		{
			FVec3 V{ State.Velocity.X, State.Velocity.Y, 0.0 };
			const double Speed = Size(V);
			const double Loss = Params.RollingDeceleration * H;
			V = Speed <= Loss ? FVec3{} : V * ((Speed - Loss) / Speed);
			State.Velocity = V;
			State.Position += V * H;
			State.Position.Z = BallRadius;
			// Rolling without slipping: the spin follows the motion.
			State.Spin = Cross(UpVector, V) / BallRadius;
		}
		else
		{
			FlightSubStep(State, H);
		}

		if (State.bInCourt)
		{
			CollideNet(State, Previous, TimeOffset, Out);
			const bool bWasOnA = Previous.X >= 0.0;
			const bool bIsOnA = State.Position.X >= 0.0;
			if (bWasOnA != bIsOnA && std::abs(State.Position.Y) <= HalfWidth)
			{
				Out.push_back(MakeContact(EContactKind::CrossedNet, ESurface::None, State, {}, TimeOffset));
			}
			CollideWalls(State, TimeOffset, Rng, Out);
		}

		// The ceiling covers the whole hall (GDD 2: hitting it is a fault).
		if (State.Position.Z > CeilingHeight - BallRadius && State.Velocity.Z > 0.0)
		{
			State.Position.Z = CeilingHeight - BallRadius;
			const FVec3 Normal{ 0.0, 0.0, -1.0 };
			Bounce(State, Normal, Params.Ceiling, Rng);
			Out.push_back(MakeContact(EContactKind::Surface, ESurface::Ceiling, State, Normal, TimeOffset));
		}

		// The court floor and the run-off are both ground; bInCourt tells them apart.
		if (!State.bRolling && State.Position.Z < BallRadius && State.Velocity.Z < 0.0)
		{
			State.Position.Z = BallRadius;
			Bounce(State, UpVector, Params.Ground, Rng);
			Out.push_back(MakeContact(EContactKind::Surface, ESurface::Ground, State, UpVector, TimeOffset));
			if (State.Velocity.Z < Params.RollingThreshold)
			{
				State.Velocity.Z = 0.0;
				State.bRolling = true;
				FBallContact Roll = MakeContact(EContactKind::Surface, ESurface::Ground, State, UpVector, TimeOffset);
				Roll.bRolling = true;
				Out.push_back(Roll);
			}
		}
	}

	void FBallSim::CollideNet(FBallState& State, const FVec3& Previous, double TimeOffset, std::vector<FBallContact>& Out) const
	{
		FVec3& P = State.Position;
		if (std::abs(P.X) >= BallRadius)
		{
			State.bTouchingNet = false;
		}
		// The side the ball approaches from. A ball starting on the plane is ignored.
		if (Previous.X == 0.0)
		{
			return;
		}
		const double Side = Sign(Previous.X);
		const bool bApproaching = Side * State.Velocity.X < 0.0;
		const double Before = Side * Previous.X - BallRadius;
		const double After = Side * P.X - BallRadius;
		if (!bApproaching || State.bTouchingNet || Before < 0.0 || After >= 0.0)
		{
			return;
		}
		// Swept test: a fast ball moves more than its diameter per sub-step.
		const double T = std::clamp(Before / (Before - After), 0.0, 1.0);
		const FVec3 Hit = Previous + (P - Previous) * T;
		if (std::abs(Hit.Y) > HalfWidth)
		{
			return;
		}
		const double Tape = NetHeightAt(Hit.Y);
		if (Hit.Z < Tape)
		{
			// Into the net: it absorbs most of the energy and drops the ball on this side.
			P = Hit;
			P.X = Side * BallRadius;
			const FSurfaceParams& Net = Params.Net;
			State.Velocity.X = -State.Velocity.X * Net.Restitution;
			State.Velocity.Y *= 1.0 - Net.Friction;
			State.Velocity.Z *= 1.0 - Net.Friction;
			State.Spin *= 1.0 - Net.Friction;
			Out.push_back(MakeContact(EContactKind::Surface, ESurface::Net, State, { Side, 0.0, 0.0 }, TimeOffset));
		}
		else if (Hit.Z < Tape + BallRadius)
		{
			// Clipped the tape: slowed, carries on (GDD 2: it can roll over).
			State.Velocity *= Params.NetTapeSpeedKeep;
			State.bTouchingNet = true;
			FBallState AtTape = State;
			AtTape.Position = Hit;
			Out.push_back(MakeContact(EContactKind::Surface, ESurface::Net, AtTape, { 0.0, 0.0, 1.0 }, TimeOffset));
		}
	}

	void FBallSim::CollideWalls(FBallState& State, double TimeOffset, FPadelRng* Rng, std::vector<FBallContact>& Out) const
	{
		FVec3& P = State.Position;
		// Back walls.
		if (std::abs(P.X) > HalfLength - BallRadius && Sign(P.X) * State.Velocity.X > 0.0)
		{
			const double Side = Sign(P.X);
			const ESurface Surface = std::abs(P.Y) <= HalfWidth ? BackWallSurfaceAt(P.Z) : ESurface::None;
			if (Surface != ESurface::None)
			{
				P.X = Side * (HalfLength - BallRadius);
				const FVec3 Normal{ -Side, 0.0, 0.0 };
				Bounce(State, Normal, Params.Surface(Surface), Rng);
				Out.push_back(MakeContact(EContactKind::Surface, Surface, State, Normal, TimeOffset));
			}
		}
		// Side walls.
		if (std::abs(P.Y) > HalfWidth - BallRadius && Sign(P.Y) * State.Velocity.Y > 0.0)
		{
			const double Side = Sign(P.Y);
			const ESurface Surface = SideWallSurfaceAt(P.X, P.Z);
			if (Surface != ESurface::None)
			{
				P.Y = Side * (HalfWidth - BallRadius);
				const FVec3 Normal{ 0.0, -Side, 0.0 };
				Bounce(State, Normal, Params.Surface(Surface), Rng);
				Out.push_back(MakeContact(EContactKind::Surface, Surface, State, Normal, TimeOffset));
			}
		}
		// Fully past a wall plane: over the top, through an exit, or around the end.
		if (std::abs(P.X) > HalfLength + BallRadius || std::abs(P.Y) > HalfWidth + BallRadius)
		{
			State.bInCourt = false;
			Out.push_back(MakeContact(EContactKind::LeftCourt, ESurface::None, State, {}, TimeOffset));
		}
	}

	void FBallSim::Bounce(FBallState& State, const FVec3& Normal, const FSurfaceParams& Surface, FPadelRng* Rng) const
	{
		FVec3 V = State.Velocity;
		const double Vn = Dot(V, Normal);
		if (Vn >= 0.0)
		{
			return;
		}
		FVec3 Vt = V - Normal * Vn;
		FVec3 W = State.Spin;

		// Friction acts on the slip of the contact point (velocity plus spin × offset),
		// capped at the impulse that stops the slip. This is what gives the topspin kick,
		// the slice skid on the grass, and the glass keeping its spin (GDD 2).
		const FVec3 ContactOffset = Normal * -BallRadius;
		FVec3 Slip = Vt + Cross(W, ContactOffset);
		Slip -= Normal * Dot(Slip, Normal);
		const double SlipSpeed = Size(Slip);
		const double NormalImpulse = (1.0 + Surface.Restitution) * -Vn;
		if (SlipSpeed > 1e-9)
		{
			const FVec3 U = Slip / SlipSpeed;
			const double K = Params.InertiaFactor;
			const double Stop = SlipSpeed / (1.0 + 1.0 / K);
			const double D = std::min(Surface.Friction * NormalImpulse, Stop);
			Vt -= U * D;
			W += Cross(Normal, U) * (D / (K * BallRadius));
		}
		FVec3 Out = Vt + Normal * (-Vn * Surface.Restitution);

		// GDD 2: the mesh is dead and slightly random.
		if (Rng && (Surface.DeflectionMax > 0.0 || Surface.SpeedVariation > 0.0))
		{
			const double Speed = Size(Out);
			if (Speed > 1e-9)
			{
				const FVec3 Dir = Out / Speed;
				Out = RotateAround(Out, Normal, Rng->Range(-Surface.DeflectionMax, Surface.DeflectionMax));
				const FVec3 Tilt = SafeNormal(Cross(Normal, Dir), SafeNormal(Cross(Normal, FVec3{ 0.0, 0.0, 1.0 }), FVec3{ 0.0, 1.0, 0.0 }));
				Out = RotateAround(Out, Tilt, Rng->Range(-Surface.DeflectionMax, Surface.DeflectionMax));
				Out *= 1.0 + Rng->Range(-Surface.SpeedVariation, Surface.SpeedVariation);
				const double OutN = Dot(Out, Normal);
				if (OutN < 0.0)
				{
					Out -= Normal * (2.0 * OutN);
				}
			}
		}
		State.Velocity = Out;
		State.Spin = W;
	}

	std::vector<FBallSample> FBallSim::Predict(const FBallState& State, double Horizon, double SampleDt,
		std::vector<FBallContact>* OutContacts) const
	{
		std::vector<FBallSample> Samples;
		FBallState Copy = State;
		const int SampleEvery = std::max(1, static_cast<int>(std::lround(SampleDt / SimTickSeconds)));
		const int Ticks = std::max(0, static_cast<int>(std::lround(Horizon / SimTickSeconds)));
		Samples.reserve(static_cast<size_t>(Ticks / SampleEvery + 2));
		int Bounces = 0;
		bool bOffGlass = false;
		Samples.push_back({ 0.0, Copy.Position, Copy.Velocity, 0, false });
		std::vector<FBallContact> Contacts;
		for (int TickIndex = 1; TickIndex <= Ticks; ++TickIndex)
		{
			Contacts.clear();
			Step(Copy, SimTickSeconds, nullptr, Contacts);
			const double TickStart = (TickIndex - 1) * SimTickSeconds;
			for (FBallContact& C : Contacts)
			{
				C.Time += TickStart;
				if (C.Kind == EContactKind::Surface && C.Surface == ESurface::Ground && !C.bRolling)
				{
					++Bounces;
				}
				if (C.Kind == EContactKind::Surface && C.Surface == ESurface::Glass)
				{
					bOffGlass = true;
				}
				if (OutContacts)
				{
					OutContacts->push_back(C);
				}
			}
			if (TickIndex % SampleEvery == 0)
			{
				Samples.push_back({ TickIndex * SimTickSeconds, Copy.Position, Copy.Velocity, Bounces, bOffGlass });
			}
		}
		return Samples;
	}
}
