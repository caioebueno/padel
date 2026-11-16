// GDD 10 lighting: every preset in Data/Lighting.json lights the court like a real
// televised venue (level, evenness, no glare into the broadcast camera, an exposure a
// light meter agrees with), fairly for both halves, inside the Metal shadow budget.
#include "doctest.h"

#include "CourtSpec.h"
#include "Presentation/Model/PadelCameraRig.h"
#include "Presentation/Model/PadelLightingModel.h"
#include "../Support/LightingTestData.h"

#include <cmath>
#include <string>
#include <vector>

using namespace Padel::Presentation;
using namespace Padel::Presentation::Lighting;
using doctest::Approx;

namespace
{
	FFixture DownLight(double Height, double IntensityCd)
	{
		FFixture F;
		F.Position = { 0.0, 0.0, Height };
		F.Direction = { 0.0, 0.0, -1.0 };
		F.IntensityCd = IntensityCd;
		F.InnerConeDeg = 30.0;
		F.OuterConeDeg = 45.0;
		F.AttenuationRadiusM = 100.0;
		return F;
	}

	double LuxAt(const std::vector<FFixture>& Fixtures, double X, double Y)
	{
		double Lux = 0.0;
		for (const FFixture& F : Fixtures)
		{
			Lux += IlluminanceLux(F, FVec3{ X, Y, 0.0 }, FVec3{ 0.0, 0.0, 1.0 });
		}
		return Lux;
	}
}

TEST_CASE("Lighting.Model.InverseSquareUnderAFixture")
{
	const FFixture F = DownLight(8.0, 32000.0);
	CHECK(IlluminanceLux(F, FVec3{ 0.0, 0.0, 0.0 }, FVec3{ 0.0, 0.0, 1.0 }) == Approx(500.0));
	// Facing away from the light gets nothing.
	CHECK(IlluminanceLux(F, FVec3{ 0.0, 0.0, 0.0 }, FVec3{ 0.0, 0.0, -1.0 }) == 0.0);
	// Beyond the range window the engine renders nothing either.
	FFixture Short = F;
	Short.AttenuationRadiusM = 5.0;
	CHECK(IlluminanceLux(Short, FVec3{ 0.0, 0.0, 0.0 }, FVec3{ 0.0, 0.0, 1.0 }) == 0.0);
}

TEST_CASE("Lighting.Model.ConeFalloffMatchesTheEngineCurve")
{
	const FFixture F = DownLight(8.0, 1.0);
	CHECK(ConeFalloff(F, FVec3{ 0.0, 0.0, 0.0 }) == Approx(1.0));
	// Inside the inner cone: full strength. Outside the outer cone: dark.
	CHECK(ConeFalloff(F, FVec3{ 8.0 * std::tan(0.5), 0.0, 0.0 }) == Approx(1.0));
	CHECK(ConeFalloff(F, FVec3{ 8.0 * std::tan(0.8), 0.0, 0.0 }) == 0.0);
	const double Mid = ConeFalloff(F, FVec3{ 8.0 * std::tan(37.5 * 3.14159265358979 / 180.0), 0.0, 0.0 });
	CHECK(Mid > 0.0);
	CHECK(Mid < 1.0);
}

TEST_CASE("Lighting.Model.ExposureMaths")
{
	// An incident meter reads EV100 0 at 2.5 lux, and each doubling is one stop.
	CHECK(MeteredEV100(2.5) == Approx(0.0));
	CHECK(MeteredEV100(5.0) == Approx(1.0));
	// Sunny 16: about 100,000 lux is EV100 15.
	CHECK(MeteredEV100(100000.0) == Approx(15.3).epsilon(0.01));
	// f/2.26 at 1/100 s, ISO 100, is EV100 9; doubling ISO needs a stop less light.
	const double N = ApertureForEV100(9.0, 100.0, 100.0);
	CHECK(std::log2(N * N * 100.0) == Approx(9.0));
	CHECK(ApertureForEV100(9.0, 100.0, 200.0) == Approx(N * std::sqrt(2.0)));
}

