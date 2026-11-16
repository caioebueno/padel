// GDD 9 broadcast camera: framing matched to a televised frame (camera spec, section 3),
// gentle tracking, the serve and point lifecycle, and the cut on a change of ends.
#include "doctest.h"

#include "CourtSpec.h"
#include "Presentation/Model/PadelCameraRig.h"

#include <cmath>
#include <initializer_list>

using namespace Padel::Presentation;
using doctest::Approx;

namespace
{
	constexpr double Wide = 16.0 / 9.0;
	constexpr double HalfLength = Padel::Court::HalfLength;
	constexpr double HalfWidth = Padel::Court::HalfWidth;
	constexpr double PlayerHeightM = 1.85;

	FScreenPoint Project(const FCameraSettings& Settings, const FCameraPose& Pose, double Aspect, const FVec3& Point)
	{
		return ProjectToScreen(Pose, HorizontalFovDeg(Settings, Pose.VerticalFovDeg, Aspect), Aspect, Point);
	}

	// The landmark's height on screen with the human team at End: 0 = top, 1 = bottom.
	double ScreenV(const FCameraSettings& Settings, double Aspect, double NearX, double Z, int End = 1)
	{
		return Project(Settings, EndPose(Settings, End, 0.0), Aspect, {End * NearX, 0.0, Z}).V;
	}

	double CourtWidthOnScreen(const FCameraSettings& Settings, double Aspect, double NearX)
	{
		const FCameraPose Pose = EndPose(Settings, 1, 0.0);
		return std::abs(Project(Settings, Pose, Aspect, {NearX, HalfWidth, 0.0}).U
			- Project(Settings, Pose, Aspect, {NearX, -HalfWidth, 0.0}).U);
	}

	double PlayerHeightOnScreen(const FCameraSettings& Settings, double NearX)
	{
		const FCameraPose Pose = EndPose(Settings, 1, 0.0);
		return Project(Settings, Pose, Wide, {NearX, 0.0, 0.0}).V - Project(Settings, Pose, Wide, {NearX, 0.0, PlayerHeightM}).V;
	}
}

TEST_CASE("Presentation.Camera: a long lens far behind and above the human team's glass (GDD 9)")
{
	const FCameraSettings Settings;
	const FCameraPose Pose = EndPose(Settings, 1, 0.0);
	CHECK(Pose.Location.Z >= 7.0);
	CHECK(Pose.Location.Z <= 8.0);
	const double Behind = Pose.Location.X - HalfLength;
	CHECK(Behind >= 13.0);
	CHECK(Behind <= 15.0);
	// Aimed at the foot of the net, looking down the court.
	CHECK(Pose.LookAt.X <= 0.5);
	CHECK(Pose.LookAt.Z == Approx(0.0));
	CHECK(Pose.VerticalFovDeg == Approx(26.0).epsilon(0.1));

	const FCameraPose Other = EndPose(Settings, -1, 0.0);
	CHECK(Other.Location.X == Approx(-Pose.Location.X));
	CHECK(Other.LookAt.X >= -0.5);
}

