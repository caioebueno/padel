// Court lighting maths (GDD 10), in metres on the simulation's axes.
//
// Plain C++20 with no Unreal headers, so the native tests check every preset in
// Data/Lighting.json on every PR. APadelVenueLighting loads a preset through the
// same Set* functions, builds the fixtures here and turns them into lights.
//
// Everything is physically based so the court looks like a real televised venue:
// - Floodlights are spot lights with an intensity in candela on the beam axis and the
//   engine's own cone falloff, so the lux we compute here is the lux Lumen renders.
// - Exposure is fixed, in EV100, like a broadcast camera with a locked iris. We check
//   it against what an incident light meter would read on the court.
// - A preset with floodlights must light the court evenly (average lux and Emin/Eavg)
//   and must never point a beam at the broadcast camera at either end (glare).

#pragma once

#include "Presentation/Model/PadelCameraRig.h"

#include <string>
#include <string_view>
#include <vector>

namespace Padel::Presentation::Lighting
{
	enum class EVenue
	{
		Indoor,
		Outdoor,
	};

	// Court floodlights light the play; house lights light the stands, never cast shadows
	// and don't count toward the court's targets.
	enum class EFixtureRole
	{
		Court,
		House,
	};

	// One line of identical floodlights along X, mirrored to both sides of the court.
	struct FFixtureRow
	{
		EFixtureRole Role = EFixtureRole::Court;
		int Count = 0;
		double StartX = 0.0;
		double EndX = 0.0;
		// Distance from the centre line (Y = 0) to the fixtures, on both sides.
		double OffsetY = 6.0;
		double HeightM = 8.6;
		// Each fixture aims at the ground this far from the centre line on its own side
		// (negative crosses the centre line), at X * AimXScale.
		double AimOwnSideM = 1.0;
		double AimXScale = 1.0;
		double IntensityCd = 0.0;
		// Half-angles, like the engine's spot light cones.
		double InnerConeDeg = 35.0;
		double OuterConeDeg = 50.0;
		double TemperatureK = 5700.0;
		double SourceRadiusM = 0.2;
		// Brightness of the fixture's lens as the camera sees it (cd/m^2).
		double LensLuminanceNits = 50000.0;
	};

	struct FSunSettings
	{
		bool bEnabled = false;
		double IlluminanceLux = 100000.0;
		double ElevationDeg = 55.0;
		// Where the light comes from, measured from +X toward +Y.
		double AzimuthDeg = 0.0;
		double TemperatureK = 6000.0;
		double SourceAngleDeg = 0.53;
	};

	struct FSkySettings
	{
		bool bAtmosphere = false;
		double SkyLightIntensity = 1.0;
		bool bRealTimeCapture = false;
		bool bLowerHemisphereIsBlack = true;
		// Diffuse sky light on the ground, for the exposure check only.
		double SkyIlluminanceLux = 0.0;
	};

	struct FFogSettings
	{
		double Density = 0.0;
		double HeightFalloff = 0.2;
		bool bVolumetric = false;
		double VolumetricScattering = 0.2;
		double VolumetricAlbedo = 0.9;
	};

	struct FPostSettings
	{
		double ExposureEV100 = 9.0;
		// 1 / seconds, like the engine's CameraShutterSpeed.
		double ShutterSpeed = 100.0;
		double ISO = 100.0;
		double WhiteBalanceK = 6500.0;
		double WhiteTint = 0.0;
		double Contrast = 1.0;
		double Saturation = 1.0;
		double BloomIntensity = 0.3;
		double VignetteIntensity = 0.2;
		double MotionBlurAmount = 0.25;
		double MotionBlurMax = 2.0;
		double LocalExposureHighlightContrast = 1.0;
		double LocalExposureShadowContrast = 1.0;
		double FilmGrain = 0.0;
		double ChromaticAberration = 0.0;
		double LensFlareIntensity = 0.0;
	};

	// The seating on each long side (mirrored), as a sloped plane through the seats, for
	// the house-light check. Empty (MaxX <= MinX) when the venue has no stands to light.
	struct FStandsArea
	{
		double MinX = 0.0;
		double MaxX = 0.0;
		double FrontY = 0.0;
		double BackY = 0.0;
		// Seat height at the front and back rows.
		double FrontZ = 0.0;
		double BackZ = 0.0;
		double TargetMinLux = 0.0;
		double TargetMaxLux = 0.0;
	};

