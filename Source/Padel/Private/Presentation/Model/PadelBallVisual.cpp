#include "Presentation/Model/PadelBallVisual.h"

#include "MiniJson.h"

#include <algorithm>
#include <cmath>

namespace Padel::Presentation
{
	namespace
	{
		void ReadBallNumber(const Json::FValue& Object, const char* Key, double& Out)
		{
			if (const Json::FValue* Value = Object.Find(Key); Value && Value->Type == Json::EType::Number)
			{
				Out = Value->Number;
			}
		}

		FBallQuat NormalizedBallQuat(const FBallQuat& Q)
		{
			const double L = std::sqrt(Q.W * Q.W + Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z);
			return L > 1e-12 ? FBallQuat{ Q.W / L, Q.X / L, Q.Y / L, Q.Z / L } : FBallQuat();
		}
	}

	bool ReadBallVisualSettings(const Json::FValue& Root, FBallVisualSettings& Out)
	{
		const Json::FValue* Visual = Root.Find("Visual");
		if (!Visual || Visual->Type != Json::EType::Object)
		{
			return false;
		}
		ReadBallNumber(*Visual, "SpinScale", Out.SpinScale);
		ReadBallNumber(*Visual, "MaxShownRadPerFrame", Out.MaxShownRadPerFrame);
		ReadBallNumber(*Visual, "TrailMinSpeedMps", Out.TrailMinSpeedMps);
		ReadBallNumber(*Visual, "TrailFullSpeedMps", Out.TrailFullSpeedMps);
		ReadBallNumber(*Visual, "TrailSeconds", Out.TrailSeconds);
		ReadBallNumber(*Visual, "TrailMaxLengthM", Out.TrailMaxLengthM);
		ReadBallNumber(*Visual, "TrailMaxOpacity", Out.TrailMaxOpacity);
		ReadBallNumber(*Visual, "TeleportDistanceM", Out.TeleportDistanceM);
		return true;
	}

	bool ParseBallVisualSettings(const std::string& Text, FBallVisualSettings& Out, std::string& Error)
	{
		Json::FValue Root;
		if (!Json::Parse(Text, Root, Error))
		{
			return false;
		}
		FBallVisualSettings Parsed = Out;
		if (!ReadBallVisualSettings(Root, Parsed))
		{
			Error = "no \"Visual\" object";
			return false;
		}
		Out = Parsed;
		return true;
	}

	FBallQuat BallQuatFromAxisAngle(const FVec3& UnitAxis, double Angle)
	{
		const double S = std::sin(Angle * 0.5);
		return { std::cos(Angle * 0.5), UnitAxis.X * S, UnitAxis.Y * S, UnitAxis.Z * S };
	}

	FBallQuat operator*(const FBallQuat& A, const FBallQuat& B)
	{
		return {
			A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z,
			A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y,
			A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X,
			A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W,
		};
	}

	FVec3 Rotate(const FBallQuat& Q, const FVec3& V)
	{
		// v' = v + 2 q x (q x v + w v), with q the vector part.
		const FVec3 U{ Q.X, Q.Y, Q.Z };
		const FVec3 T = Cross(U, V) * 2.0;
		return V + T * Q.W + Cross(U, T);
	}

	double ShownSpinAngle(const FVec3& SpinRadPerS, double DeltaS, const FBallVisualSettings& Settings)
	{
		if (DeltaS <= 0.0)
		{
			return 0.0;
		}
		const double Angle = Size(SpinRadPerS) * std::max(Settings.SpinScale, 0.0) * DeltaS;
		return std::min(Angle, std::max(Settings.MaxShownRadPerFrame, 0.0));
	}

	double TrailOpacityForSpeed(double SpeedMps, const FBallVisualSettings& Settings)
	{
		const double Lo = Settings.TrailMinSpeedMps;
		const double Hi = Settings.TrailFullSpeedMps;
		double T = 0.0;
		if (Hi <= Lo)
		{
			T = SpeedMps >= Lo ? 1.0 : 0.0;
		}
		else
		{
			T = std::clamp((SpeedMps - Lo) / (Hi - Lo), 0.0, 1.0);
		}
		return Settings.TrailMaxOpacity * T * T * (3.0 - 2.0 * T);
	}

	void FBallVisual::Reset(const FVec3& InPosition)
	{
		Position = InPosition;
		TrailTail = InPosition;
		TrailOpacity = 0.0;
		bHasPosition = true;
		History.clear();
		History.push_back({ Clock, InPosition });
	}

	void FBallVisual::Update(const FVec3& InPosition, const FVec3& Velocity, const FVec3& Spin, double DeltaS)
	{
		const double Dt = std::max(DeltaS, 0.0);
		if (!bHasPosition || Size(InPosition - Position) > Settings.TeleportDistanceM)
		{
			Reset(InPosition);
		}

		// Turn about the spin axis (world axes, like the simulation's), by the shown angle.
		const double Angle = ShownSpinAngle(Spin, Dt, Settings);
		if (Angle > 0.0)
		{
			const FVec3 Axis = SafeNormal(Spin);
			Orientation = NormalizedBallQuat(BallQuatFromAxisAngle(Axis, Angle) * Orientation);
		}

		Clock += Dt;
		Position = InPosition;
		if (Dt > 0.0 || History.empty())
		{
			History.push_back({ Clock, InPosition });
		}
		else
		{
			History.back().Position = InPosition;
		}
		UpdateTrail(Size(Velocity));
	}

	void FBallVisual::UpdateTrail(double SpeedMps)
	{
		// Keep one sample at or before the trail's reach, so the tail can be interpolated.
		const double Reach = Clock - std::max(Settings.TrailSeconds, 0.0);
		while (History.size() > 2 && History[1].Time <= Reach)
		{
			History.erase(History.begin());
		}

		FVec3 Tail = History.front().Position;
		if (History.size() >= 2 && History.front().Time < Reach)
		{
			// The tail lies between the oldest sample and the next one.
			const FSample& A = History[0];
			const FSample& B = History[1];
			const double Span = B.Time - A.Time;
			const double T = Span > 0.0 ? std::clamp((Reach - A.Time) / Span, 0.0, 1.0) : 1.0;
			Tail = A.Position + (B.Position - A.Position) * T;
		}
		FVec3 Back = Tail - Position;
		const double Length = Size(Back);
		if (Length > Settings.TrailMaxLengthM && Length > 0.0)
		{
			Back = Back * (Settings.TrailMaxLengthM / Length);
		}
		TrailTail = Position + Back;
		TrailOpacity = Size(Back) > 1e-6 ? TrailOpacityForSpeed(SpeedMps, Settings) : 0.0;
	}
}