TEST_CASE("Lighting.Model.SunDirection")
{
	FSunSettings Overhead;
	Overhead.ElevationDeg = 90.0;
	const FVec3 Down = SunDirection(Overhead);
	CHECK(Down.Z == Approx(-1.0));
	FSunSettings FromPlusX;
	FromPlusX.ElevationDeg = 0.0;
	FromPlusX.AzimuthDeg = 0.0;
	CHECK(SunDirection(FromPlusX).X == Approx(-1.0));
}

TEST_CASE("Lighting.Model.LoaderRejectsUnknownKeys")
{
	FLightingPreset Preset;
	CHECK(SetPresetNumber(Preset, "Post.ExposureEV100", 11.0));
	CHECK(Preset.Post.ExposureEV100 == 11.0);
	CHECK_FALSE(SetPresetNumber(Preset, "Post.ExposureEv100", 11.0));
	CHECK_FALSE(SetPresetFlag(Preset, "Sun.Enable", true));
	CHECK_FALSE(SetPresetText(Preset, "Venue", "Stadium"));
	FFixtureRow Row;
	CHECK(SetRowNumber(Row, "Count", 4.0));
	CHECK(Row.Count == 4);
	CHECK_FALSE(SetRowNumber(Row, "Intensity", 1.0));
}

TEST_CASE("Lighting.Data.EveryKeyIsKnownAndTheDefaultExists")
{
	const LightingTestData::FLightingFile File = LightingTestData::Load();
	for (const std::string& Key : File.UnknownKeys)
	{
		INFO("unknown key in Data/Lighting.json: " << Key);
		CHECK(false);
	}
	CHECK(File.Presets.count(File.Default) == 1);
	// The slice court is indoors (GDD 12).
	CHECK(File.Presets.at(File.Default).Venue == EVenue::Indoor);
	for (const char* Name : { "IndoorClub", "OutdoorDay", "OutdoorNight" })
	{
		CHECK(File.Presets.count(Name) == 1);
	}
}

TEST_CASE("Lighting.Data.FloodlitPresetsLightTheCourtEvenly")
{
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		INFO(Name);
		const std::vector<FFixture> Fixtures = BuildFixtures(Preset);
		CHECK((Preset.TargetAverageLux > 0.0) == Preset.bFloodlightsOn);
		if (!Preset.bFloodlightsOn)
		{
			CHECK(Preset.Sun.bEnabled);
			continue;
		}
		CHECK_FALSE(Fixtures.empty());
		const FIlluminanceReport Report = MeasureCourt(Fixtures);
		INFO("average " << Report.AverageLux << " lux, min " << Report.MinLux << ", uniformity " << Report.Uniformity);
		CHECK(Report.AverageLux >= Preset.TargetAverageLux);
		CHECK(Report.Uniformity >= Preset.TargetUniformity);
		// Racket-sport standards ask for at least 0.7 even at club level.
		CHECK(Report.Uniformity >= 0.7);
		// Not absurdly over-lit either: a hot spot would wash out the ball.
		CHECK(Report.MaxLux <= 2.0 * Report.AverageLux);
	}
}

TEST_CASE("Lighting.Data.BothHalvesAreLitTheSame")
{
	// The court is fair: neither team plays into darker corners.
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		INFO(Name);
		const std::vector<FFixture> Fixtures = BuildFixtures(Preset);
		for (const double X : { 0.5, 3.0, 6.95, 9.5 })
		{
			for (const double Y : { 0.5, 2.5, 4.5 })
			{
				const double Reference = LuxAt(Fixtures, X, Y);
				CHECK(LuxAt(Fixtures, -X, Y) == Approx(Reference).epsilon(0.001));
				CHECK(LuxAt(Fixtures, X, -Y) == Approx(Reference).epsilon(0.001));
			}
		}
	}
}

TEST_CASE("Lighting.Data.NoBeamPointsAtTheBroadcastCamera")
{
	// GDD 9: the camera sits behind either back glass and tracks up to 1.5 m sideways.
	const FCameraSettings Camera;
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		INFO(Name);
		const std::vector<FFixture> Fixtures = BuildFixtures(Preset);
		if (!Preset.bFloodlightsOn)
		{
			continue;
		}
		for (const int End : { 1, -1 })
		{
			for (const double TrackY : { -Camera.MaxTrackingM, 0.0, Camera.MaxTrackingM })
			{
				const FVec3 Lens = EndPose(Camera, End, TrackY).Location;
				INFO("camera at " << Lens.X << ", " << Lens.Y << ", " << Lens.Z);
				CHECK(GlareMarginDeg(Fixtures, Lens) >= 5.0);
			}
		}
	}
}