TEST_CASE("Presentation.Camera: frames the court like the broadcast reference (camera spec 3)")
{
	const FCameraSettings Settings;
	for (const int End : {1, -1})
	{
		INFO("human team's end " << End);
		// Far back wall: the top of its mesh is in frame, near the top.
		const double FarMeshTop = ScreenV(Settings, Wide, -HalfLength, Padel::Court::FenceTotalHeight, End);
		CHECK(FarMeshTop >= 0.03);
		CHECK(FarMeshTop <= 0.10);
		const double FarGlassFoot = ScreenV(Settings, Wide, -HalfLength, 0.0, End);
		CHECK(FarGlassFoot >= 0.28);
		CHECK(FarGlassFoot <= 0.36);
		// The net sits just above the middle.
		const double Net = ScreenV(Settings, Wide, 0.0, Padel::Court::NetHeightCentre, End);
		CHECK(Net >= 0.39);
		CHECK(Net <= 0.47);
		const double ServiceT = ScreenV(Settings, Wide, Padel::Court::ServiceLineFromNet, 0.0, End);
		CHECK(ServiceT >= 0.70);
		CHECK(ServiceT <= 0.78);
		// The near baseline, with room below it for the near pair's feet.
		const double NearBaseline = ScreenV(Settings, Wide, HalfLength, 0.0, End);
		CHECK(NearBaseline >= 0.85);
		CHECK(NearBaseline <= 0.94);
	}

	const double NearWidth = CourtWidthOnScreen(Settings, Wide, HalfLength);
	CHECK(NearWidth >= 0.72);
	CHECK(NearWidth <= 0.86);
	const double FarWidth = CourtWidthOnScreen(Settings, Wide, -HalfLength);
	CHECK(FarWidth >= 0.32);
	CHECK(FarWidth <= 0.42);

	// Near pair big, far pair small: about two to one, as on television.
	const double Ratio = PlayerHeightOnScreen(Settings, 8.0) / PlayerHeightOnScreen(Settings, -8.0);
	CHECK(Ratio >= 1.5);
	CHECK(Ratio <= 2.0);
}

TEST_CASE("Presentation.Camera: every screen shape shows the whole court")
{
	const FCameraSettings Settings;
	// 16:9, MacBook 16:10, ultrawide 21:9, and 4:3 where the horizontal floor takes over.
	for (const double Aspect : {16.0 / 9.0, 16.0 / 10.0, 21.0 / 9.0, 4.0 / 3.0})
	{
		INFO("aspect " << Aspect);
		const double Hfov = HorizontalFovDeg(Settings, Settings.VerticalFieldOfViewDeg, Aspect);
		CHECK(Hfov >= Settings.MinHorizontalFieldOfViewDeg);
		const FCameraPose Pose = EndPose(Settings, 1, 0.0);
		for (const double X : {-HalfLength, HalfLength})
		{
			for (const double Y : {-HalfWidth, HalfWidth})
			{
				INFO("corner " << X << ", " << Y);
				CHECK(ProjectToScreen(Pose, Hfov, Aspect, {X, Y, 0.0}).IsInFrame());
			}
		}
	}
	// The vertical field of view is what's held: wider screens get a wider horizontal one.
	CHECK(HorizontalFovDeg(Settings, 26.0, 21.0 / 9.0) > HorizontalFovDeg(Settings, 26.0, 16.0 / 9.0));
	CHECK(HorizontalFovDeg(Settings, 26.0, 16.0 / 9.0) == Approx(44.6).epsilon(0.01));
}

TEST_CASE("Presentation.Camera: both pairs stay in frame at every tracking offset")
{
	const FCameraSettings Settings;
	for (const int End : {1, -1})
	{
		for (const double TrackY : {-Settings.MaxTrackingM, 0.0, Settings.MaxTrackingM})
		{
			const FCameraPose Pose = EndPose(Settings, End, TrackY);
			// A player on either baseline (feet half a metre inside), against either side wall.
			for (const double X : {HalfLength - 0.5, -(HalfLength - 0.5)})
			{
				for (const double Y : {HalfWidth - 0.3, -(HalfWidth - 0.3)})
				{
					INFO("end " << End << ", track " << TrackY << ", player at " << X << ", " << Y);
					CHECK(Project(Settings, Pose, Wide, {X, Y, 0.0}).IsInFrame());
					CHECK(Project(Settings, Pose, Wide, {X, Y, PlayerHeightM}).IsInFrame());
				}
			}
		}
	}
}

