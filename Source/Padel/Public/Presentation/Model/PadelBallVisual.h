// How the ball looks in motion (GDD 10: readable at a glance, a subtle motion trail at
// high speed, a seam that shows spin). See docs/decisions/0007-ball.md.
//
// Plain C++20 with no Unreal headers, so the native tests check it on every PR. APadelBall
// feeds it the simulation's ball every frame and applies the result to its meshes.
//
// It is presentation, not simulation: it runs on frame time and never feeds back into
// PadelCore. Two things it does on purpose that the simulation doesn't:
//
//  - Spin is shown, not reproduced. A smash spins at 50 turns a second or more, which at
//    60 frames a second is most of a turn per frame: the eye sees the seam jump at random
//    or even turn backwards (the wagon-wheel effect). So the mesh turns about the real
//    spin axis, in the real direction, at the real rate up to MaxShownRadPerFrame per
//    frame and no faster. Slow spin (a sliced lob, a chiquita) is shown exactly.
//  - The trail follows the path, not the velocity. Its tail is where the ball was
//    TrailSeconds ago, so after a bounce off the glass it points back to the glass
//    instead of through it.

#pragma once

#include "VecMath.h"

#include <string>
#include <vector>

namespace Padel::Json
{
	struct FValue;
}

namespace Padel::Presentation
{
	// Loaded from Data/Ball.json ("Visual"). Defaults match that file.
	struct FBallVisualSettings
	{
		// Shown spin = real spin * SpinScale, capped per frame below.
		double SpinScale = 1.0;
		// Most the ball turns in one frame, radians. Under a quarter turn keeps the
		// direction readable: the seam has two-fold symmetry, so half a turn looks like none.
		double MaxShownRadPerFrame = 0.9;
		// No trail below TrailMinSpeedMps; full opacity from TrailFullSpeedMps.
		double TrailMinSpeedMps = 15.0;
		double TrailFullSpeedMps = 30.0;
		// The trail reaches back to where the ball was this long ago, at most TrailMaxLengthM.
		double TrailSeconds = 0.04;
		double TrailMaxLengthM = 1.1;
		double TrailMaxOpacity = 0.35;
		// A jump bigger than this in one frame is a new ball (a serve toss, a reset), not motion.
		double TeleportDistanceM = 1.5;
	};

	// Reads a "Visual" object like Data/Ball.json's. Missing keys keep their defaults.
	// Returns false (and leaves Out as it was) when Root has no "Visual" object.
	bool ReadBallVisualSettings(const Json::FValue& Root, FBallVisualSettings& Out);

	// Parses the text of Data/Ball.json. On failure Out keeps its values and Error says why.
	bool ParseBallVisualSettings(const std::string& Text, FBallVisualSettings& Out, std::string& Error);

	// Unit quaternion (W + Xi + Yj + Zk) on the court axes; the mesh's rotation.
	struct FBallQuat
	{
		double W = 1.0;
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	FBallQuat BallQuatFromAxisAngle(const Court::FVec3& UnitAxis, double Angle);
	// A then B: rotates by B first, then by A (like Unreal's FQuat A * B).
	FBallQuat operator*(const FBallQuat& A, const FBallQuat& B);
	Court::FVec3 Rotate(const FBallQuat& Q, const Court::FVec3& V);

	// The angle the ball turns this frame (radians, >= 0) about the spin's own axis.
	double ShownSpinAngle(const Court::FVec3& SpinRadPerS, double DeltaS, const FBallVisualSettings& Settings);

	// 0 below TrailMinSpeedMps, TrailMaxOpacity from TrailFullSpeedMps, smooth in between.
	double TrailOpacityForSpeed(double SpeedMps, const FBallVisualSettings& Settings);

	class FBallVisual
	{
	public:
		explicit FBallVisual(const FBallVisualSettings& InSettings = FBallVisualSettings()) : Settings(InSettings) {}

		// Starts afresh at Position: no trail. The orientation is kept, so a new ball
		// doesn't snap to a fixed pose.
		void Reset(const Court::FVec3& Position);

		// One presented frame: the ball is at Position with this velocity and spin (sim
		// units: metres, m/s, rad/s). DeltaS is the frame time; 0 adds a sample without
		// turning the ball (two presents in one frame).
		void Update(const Court::FVec3& Position, const Court::FVec3& Velocity, const Court::FVec3& Spin, double DeltaS);

		const FBallQuat& GetOrientation() const { return Orientation; }
		const Court::FVec3& GetPosition() const { return Position; }
		// The trail runs from the ball back to the tail. Opacity 0 means no trail.
		const Court::FVec3& GetTrailTail() const { return TrailTail; }
		double GetTrailLength() const { return Size(Position - TrailTail); }
		double GetTrailOpacity() const { return TrailOpacity; }
		const FBallVisualSettings& GetSettings() const { return Settings; }

	private:
		struct FSample
		{
			double Time = 0.0;
			Court::FVec3 Position;
		};

		void UpdateTrail(double SpeedMps);

		FBallVisualSettings Settings;
		FBallQuat Orientation;
		Court::FVec3 Position;
		Court::FVec3 TrailTail;
		double TrailOpacity = 0.0;
		double Clock = 0.0;
		bool bHasPosition = false;
		// Oldest first; only what the trail can reach.
		std::vector<FSample> History;
	};
}
