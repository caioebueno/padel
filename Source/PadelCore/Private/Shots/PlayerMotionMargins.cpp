// FPlayerMotion at the glass and the net (GDD 6.3, BIOMECHANICS_MOVEMENT 2.6, 5.7): braking
// into the margins of the player's own half, and the hard stop behind it. Split from
// PlayerMotion.cpp to keep both files a readable size.

#include "Shots/PlayerMotion.h"

#include "CourtSpec.h"
#include "Shots/AccelRamp.h"

#include <algorithm>
#include <cmath>

namespace Padel::Shots
{
	void FPlayerMotion::LimitAtMargins(FVec3& Desired, double Brake, double Dt) const
	{
		// GDD 6.3 (BIOMECHANICS_MOVEMENT 2.6): a player arriving at the glass or the net
		// slows over the last steps instead of stopping dead. Toward a margin, ask for no
		// more speed than the player can still brake away inside the room left. The limit is
		// the stepped one, not sqrt(2 a d): speed falls by Brake * Dt a tick and each tick
		// moves the player its new speed * Dt, so k ticks of braking from k * Brake * Dt
		// cover Brake * Dt^2 * k (k + 1) / 2.
		if (Brake <= 0.0 || Dt <= 0.0)
		{
			return;
		}
		double Lo = 0.0;
		double Hi = 0.0;
		double Side = 0.0;
		OwnHalfLimits(Lo, Hi, Side);
		const double Step = Brake * Dt;
		const auto Limit = [Step, Dt](double& Component, double Pos, double Min, double Max)
		{
			if (Component > 0.0)
			{
				Component = std::min(Component, MarginSpeed(Max - Pos, Step, Dt));
			}
			else if (Component < 0.0)
			{
				Component = std::max(Component, -MarginSpeed(Pos - Min, Step, Dt));
			}
		};
		Limit(Desired.X, Position.X, Lo, Hi);
		Limit(Desired.Y, Position.Y, -Side, Side);
	}

	namespace
	{
		// RampMarginBraking: within this of a margin a slow creep finishes on the line.
		constexpr double CreepRoomM = 0.01;
	}

	double FPlayerMotion::AxisBrake(double Brake, double AxisSpeed) const
	{
		// Braking to a stop runs along the velocity, so an axis gets its share of the brake;
		// on one axis alone RampMarginBraking holds the other's speed and it gets all of it,
		// so the share is the safe side.
		const double Speed = LengthXY(Velocity);
		return Speed > 0.0 ? Brake * std::max(0.2, std::abs(AxisSpeed) / Speed) : Brake;
	}

	void FPlayerMotion::RampMarginBraking(FVec3& Desired, double Brake, double JerkMax, double Dt)
	{
		// GDD 6.3 (BIOMECHANICS_MOVEMENT 2.6, 5.7): a player arriving at the glass or the net
		// slows over the last steps instead of stopping dead, and the brake takes AccelRiseS
		// to build. So brake once the stop, stepped from the push under way now, would end
		// within a tick's travel of the margin. Braking holds until that axis is stopped or
		// the stick turns away, so it doesn't chatter on and off at the line.
		double Lo = 0.0;
		double Hi = 0.0;
		double Side = 0.0;
		OwnHalfLimits(Lo, Hi, Side);
		const auto Axis = [this, Brake, JerkMax, Dt](bool& bLatch, double& Want, double V, double A, double Pos, double Min, double Max)
		{
			// Standing, the way the stick points is the way the player is about to go.
			const double Way = V != 0.0 ? V : Want;
			const double Sign = Way > 0.0 ? 1.0 : (Way < 0.0 ? -1.0 : 0.0);
			if (Sign == 0.0 || Want * Sign <= 0.0)
			{
				bLatch = false;
				return;
			}
			const double Room = Sign > 0.0 ? Max - Pos : Pos - Min;
			if (std::abs(V) <= Tuning->EdgeStopToleranceMps)
			{
				// A walking creep needs no planned stop (and a planned stop that ended short
				// lets go here, so the player can creep the rest of the way). The last
				// centimetre eases onto the line, closing the gap a little each tick: the ramp
				// settles exactly on so slow a wanted speed.
				bLatch = false;
				if (Room <= CreepRoomM)
				{
					// The last hair (a tick at the settled speed) is closed in one tick.
					const double Gap = std::max(0.0, Room);
					const double Ease = Gap <= AccelRamp::SettledMps * Dt ? Gap / Dt : Gap / std::max(Tuning->AccelRiseS, Dt);
					Want = Sign * std::min(Want * Sign, Ease);
					return;
				}
			}
			else if (!bLatch)
			{
				// Two more ticks of the push building at the jerk limit come first: a stop
				// planned from now would only start after them.
				double Ahead = 0.0;
				double V1 = V * Sign;
				double A1 = A * Sign;
				for (int Tick = 0; Tick < 2; ++Tick)
				{
					A1 = std::min(A1 + JerkMax * Dt, JerkMax * Tuning->AccelRiseS);
					V1 = std::max(0.0, V1 + A1 * Dt);
					Ahead += V1 * Dt;
				}
				bLatch = Ahead + AccelRamp::StopDistance(V1, A1, AxisBrake(Brake, V), Tuning->AccelRiseS, JerkMax, Dt) >= Room;
			}
			// No faster than a ramped stop can still shed in the room left: the speed falls
			// along that curve to the line, so the approach eases in rather than stopping
			// short and starting again. Once braking, no more push either.
			const double Cap = AccelRamp::StopSpeed(Room, AxisBrake(Brake, V), Tuning->AccelRiseS);
			Want = Sign * std::min(Want * Sign, bLatch ? std::min(Cap, std::abs(V)) : Cap);
		};
		Axis(bMarginBrakeX, Desired.X, Velocity.X, Ramp.AX, Position.X, Lo, Hi);
		Axis(bMarginBrakeY, Desired.Y, Velocity.Y, Ramp.AY, Position.Y, -Side, Side);
		// Braking into a margin comes first: the other axis may slow down but not speed up
		// or push the other way, so it never takes more of the brake than its share of the
		// velocity (the share AxisBrake planned with).
		const auto Hold = [](double& Want, double V)
		{
			Want = Want * V > 0.0 ? (V > 0.0 ? std::min(Want, V) : std::max(Want, V)) : 0.0;
		};
		if (bMarginBrakeX && !bMarginBrakeY)
		{
			Hold(Desired.Y, Velocity.Y);
		}
		else if (bMarginBrakeY && !bMarginBrakeX)
		{
			Hold(Desired.X, Velocity.X);
		}
	}