TEST_CASE("Presentation.Camera: a ball near the middle doesn't move the camera; the range is capped")
{
	const FCameraSettings Settings;
	CHECK(TrackingTarget(Settings, 0.0) == Approx(0.0));
	CHECK(TrackingTarget(Settings, Settings.TrackingDeadzoneM) == Approx(0.0));
	CHECK(TrackingTarget(Settings, -Settings.TrackingDeadzoneM) == Approx(0.0));
	CHECK(TrackingTarget(Settings, 3.0) > 0.0);
	CHECK(TrackingTarget(Settings, -3.0) == Approx(-TrackingTarget(Settings, 3.0)));
	CHECK(TrackingTarget(Settings, 100.0) == Approx(Settings.MaxTrackingM));
	CHECK(TrackingTarget(Settings, -100.0) == Approx(-Settings.MaxTrackingM));
	CHECK(Settings.MaxTrackingM <= 1.0);
	// It leads the ball: a ball in the middle heading for the wall already moves the aim.
	CHECK(TrackingTarget(Settings, 0.5, 10.0) > 0.0);
}

TEST_CASE("Presentation.Camera: tracking eases in, never overshoots, never whips, never turns")
{
	const FCameraSettings Settings;
	FCameraRig Rig(Settings, 1);
	const FCameraPose Start = Rig.CurrentPose();

	// From one side to the other.
	Rig.SetBallY(-100.0);
	for (int Frame = 0; Frame < 600; ++Frame)
	{
		Rig.Step(1.0 / 60.0);
	}
	CHECK(Rig.GetTrackY() == Approx(-Settings.MaxTrackingM).epsilon(0.01));

	Rig.SetBallY(100.0);
	double Previous = Rig.GetTrackY();
	const double FirstStep = [&]
	{
		Rig.Step(1.0 / 60.0);
		return Rig.GetTrackY() - Previous;
	}();
	// Eases in rather than starting at full speed.
	CHECK(FirstStep > 0.0);
	CHECK(FirstStep < Settings.MaxTrackingSpeedMps / 60.0 * 0.5);
	Previous = Rig.GetTrackY();
	FCameraPose Pose;
	for (int Frame = 0; Frame < 600; ++Frame)
	{
		Pose = Rig.Step(1.0 / 60.0);
		const double Moved = Rig.GetTrackY() - Previous;
		CHECK(Moved >= -1e-12);
		CHECK(Moved <= Settings.MaxTrackingSpeedMps / 60.0 + 1e-9);
		CHECK(Rig.GetTrackY() <= Settings.MaxTrackingM + 1e-9);
		Previous = Rig.GetTrackY();
	}
	CHECK(Rig.GetTrackY() == Approx(Settings.MaxTrackingM).epsilon(0.01));

	// Same view direction as the start: it moved sideways, it didn't rotate.
	CHECK(Pose.LookAt.X - Pose.Location.X == Approx(Start.LookAt.X - Start.Location.X));
	CHECK(Pose.LookAt.Y - Pose.Location.Y == Approx(Start.LookAt.Y - Start.Location.Y));
	CHECK(Pose.LookAt.Z - Pose.Location.Z == Approx(Start.LookAt.Z - Start.Location.Z));
}

TEST_CASE("Presentation.Camera: each serve starts centred, tracking starts at the serve strike")
{
	const FCameraSettings Settings;
	FCameraRig Rig(Settings, 1);
	Rig.SetBallY(100.0);
	for (int Frame = 0; Frame < 300; ++Frame)
	{
		Rig.Step(1.0 / 60.0);
	}
	REQUIRE(Rig.GetTrackY() > 0.5);

	Rig.OnServeCalled();
	CHECK(Rig.GetTracking() == FCameraRig::ETracking::ServeHold);
	// The server is out wide and the ball moves in the toss: the camera still centres.
	Rig.SetBall(4.0, 3.0);
	for (int Frame = 0; Frame < 300; ++Frame)
	{
		Rig.Step(1.0 / 60.0);
	}
	CHECK(Rig.GetTrackY() == Approx(0.0).epsilon(0.001).scale(1.0));

	Rig.OnBallStruck();
	CHECK(Rig.GetTracking() == FCameraRig::ETracking::Live);
	Rig.Step(0.5);
	CHECK(Rig.GetTrackY() > 0.0);
}