	struct FLightingPreset
	{
		std::string Name;
		EVenue Venue = EVenue::Indoor;
		// The fixtures are part of the venue and always built; this says whether they shine.
		bool bFloodlightsOn = true;
		// 0 means the preset has no floodlight target (daylight).
		double TargetAverageLux = 0.0;
		double TargetUniformity = 0.0;
		// Shadow-casting local lights cost the most on Metal (docs/decisions/0009).
		int MaxShadowedFixtures = 4;
		FSunSettings Sun;
		FSkySettings Sky;
		FFogSettings Fog;
		std::vector<FFixtureRow> Rows;
		FStandsArea Stands;
		FPostSettings Post;
	};

	struct FFixture
	{
		EFixtureRole Role = EFixtureRole::Court;
		FVec3 Position;
		// Unit vector along the beam axis.
		FVec3 Direction;
		double IntensityCd = 0.0;
		double InnerConeDeg = 0.0;
		double OuterConeDeg = 0.0;
		double TemperatureK = 6500.0;
		double SourceRadiusM = 0.0;
		double LensLuminanceNits = 0.0;
		// Far enough that the engine's range window never dims the court.
		double AttenuationRadiusM = 0.0;
		bool bCastShadows = false;
	};

	struct FIlluminanceReport
	{
		double AverageLux = 0.0;
		double MinLux = 0.0;
		double MaxLux = 0.0;
		// Emin / Eavg, the uniformity sports lighting standards use.
		double Uniformity = 0.0;
		int Samples = 0;
	};

	// Loading. Keys are the JSON field names, nested objects as "Sun.IlluminanceLux".
	// Each returns false for an unknown key, so a typo in Data/Lighting.json is caught.
	bool SetPresetNumber(FLightingPreset& Preset, std::string_view Key, double Value);
	bool SetPresetFlag(FLightingPreset& Preset, std::string_view Key, bool bValue);
	bool SetPresetText(FLightingPreset& Preset, std::string_view Key, std::string_view Value);
	bool SetRowNumber(FFixtureRow& Row, std::string_view Key, double Value);
	bool SetRowText(FFixtureRow& Row, std::string_view Key, std::string_view Value);

	// Every fixture of a preset, rows in order, -Y side first. Up to MaxShadowedFixtures
	// of the court fixtures nearest the court centre cast shadows, in whole mirror groups.
	std::vector<FFixture> BuildFixtures(const FLightingPreset& Preset);

	// The engine's spot light falloff: 1 on the axis, 0 outside the outer cone.
	double ConeFalloff(const FFixture& Fixture, const FVec3& Point);

	// Illuminance in lux on a surface at Point facing Normal (unit), from one fixture.
	double IlluminanceLux(const FFixture& Fixture, const FVec3& Point, const FVec3& Normal);

	// Horizontal illuminance on the playing area (20 x 10 m) on a square grid.
	FIlluminanceReport MeasureCourt(const std::vector<FFixture>& Fixtures, double GridSpacingM = 0.5);

	// Average illuminance at HeightM on vertical planes facing +X (FacingX = 1) or -X, over
	// the playing area: how well faces and fronts are lit toward that end's camera.
	double MeasureCourtVertical(const std::vector<FFixture>& Fixtures, int FacingX, double HeightM = 1.0, double GridSpacingM = 1.0);

	// Horizontal illuminance on one side's stands (the +Y side; both sides are mirrored).
	FIlluminanceReport MeasureStands(const std::vector<FFixture>& Fixtures, const FStandsArea& Stands, double GridSpacingM = 0.5);

	// Angle in degrees between a fixture's beam axis and the direction to Point.
	double BeamAngleToDeg(const FFixture& Fixture, const FVec3& Point);

	// The smallest margin (degrees) by which Point stays outside every fixture's outer
	// cone. Negative means Point is in a beam.
	double GlareMarginDeg(const std::vector<FFixture>& Fixtures, const FVec3& Point);

	// The smallest angle (degrees) between a camera's view axis and the direction to any
	// fixture: how far the nearest luminaire sits from the middle of the picture.
	double NearestFixtureToViewAxisDeg(const std::vector<FFixture>& Fixtures, const FVec3& Camera, const FVec3& LookAt);

	// Daylight on the ground: the sun's horizontal component plus the sky.
	double DaylightLux(const FLightingPreset& Preset);

	// What an incident light meter reads for this illuminance (ISO 100, C = 250).
	double MeteredEV100(double Lux);

	// The f-number that gives ExposureEV100 at this shutter speed and ISO.
	double ApertureForEV100(double ExposureEV100, double ShutterSpeed, double ISO);

	// The sun's travel direction (unit vector, pointing from the sun to the ground).
	FVec3 SunDirection(const FSunSettings& Sun);

	const char* VenueName(EVenue Venue);
}