	double FPlayerMotion::MarginSpeed(double Room, double Step, double Dt)
	{
		// Whole ticks, so each one sheds exactly Step = Brake * Dt; inside the last tick, just
		// the room that is left.
		const double Ticks = std::floor(0.5 * (std::sqrt(1.0 + 8.0 * std::max(0.0, Room) / (Step * Dt)) - 1.0));
		return Ticks >= 1.0 ? Ticks * Step : std::max(0.0, Room) / Dt;
	}

	void FPlayerMotion::MarginBraking(double Brake, double JerkMax, double Dt, bool& bX, bool& bY) const
	{
		// An axis on which the player is moving toward a margin and is at (or within a tick
		// of) the most speed they can still brake away there: its braking has to happen now.
		bX = false;
		bY = false;
		if (Brake <= 0.0 || Dt <= 0.0)
		{
			return;
		}
		double Lo = 0.0;
		double Hi = 0.0;
		double Side = 0.0;
		OwnHalfLimits(Lo, Hi, Side);
		const double Step = Brake * Dt;
		// With AccelRiseS this is the safety net behind RampMarginBraking: it takes over only
		// once the ramped stop, stepped from now, would run past the margin itself.
		const auto Binding = [this, Step, Brake, JerkMax, Dt](double V, double A, double Pos, double Min, double Max)
		{
			const double Room = V > 0.0 ? Max - Pos : Pos - Min;
			if (V == 0.0)
			{
				return false;
			}
			if (JerkMax > 0.0)
			{
				const double Sign = V > 0.0 ? 1.0 : -1.0;
				return AccelRamp::StopDistance(V * Sign, A * Sign, AxisBrake(Brake, V), Tuning->AccelRiseS, JerkMax, Dt) > Room;
			}
			return std::abs(V) + Step >= MarginSpeed(Room, Step, Dt);
		};
		bX = Binding(Velocity.X, Ramp.AX, Position.X, Lo, Hi);
		bY = Binding(Velocity.Y, Ramp.AY, Position.Y, -Side, Side);
	}

	void FPlayerMotion::OwnHalfLimits(double& Lo, double& Hi, double& Side) const
	{
		const double Near = Tuning->NetMarginM;
		const double Far = Court::HalfLength - Tuning->WallMarginM;
		Side = Court::HalfWidth - Tuning->WallMarginM;
		Lo = TeamSign > 0.0 ? Near : -Far;
		Hi = TeamSign > 0.0 ? Far : -Near;
	}

	void FPlayerMotion::ClampToOwnHalf()
	{
		// Players stay on their own half; crossing the net is a referee matter (GDD 3.3).
		// LimitAtMargins (RampMarginBraking with AccelRiseS) brakes the player into these
		// limits; this is the hard stop behind it.
		double Lo = 0.0;
		double Hi = 0.0;
		double Side = 0.0;
		OwnHalfLimits(Lo, Hi, Side);
		const FVec3 Before = Velocity;
		if (Position.X < Lo || Position.X > Hi)
		{
			Position.X = Clamp(Position.X, Lo, Hi);
			Velocity.X = 0.0;
			// Nothing left to push against on this axis (AccelRamp).
			Ramp.AX = 0.0;
		}
		if (Position.Y < -Side || Position.Y > Side)
		{
			Position.Y = Clamp(Position.Y, -Side, Side);
			Velocity.Y = 0.0;
			Ramp.AY = 0.0;
		}
		// GDD 6.3 (BIOMECHANICS_MOVEMENT 2.6): a dead stop is speed the clamp throws away.
		// Arriving at the margin slower than EdgeStopToleranceMps is the braking curve's
		// planned arrival plus a rounding hair, not a stop.
		const double Lost = std::max(std::abs(Before.X - Velocity.X), std::abs(Before.Y - Velocity.Y));
		EdgeStopCount += Lost > Tuning->EdgeStopToleranceMps ? 1 : 0;
	}
}
