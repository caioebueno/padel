#include "Presentation/Model/PadelCameraRig.h"

#include "CourtSpec.h"

#include <algorithm>
#include <cmath>

namespace Padel::Presentation
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Ln2 = 0.69314718055994530942;

		double ToRadians(double Degrees) { return Degrees * Pi / 180.0; }
		double ToDegrees(double Radians) { return Radians * 180.0 / Pi; }

		FVec3 Sub(const FVec3& A, const FVec3& B) { return {A.X - B.X, A.Y - B.Y, A.Z - B.Z}; }
		double Dot(const FVec3& A, const FVec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		FVec3 Cross(const FVec3& A, const FVec3& B)
		{
			return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X};
		}
		FVec3 Normalised(const FVec3& V)
		{
			const double Length = std::sqrt(Dot(V, V));
			return Length > 0.0 ? FVec3{V.X / Length, V.Y / Length, V.Z / Length} : FVec3{1.0, 0.0, 0.0};
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
		Pose.VerticalFovDeg = Settings.VerticalFieldOfViewDeg;
		return Pose;
	}

	FCameraPose CutawayPose(const FCameraSettings& Settings, double T)
	{
		// A slow pan about the vertical through the lens, from -Pan/2 to +Pan/2.
		const double Angle = ToRadians(Settings.CutawayPanDeg) * (std::clamp(T, 0.0, 1.0) - 0.5);
		const FVec3 Offset = Sub(Settings.CutawayLookAtM, Settings.CutawayLocationM);
		const double C = std::cos(Angle);
		const double S = std::sin(Angle);
		FCameraPose Pose;
		Pose.Location = Settings.CutawayLocationM;
		Pose.LookAt = {Pose.Location.X + Offset.X * C - Offset.Y * S, Pose.Location.Y + Offset.X * S + Offset.Y * C,
			Pose.Location.Z + Offset.Z};
		Pose.VerticalFovDeg = Settings.CutawayVerticalFieldOfViewDeg;
		return Pose;
	}

	double TrackingTarget(const FCameraSettings& Settings, double BallY, double BallVelY)
	{
		// Seen from the -X end the court's +Y is on screen-left, but the offset is in world
		// space, so the same formula keeps the ball framed from either end.
		const double Lead = BallY + BallVelY * Settings.TrackingLeadSeconds;
		const double Beyond = std::max(std::abs(Lead) - Settings.TrackingDeadzoneM, 0.0);
		const double Offset = std::min(Beyond * Settings.TrackingStrength, Settings.MaxTrackingM);
		return Lead < 0.0 ? -Offset : Offset;
	}

	double HorizontalFovDeg(const FCameraSettings& Settings, double VerticalFovDeg, double Aspect)
	{
		const double SafeAspect = Aspect > 0.0 ? Aspect : 16.0 / 9.0;
		const double Held = ToDegrees(2.0 * std::atan(std::tan(ToRadians(VerticalFovDeg) / 2.0) * SafeAspect));
		// Narrow screens hold the court's width instead, and the picture grows taller.
		return std::max(Held, Settings.MinHorizontalFieldOfViewDeg);
	}

	FScreenPoint ProjectToScreen(const FCameraPose& Pose, double HorizontalFov, double Aspect, const FVec3& Point)
	{
		// The same basis as the engine's look-at rotation: no roll, Z up.
		const FVec3 Forward = Normalised(Sub(Pose.LookAt, Pose.Location));
		const FVec3 Left = Normalised(Cross({0.0, 0.0, 1.0}, Forward));
		const FVec3 Up = Cross(Forward, Left);
		const FVec3 ToPoint = Sub(Point, Pose.Location);
		const double Depth = Dot(ToPoint, Forward);
		FScreenPoint Screen;
		Screen.bInFront = Depth > 0.0;
		if (!Screen.bInFront)
		{
			return Screen;
		}
		const double HalfWidth = std::tan(ToRadians(HorizontalFov) / 2.0);
		const double HalfHeight = HalfWidth / (Aspect > 0.0 ? Aspect : 16.0 / 9.0);
		Screen.U = 0.5 - 0.5 * Dot(ToPoint, Left) / (Depth * HalfWidth);
		Screen.V = 0.5 - 0.5 * Dot(ToPoint, Up) / (Depth * HalfHeight);
		return Screen;
	}

	FCameraRig::FCameraRig(const FCameraSettings& InSettings, int InEnd)
		: Settings(InSettings)
		, End(InEnd >= 0 ? 1 : -1)
	{
	}

	void FCameraRig::SetEnd(int EndSign)
	{
		const int NewEnd = EndSign >= 0 ? 1 : -1;
		if (NewEnd == End)
		{
			return;
		}
		End = NewEnd;
		// GDD 9: cut to the transition shot, then cut to the new end, centred.
		Shot = Settings.EndCutawaySeconds > 0.0 ? EShot::EndCutaway : EShot::Main;
		CutawayElapsed = 0.0;
		TrackY = 0.0;
		TrackVel = 0.0;
	}

	void FCameraRig::OnServeCalled()
	{
		Tracking = ETracking::ServeHold;
	}

	void FCameraRig::OnBallStruck()
	{
		// The first strike after the serve call is the serve: tracking starts there.
		if (Tracking == ETracking::ServeHold)
		{
			Tracking = ETracking::Live;
		}
	}

	void FCameraRig::OnPointEnded()
	{
		Tracking = ETracking::Recentring;
		RecentreElapsed = 0.0;
		RecentreFrom = TrackY;
		TrackVel = 0.0;
	}

	FCameraPose FCameraRig::Step(double DeltaSeconds)
	{
		const double Dt = std::max(DeltaSeconds, 0.0);
		if (Shot == EShot::EndCutaway)
		{
			CutawayElapsed += Dt;
			if (CutawayElapsed >= Settings.EndCutawaySeconds)
			{
				Shot = EShot::Main;
			}
		}
		StepTracking(Dt);
		return CurrentPose();
	}

	void FCameraRig::StepTracking(double Dt)
	{
		switch (Tracking)
		{
		case ETracking::Live:
			SpringToward(TrackingTarget(Settings, BallYM, BallVelYMps), Dt);
			break;
		case ETracking::ServeHold:
			SpringToward(0.0, Dt);
			break;
		case ETracking::Recentring:
		{
			// Hold so the player sees where the point finished, then ease to the centre.
			RecentreElapsed += Dt;
			const double Ease = Settings.RecentreSeconds > 0.0
				? SmoothStep((RecentreElapsed - Settings.RecentreDelaySeconds) / Settings.RecentreSeconds)
				: (RecentreElapsed >= Settings.RecentreDelaySeconds ? 1.0 : 0.0);
			TrackY = RecentreFrom * (1.0 - Ease);
			TrackVel = 0.0;
			break;
		}
		}
	}

	void FCameraRig::SpringToward(double Target, double Dt)
	{
		if (Dt <= 0.0)
		{
			return;
		}
		// Critically damped spring (no overshoot from rest), parameterised by half-life.
		const double HalfLife = std::max(Settings.TrackingHalfLifeSeconds, 1e-3);
		const double Omega = 2.0 * Ln2 / HalfLife;
		const double J0 = TrackY - Target;
		const double J1 = TrackVel + J0 * Omega;
		const double Decay = std::exp(-Omega * Dt);
		double NewY = Decay * (J0 + J1 * Dt) + Target;
		double NewVel = Decay * (TrackVel - J1 * Omega * Dt);

		// Never faster than the cap, so a smash into the side glass can't whip the frame.
		const double MaxStep = Settings.MaxTrackingSpeedMps * Dt;
		if (std::abs(NewY - TrackY) > MaxStep)
		{
			const double Sign = NewY > TrackY ? 1.0 : -1.0;
			NewY = TrackY + Sign * MaxStep;
			NewVel = Sign * Settings.MaxTrackingSpeedMps;
		}
		NewVel = std::clamp(NewVel, -Settings.MaxTrackingSpeedMps, Settings.MaxTrackingSpeedMps);
		TrackY = std::clamp(NewY, -Settings.MaxTrackingM, Settings.MaxTrackingM);
		TrackVel = NewVel;
	}

	FCameraPose FCameraRig::CurrentPose() const
	{
		if (Shot == EShot::EndCutaway)
		{
			const double T = Settings.EndCutawaySeconds > 0.0 ? CutawayElapsed / Settings.EndCutawaySeconds : 1.0;
			return CutawayPose(Settings, T);
		}
		return EndPose(Settings, End, TrackY);
	}
}