TEST_CASE("Lighting.Data.FixturesStayOutOfPlay")
{
	// GDD 2: indoors the ceiling is a fault at 8 m, so lights hang above it. Every fixture is
	// outside the side walls, so a player looking up at a lob never looks into a light.
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		INFO(Name);
		for (const FFixture& F : BuildFixtures(Preset))
		{
			CHECK(std::abs(F.Position.Y) > Padel::Court::HalfWidth + 0.3);
			if (Preset.Venue == EVenue::Indoor)
			{
				CHECK(F.Position.Z >= Padel::Court::CeilingHeight);
			}
			// Beams point down at the court.
			CHECK(F.Direction.Z < -0.5);
			CHECK(F.AttenuationRadiusM > 22.0);
		}
	}
}

TEST_CASE("Lighting.Data.ExposureMatchesTheLight")
{
	// The fixed broadcast exposure is within a stop of what a meter reads on the court.
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		INFO(Name);
		const std::vector<FFixture> Fixtures = BuildFixtures(Preset);
		const double Lux = Preset.bFloodlightsOn ? MeasureCourt(Fixtures).AverageLux : DaylightLux(Preset);
		CHECK(Lux > 0.0);
		CHECK(std::abs(Preset.Post.ExposureEV100 - MeteredEV100(Lux)) <= 1.0);
		// A usable lens: between f/1.4 and f/22.
		const double N = ApertureForEV100(Preset.Post.ExposureEV100, Preset.Post.ShutterSpeed, Preset.Post.ISO);
		CHECK(N >= 1.4);
		CHECK(N <= 22.0);
	}
}

TEST_CASE("Lighting.Data.ColourAndShadowBudget")
{
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		INFO(Name);
		const std::vector<FFixture> Fixtures = BuildFixtures(Preset);
		int Shadowed = 0;
		for (const FFixture& F : Fixtures)
		{
			Shadowed += F.bCastShadows ? 1 : 0;
			if (F.Role == EFixtureRole::House)
			{
				// Warm house light for the stands (ARENA_SPEC 6.2), never a shadow caster.
				CHECK(F.TemperatureK >= 2700.0);
				CHECK(F.TemperatureK <= 4500.0);
				CHECK_FALSE(F.bCastShadows);
				continue;
			}
			// Real sports LEDs: neutral to cool daylight.
			CHECK(F.TemperatureK >= 4000.0);
			CHECK(F.TemperatureK <= 6500.0);
			// The camera is white-balanced to the floodlights, so the lines stay white.
			if (Preset.bFloodlightsOn)
			{
				CHECK(std::abs(Preset.Post.WhiteBalanceK - F.TemperatureK) <= 500.0);
			}
		}
		// Shadow-casting local lights are the biggest cost on an M2 Pro (decision 0006).
		CHECK(Preset.MaxShadowedFixtures <= 4);
		CHECK(Shadowed <= Preset.MaxShadowedFixtures);
		CHECK((Shadowed > 0) == Preset.bFloodlightsOn);
		// Shadows are fair too: every shadowed fixture's mirror images are shadowed.
		for (const FFixture& F : Fixtures)
		{
			for (const FFixture& G : Fixtures)
			{
				const bool bMirror = std::abs(std::abs(F.Position.X) - std::abs(G.Position.X)) < 1e-6
					&& std::abs(std::abs(F.Position.Y) - std::abs(G.Position.Y)) < 1e-6;
				if (bMirror)
				{
					CHECK(F.bCastShadows == G.bCastShadows);
				}
			}
		}
		// The shadowed ones are the most central, so the players' shadows come from them.
		double FarthestShadowed = 0.0;
		double NearestPlain = 1e9;
		for (const FFixture& F : Fixtures)
		{
			if (F.Role != EFixtureRole::Court)
			{
				continue;
			}
			const double R = std::hypot(F.Position.X, F.Position.Y);
			if (F.bCastShadows)
			{
				FarthestShadowed = std::max(FarthestShadowed, R);
			}
			else
			{
				NearestPlain = std::min(NearestPlain, R);
			}
		}
		CHECK(FarthestShadowed <= NearestPlain + 1e-9);
	}
}

