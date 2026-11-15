// Broadcast end camera maths (GDD 9), in metres on the simulation's axes.
//
// Plain C++20 with no Unreal headers, so the native tests check it on every PR.
// APadelBroadcastCamera feeds it the ball position and the human team's end, steps it
// each frame and converts the pose to world units at the boundary (PadelCourtUnits.h).
//
// - The camera sits behind and above the human team's back glass, looking down the court.
// - It tracks the ball sideways, gently and never more than MaxTrackingM (±1.5 m), by
//   moving, not turning: the look-at point slides with it, so it never rotates in a point.
// - On a change of ends it orbits round the court to the other end during the change-over,
//   so the human's team stays at the bottom of the screen.

#pragma once

namespace Padel::Presentation
{
	// Loaded from Data/Camera.json ("Broadcast" row). Defaults match that file.
	struct FCameraSettings
	{
		double HeightM = 8.0;
		double BehindGlassM = 7.0;
		// The look-at point is this far past the net, in the far half.
		double LookAtPastNetM = 2.0;
		double FieldOfViewDeg = 50.0;
		// Camera offset per metre of ball offset across the court.
		double TrackingStrength = 0.3;
		double MaxTrackingM = 1.5;
		// How fast the tracking offset follows its target, per second.
		double TrackingResponse = 2.5;
		double EndTransitionSeconds = 1.6;
	};

	struct FVec3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	struct FCameraPose
	{
		FVec3 Location;
		FVec3 LookAt;
	};

	// Pose behind the back glass of the half with sign EndSign (+1 = +X), shifted by TrackY.
	FCameraPose EndPose(const FCameraSettings& Settings, int EndSign, double TrackY);

	// The tracking offset the camera aims for when the ball is at BallY across the court.
	double TrackingTarget(const FCameraSettings& Settings, double BallY);

	class FCameraRig
	{
	public:
		explicit FCameraRig(const FCameraSettings& InSettings = FCameraSettings(), int InEnd = 1);

		void SetBallY(double BallY) { BallYM = BallY; }

		// The human team's half (+1 or -1). A change starts the change-over orbit.
		void SetEnd(int EndSign);

		// Advances by DeltaSeconds and returns the pose to show.
		FCameraPose Step(double DeltaSeconds);

		FCameraPose CurrentPose() const;
		int GetEnd() const { return End; }
		bool IsChangingEnds() const { return TransitionElapsed < Settings.EndTransitionSeconds; }
		double GetTrackY() const { return TrackY; }
		const FCameraSettings& GetSettings() const { return Settings; }

	private:
		FCameraSettings Settings;
		int End = 1;
		int FromEnd = 1;
		double TrackY = 0.0;
		double BallYM = 0.0;
		double TransitionElapsed = 0.0;
	};
}
