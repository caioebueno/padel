// Broadcast end camera maths (GDD 9), in metres on the simulation's axes.
//
// Plain C++20 with no Unreal headers, so the native tests check it on every PR.
// APadelBroadcastCamera feeds it the ball and the match's moments, steps it each frame
// and converts the pose to world units at the boundary (PadelCourtUnits.h).
//
// - A long lens from far behind and above the human team's back glass, aimed at the foot
//   of the net, matched to a televised frame (camera spec, GDD 9): the whole court, both
//   back walls and both pairs are always in shot.
// - It tracks the ball sideways by moving, not turning (the look-at point slides with it),
//   ignores a ball near the middle, and eases with a critically damped spring capped in
//   speed and range, so a hit never jolts the picture.
// - Each serve starts centred: tracking is held from the serve call to the serve strike,
//   and after a point the camera holds, then eases back to the centre.
// - A change of ends cuts to a transition shot from the net side, then to the other end.

#pragma once

namespace Padel::Presentation
{
	struct FVec3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	// Loaded from Data/Camera.json. Defaults match that file.
	struct FCameraSettings
	{
		// "Broadcast": the main camera.
		double HeightM = 7.25;
		double BehindGlassM = 14.0;
		// The aim point on the floor is this far past the net, in the far half (0 = the net's foot).
		double LookAtPastNetM = 0.0;
		// Held for every screen shape, down to MinHorizontalFieldOfViewDeg across.
		double VerticalFieldOfViewDeg = 26.0;
		double MinHorizontalFieldOfViewDeg = 40.0;
		// A ball within this distance of the centre line doesn't move the camera.
		double TrackingDeadzoneM = 1.0;
		// Camera offset per metre of ball offset beyond the dead zone.
		double TrackingStrength = 0.3;
		double MaxTrackingM = 1.0;
		// The camera aims where the ball will be this far ahead.
		double TrackingLeadSeconds = 0.25;
		double TrackingHalfLifeSeconds = 0.5;
		double MaxTrackingSpeedMps = 1.2;
		double RecentreDelaySeconds = 0.6;
		double RecentreSeconds = 1.0;
		double EndCutawaySeconds = 2.5;

		// "EndCutaway": the change-of-ends transition shot, from the net-side position.
		FVec3 CutawayLocationM = {0.0, 9.8, 2.2};
		FVec3 CutawayLookAtM = {0.0, -2.0, 1.0};
		double CutawayVerticalFieldOfViewDeg = 30.0;
		// Total slow pan across the shot.
		double CutawayPanDeg = 2.0;
	};

	struct FCameraPose
	{
		FVec3 Location;
		FVec3 LookAt;
		double VerticalFovDeg = 26.0;
	};

	// Where a point lands on screen: 0..1 from the left and from the top.
	struct FScreenPoint
	{
		double U = 0.0;
		double V = 0.0;
		bool bInFront = false;

		bool IsInFrame() const { return bInFront && U >= 0.0 && U <= 1.0 && V >= 0.0 && V <= 1.0; }
	};

	// Main-camera pose behind the back glass of the half with sign EndSign (+1 = +X), shifted by TrackY.
	FCameraPose EndPose(const FCameraSettings& Settings, int EndSign, double TrackY);

	// The change-of-ends transition shot, T from 0 to 1 across the shot (it pans slowly).
	FCameraPose CutawayPose(const FCameraSettings& Settings, double T);

	// The tracking offset the camera aims for with the ball at BallY moving at BallVelY across the court.
	double TrackingTarget(const FCameraSettings& Settings, double BallY, double BallVelY = 0.0);

	// The horizontal field of view to give the engine (which holds the horizontal) for a pose's
	// vertical field of view on a screen of this width / height, floored at the minimum.
	double HorizontalFovDeg(const FCameraSettings& Settings, double VerticalFovDeg, double Aspect);

	// Projects a point through a pose (no roll, Z up) with this horizontal FOV and aspect.
	FScreenPoint ProjectToScreen(const FCameraPose& Pose, double HorizontalFovDeg, double Aspect, const FVec3& Point);

	class FCameraRig
	{
	public:
		enum class EShot
		{
			Main,
			EndCutaway,
		};

		enum class ETracking
		{
			// Following the ball.
			Live,
			// From the serve call to the serve strike: easing to, then held at, the centre.
			ServeHold,
			// After a point: holding, then easing back to the centre.
			Recentring,
		};

		explicit FCameraRig(const FCameraSettings& InSettings = FCameraSettings(), int InEnd = 1);

		void SetBall(double BallY, double BallVelY)
		{
			BallYM = BallY;
			BallVelYMps = BallVelY;
		}
		void SetBallY(double BallY) { SetBall(BallY, 0.0); }

		// The human team's half (+1 or -1). A change cuts to the transition shot, then to that end.
		void SetEnd(int EndSign);

		// Match moments (UPadelMatchEvents): a serve is called (also after a fault or let), any
		// ball is struck, a point ends.
		void OnServeCalled();
		void OnBallStruck();
		void OnPointEnded();

		// Advances by DeltaSeconds and returns the pose to show.
		FCameraPose Step(double DeltaSeconds);

		FCameraPose CurrentPose() const;
		int GetEnd() const { return End; }
		EShot GetShot() const { return Shot; }
		ETracking GetTracking() const { return Tracking; }
		bool IsChangingEnds() const { return Shot == EShot::EndCutaway; }
		double GetTrackY() const { return TrackY; }
		double GetTrackVelocity() const { return TrackVel; }
		const FCameraSettings& GetSettings() const { return Settings; }

	private:
		void StepTracking(double Dt);
		void SpringToward(double Target, double Dt);

		FCameraSettings Settings;
		int End = 1;
		EShot Shot = EShot::Main;
		ETracking Tracking = ETracking::Live;
		double TrackY = 0.0;
		double TrackVel = 0.0;
		double BallYM = 0.0;
		double BallVelYMps = 0.0;
		double CutawayElapsed = 0.0;
		double RecentreElapsed = 0.0;
		double RecentreFrom = 0.0;
	};
}