TEST_CASE("Presentation.Camera: after a point it holds, then eases back to the centre")
{
	const FCameraSettings Settings;
	FCameraRig Rig(Settings, 1);
	Rig.SetBallY(-100.0);
	for (int Frame = 0; Frame < 300; ++Frame)
	{
		Rig.Step(1.0 / 60.0);
	}
	const double AtPointEnd = Rig.GetTrackY();
	REQUIRE(AtPointEnd < -0.5);

	Rig.OnPointEnded();
	// A strike after the point (a ball knocked back) doesn't restart tracking.
	Rig.OnBallStruck();
	Rig.Step(Settings.RecentreDelaySeconds * 0.9);
	CHECK(Rig.GetTrackY() == Approx(AtPointEnd));
	Rig.Step(Settings.RecentreDelaySeconds * 0.1 + Settings.RecentreSeconds * 0.5);
	CHECK(Rig.GetTrackY() > AtPointEnd);
	CHECK(Rig.GetTrackY() < 0.0);
	Rig.Step(Settings.RecentreSeconds);
	CHECK(Rig.GetTrackY() == Approx(0.0));
}

TEST_CASE("Presentation.Camera: a change of ends cuts through the transition shot (GDD 9)")
{
	const FCameraSettings Settings;
	FCameraRig Rig(Settings, 1);
	const FCameraPose Before = Rig.CurrentPose();

	Rig.SetEnd(-1);
	CHECK(Rig.IsChangingEnds());
	// A cut, not a move: the very next frame is the transition shot from the net side.
	const FCameraPose Cutaway = Rig.Step(1.0 / 60.0);
	CHECK(Cutaway.Location.X == Approx(Settings.CutawayLocationM.X));
	CHECK(Cutaway.Location.Y == Approx(Settings.CutawayLocationM.Y));
	CHECK(Cutaway.Location.Z == Approx(Settings.CutawayLocationM.Z));
	CHECK(Cutaway.VerticalFovDeg == Approx(Settings.CutawayVerticalFieldOfViewDeg));
	// It pans slowly, from a fixed spot.
	const FCameraPose Later = Rig.Step(Settings.EndCutawaySeconds * 0.5);
	CHECK(Later.Location.Y == Approx(Cutaway.Location.Y));
	CHECK(Later.LookAt.X != Approx(Cutaway.LookAt.X));

	Rig.Step(Settings.EndCutawaySeconds);
	CHECK_FALSE(Rig.IsChangingEnds());
	const FCameraPose After = Rig.CurrentPose();
	CHECK(After.Location.X == Approx(-Before.Location.X));
	CHECK(After.Location.Y == Approx(0.0));
	CHECK(After.LookAt.X >= -0.5);
	// The human's team is at the bottom again: their baseline is low on screen.
	CHECK(Project(Settings, After, Wide, {-HalfLength, 0.0, 0.0}).V > 0.85);

	// Setting the same end again does nothing.
	Rig.SetEnd(-1);
	CHECK_FALSE(Rig.IsChangingEnds());
}

TEST_CASE("Presentation.Camera: the same inputs give the same poses")
{
	const FCameraSettings Settings;
	FCameraRig A(Settings, 1);
	FCameraRig B(Settings, 1);
	for (int Frame = 0; Frame < 400; ++Frame)
	{
		const double BallY = 4.5 * std::sin(Frame * 0.05);
		const double BallVelY = 4.5 * 0.05 * 60.0 * std::cos(Frame * 0.05);
		A.SetBall(BallY, BallVelY);
		B.SetBall(BallY, BallVelY);
		if (Frame == 200)
		{
			A.OnPointEnded();
			B.OnPointEnded();
		}
		const FCameraPose PA = A.Step(1.0 / 120.0);
		const FCameraPose PB = B.Step(1.0 / 120.0);
		CHECK(PA.Location.Y == PB.Location.Y);
	}
}