TEST_CASE("Lighting.Data.OneVenueOneRig")
{
	// The trusses and masts are venue meshes built from these rows (build_venue.py), so every
	// preset of a venue must put its fixtures in the same places.
	const LightingTestData::FLightingFile File = LightingTestData::Load();
	for (const auto& [NameA, A] : File.Presets)
	{
		for (const auto& [NameB, B] : File.Presets)
		{
			if (A.Venue != B.Venue)
			{
				continue;
			}
			INFO(NameA << " and " << NameB);
			const std::vector<FFixture> FA = BuildFixtures(A);
			const std::vector<FFixture> FB = BuildFixtures(B);
			REQUIRE(FA.size() == FB.size());
			for (size_t I = 0; I < FA.size(); ++I)
			{
				CHECK(FA[I].Position.X == Approx(FB[I].Position.X));
				CHECK(FA[I].Position.Y == Approx(FB[I].Position.Y));
				CHECK(FA[I].Position.Z == Approx(FB[I].Position.Z));
			}
		}
	}
}

TEST_CASE("Lighting.Data.NoLuminaireNearTheMiddleOfTheBroadcastPicture")
{
	// ARENA_SPEC 6.3: no luminaire within 15 degrees of the broadcast camera's view axis,
	// at either end, so nothing flares near the ball.
	const FCameraSettings Camera;
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		INFO(Name);
		const std::vector<FFixture> Fixtures = BuildFixtures(Preset);
		for (const int End : { 1, -1 })
		{
			const FCameraPose Pose = EndPose(Camera, End, 0.0);
			CHECK(NearestFixtureToViewAxisDeg(Fixtures, Pose.Location, Pose.LookAt) >= 15.0);
		}
	}
}

TEST_CASE("Lighting.Data.PlayersAreLitTowardBothCameras")
{
	// Faces and shirt fronts need light toward each broadcast camera, not just the floor.
	// Side-only rigs (no end trusses: ARENA_SPEC 6.1) give about a third of the floor level.
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		if (!Preset.bFloodlightsOn)
		{
			continue;
		}
		INFO(Name);
		const std::vector<FFixture> Fixtures = BuildFixtures(Preset);
		const double Horizontal = MeasureCourt(Fixtures).AverageLux;
		const double TowardPlusX = MeasureCourtVertical(Fixtures, 1);
		const double TowardMinusX = MeasureCourtVertical(Fixtures, -1);
		INFO("vertical " << TowardPlusX << " / " << TowardMinusX << " lux, horizontal " << Horizontal);
		CHECK(TowardPlusX >= 0.3 * Horizontal);
		CHECK(TowardPlusX == Approx(TowardMinusX).epsilon(0.001));
	}
}

TEST_CASE("Lighting.Data.StandsSitInADarkerRoom")
{
	// ARENA_SPEC 6.2: 150-250 lux on the seats, about 5:1 below the court, which is what
	// makes it read as television (a bright court in a darker room).
	for (const auto& [Name, Preset] : LightingTestData::Load().Presets)
	{
		if (Preset.Stands.MaxX <= Preset.Stands.MinX)
		{
			continue;
		}
		INFO(Name);
		const std::vector<FFixture> Fixtures = BuildFixtures(Preset);
		const FIlluminanceReport Stands = MeasureStands(Fixtures, Preset.Stands);
		const double Court = MeasureCourt(Fixtures).AverageLux;
		INFO("stands " << Stands.AverageLux << " lux (min " << Stands.MinLux << "), court " << Court);
		CHECK(Stands.AverageLux >= Preset.Stands.TargetMinLux);
		CHECK(Stands.AverageLux <= Preset.Stands.TargetMaxLux);
		CHECK(Court / Stands.AverageLux >= 3.0);
		CHECK(Court / Stands.AverageLux <= 8.0);
	}
}
