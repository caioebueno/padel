#include "Presentation/Model/PadelCameraRig.h"

#include "CourtSpec.h"

#include <algorithm>
#include <cmath>

namespace Padel::Presentation
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		FVec3 RotateAboutZ(const FVec3& V, double Angle)
		{
			const double C = std::cos(Angle);
			const double S = std::sin(Angle);
			return {V.X * C - V.Y * S, V.X * S + V.Y * C, V.Z};
		}

		double SmoothStep(double T)
		{
			T = std::clamp(T, 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
	}

	FCameraPose EndPose(const FCameraSettings& Settings, int EndSign, double TrackY)
	{
		const double Sign = EndSign >= 0 ? 1.0 : -1.0;
		FCameraPose Pose;
		Pose.Location = {Sign * (Court::HalfLength + Settings.BehindGlassM), TrackY, Settings.HeightM};
		Pose.LookAt = {-Sign * Settings.LookAtPastNetM, TrackY, 0.0};
		return Pose;
	}

	double TrackingTarget(const FCameraSettings& Settings, double BallY)
	{
		// Seen from the -X end the court's +Y is on screen-left, but the offset is in world
		// space, so the same formula keeps the ball centred from either end.
		return std::clamp(BallY * Settings.TrackingStrength, -Settings.MaxTrackingM, Settings.MaxTrackingM);
	}

	FCameraRig::FCameraRig(const FCameraSettings& InSettings, int InEnd)
		: Settings(InSettings)
		, End(InEnd >= 0 ? 1 : -1)
		, FromEnd(End)
		, TransitionElapsed(InSettings.EndTransitionSeconds)
	{
	}

	void FCameraRig::SetEnd(int EndSign)
	{
		const int NewEnd = EndSign >= 0 ? 1 : -1;
		if (NewEnd == End)
		{
			return;
		}
		FromEnd = End;
		End = NewEnd;
		TransitionElapsed = 0.0;
		// The orbit starts from the centred pose; tracking resumes at the new end.
		TrackY = 0.0;
	}

	FCameraPose FCameraRig::Step(double DeltaSeconds)
	{
		const double Dt = std::max(DeltaSeconds, 0.0);
		if (IsChangingEnds())
		{
			TransitionElapsed = std::min(TransitionElapsed + Dt, Settings.EndTransitionSeconds);
		}
		else
		{
			const double Alpha = 1.0 - std::exp(-Settings.TrackingResponse * Dt);
			TrackY += (TrackingTarget(Settings, BallYM) - TrackY) * Alpha;
		}
		return CurrentPose();
	}

	FCameraPose FCameraRig::CurrentPose() const
	{
		if (!IsChangingEnds() || Settings.EndTransitionSeconds <= 0.0)
		{
			return EndPose(Settings, End, TrackY);
		}
		// Orbit half a turn round the court's centre, from the old end to the new one.
		const double T = SmoothStep(TransitionElapsed / Settings.EndTransitionSeconds);
		const FCameraPose From = EndPose(Settings, FromEnd, 0.0);
		const double Angle = Pi * T;
		return {RotateAboutZ(From.Location, Angle), RotateAboutZ(From.LookAt, Angle)};
	}
}
