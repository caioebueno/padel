#include "Presentation/Model/PadelLightingModel.h"

#include "CourtSpec.h"

#include <algorithm>
#include <cmath>

namespace Padel::Presentation::Lighting
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		// Incident-meter calibration constant for lux (ISO 2720).
		constexpr double IncidentMeterC = 250.0;

		double Radians(double Degrees) { return Degrees * Pi / 180.0; }
		double Degrees(double Radians) { return Radians * 180.0 / Pi; }
		double Dot(const FVec3& A, const FVec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		FVec3 Sub(const FVec3& A, const FVec3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
		double Length(const FVec3& V) { return std::sqrt(Dot(V, V)); }

		FVec3 Normalised(const FVec3& V)
		{
			const double L = Length(V);
			return L > 0.0 ? FVec3{ V.X / L, V.Y / L, V.Z / L } : FVec3{ 0.0, 0.0, -1.0 };
		}

		// One table per struct, so the JSON keys are written once.
		struct FNumberKey
		{
			std::string_view Key;
			double* (*Get)(FLightingPreset&);
		};

#define PADEL_NUMBER(KeyText, Member) FNumberKey{ KeyText, [](FLightingPreset& P) -> double* { return &P.Member; } }
		const FNumberKey NumberKeys[] = {
			PADEL_NUMBER("TargetAverageLux", TargetAverageLux),
			PADEL_NUMBER("TargetUniformity", TargetUniformity),
			PADEL_NUMBER("Sun.IlluminanceLux", Sun.IlluminanceLux),
			PADEL_NUMBER("Sun.ElevationDeg", Sun.ElevationDeg),
			PADEL_NUMBER("Sun.AzimuthDeg", Sun.AzimuthDeg),
			PADEL_NUMBER("Sun.TemperatureK", Sun.TemperatureK),
			PADEL_NUMBER("Sun.SourceAngleDeg", Sun.SourceAngleDeg),
			PADEL_NUMBER("Sky.SkyLightIntensity", Sky.SkyLightIntensity),
			PADEL_NUMBER("Sky.SkyIlluminanceLux", Sky.SkyIlluminanceLux),
			PADEL_NUMBER("Fog.Density", Fog.Density),
			PADEL_NUMBER("Fog.HeightFalloff", Fog.HeightFalloff),
			PADEL_NUMBER("Fog.VolumetricScattering", Fog.VolumetricScattering),
			PADEL_NUMBER("Fog.VolumetricAlbedo", Fog.VolumetricAlbedo),
			PADEL_NUMBER("Post.ExposureEV100", Post.ExposureEV100),
			PADEL_NUMBER("Post.ShutterSpeed", Post.ShutterSpeed),
			PADEL_NUMBER("Post.ISO", Post.ISO),
			PADEL_NUMBER("Post.WhiteBalanceK", Post.WhiteBalanceK),
			PADEL_NUMBER("Post.WhiteTint", Post.WhiteTint),
			PADEL_NUMBER("Post.Contrast", Post.Contrast),
			PADEL_NUMBER("Post.Saturation", Post.Saturation),
			PADEL_NUMBER("Post.BloomIntensity", Post.BloomIntensity),
			PADEL_NUMBER("Post.VignetteIntensity", Post.VignetteIntensity),
			PADEL_NUMBER("Post.MotionBlurAmount", Post.MotionBlurAmount),
			PADEL_NUMBER("Post.MotionBlurMax", Post.MotionBlurMax),
			PADEL_NUMBER("Post.LocalExposureHighlightContrast", Post.LocalExposureHighlightContrast),
			PADEL_NUMBER("Post.LocalExposureShadowContrast", Post.LocalExposureShadowContrast),
			PADEL_NUMBER("Post.FilmGrain", Post.FilmGrain),
			PADEL_NUMBER("Post.ChromaticAberration", Post.ChromaticAberration),
			PADEL_NUMBER("Post.LensFlareIntensity", Post.LensFlareIntensity),
			PADEL_NUMBER("Stands.MinX", Stands.MinX),
			PADEL_NUMBER("Stands.MaxX", Stands.MaxX),
			PADEL_NUMBER("Stands.FrontY", Stands.FrontY),
			PADEL_NUMBER("Stands.BackY", Stands.BackY),
			PADEL_NUMBER("Stands.FrontZ", Stands.FrontZ),
			PADEL_NUMBER("Stands.BackZ", Stands.BackZ),
			PADEL_NUMBER("Stands.TargetMinLux", Stands.TargetMinLux),
			PADEL_NUMBER("Stands.TargetMaxLux", Stands.TargetMaxLux),
		};
#undef PADEL_NUMBER

		struct FRowKey
		{
			std::string_view Key;
			double* (*Get)(FFixtureRow&);
		};

#define PADEL_ROW(Member) FRowKey{ #Member, [](FFixtureRow& R) -> double* { return &R.Member; } }
		const FRowKey RowKeys[] = {
			PADEL_ROW(StartX),
			PADEL_ROW(EndX),
			PADEL_ROW(OffsetY),
			PADEL_ROW(HeightM),
			PADEL_ROW(AimOwnSideM),
			PADEL_ROW(AimXScale),
			PADEL_ROW(IntensityCd),
			PADEL_ROW(InnerConeDeg),
			PADEL_ROW(OuterConeDeg),
			PADEL_ROW(TemperatureK),
			PADEL_ROW(SourceRadiusM),
			PADEL_ROW(LensLuminanceNits),
		};
#undef PADEL_ROW
	}

	bool SetPresetNumber(FLightingPreset& Preset, std::string_view Key, double Value)
	{
		if (Key == "MaxShadowedFixtures")
		{
			Preset.MaxShadowedFixtures = static_cast<int>(std::lround(Value));
			return true;
		}
		for (const FNumberKey& Entry : NumberKeys)
		{
			if (Entry.Key == Key)
			{
				*Entry.Get(Preset) = Value;
				return true;
			}
		}
		return false;
	}

	bool SetPresetFlag(FLightingPreset& Preset, std::string_view Key, bool bValue)
	{
		if (Key == "FloodlightsOn") { Preset.bFloodlightsOn = bValue; return true; }
		if (Key == "Sun.Enabled") { Preset.Sun.bEnabled = bValue; return true; }
		if (Key == "Sky.Atmosphere") { Preset.Sky.bAtmosphere = bValue; return true; }
		if (Key == "Sky.RealTimeCapture") { Preset.Sky.bRealTimeCapture = bValue; return true; }
		if (Key == "Sky.LowerHemisphereIsBlack") { Preset.Sky.bLowerHemisphereIsBlack = bValue; return true; }
		if (Key == "Fog.Volumetric") { Preset.Fog.bVolumetric = bValue; return true; }
		return false;
	}

	bool SetPresetText(FLightingPreset& Preset, std::string_view Key, std::string_view Value)
	{
		if (Key == "Venue")
		{
			if (Value == "Indoor") { Preset.Venue = EVenue::Indoor; return true; }
			if (Value == "Outdoor") { Preset.Venue = EVenue::Outdoor; return true; }
		}
		return false;
	}

	bool SetRowNumber(FFixtureRow& Row, std::string_view Key, double Value)
	{
		if (Key == "Count")
		{
			Row.Count = static_cast<int>(std::lround(Value));
			return true;
		}
		for (const FRowKey& Entry : RowKeys)
		{
			if (Entry.Key == Key)
			{
				*Entry.Get(Row) = Value;
				return true;
			}
		}
		return false;
	}

	bool SetRowText(FFixtureRow& Row, std::string_view Key, std::string_view Value)
	{
		if (Key == "Role")
		{
			if (Value == "Court") { Row.Role = EFixtureRole::Court; return true; }
			if (Value == "House") { Row.Role = EFixtureRole::House; return true; }
		}
		return false;
	}

	std::vector<FFixture> BuildFixtures(const FLightingPreset& Preset)
	{
		std::vector<FFixture> Fixtures;
		// The farthest court corner from any fixture sets its range, with headroom so the
		// engine's inverse-square window (which fades to 0 at the radius) stays negligible.
		const double CornerReach = std::sqrt(Court::Length * Court::Length + Court::Width * Court::Width);
		for (const FFixtureRow& Row : Preset.Rows)
		{
			for (const double Side : { -1.0, 1.0 })
			{
				for (int Index = 0; Index < Row.Count; ++Index)
				{
					const double T = Row.Count > 1 ? static_cast<double>(Index) / (Row.Count - 1) : 0.5;
					FFixture F;
					F.Role = Row.Role;
					F.Position = { Row.StartX + (Row.EndX - Row.StartX) * T, Side * Row.OffsetY, Row.HeightM };
					const FVec3 Aim{ F.Position.X * Row.AimXScale, Side * Row.AimOwnSideM, 0.0 };
					F.Direction = Normalised(Sub(Aim, F.Position));
					F.IntensityCd = Row.IntensityCd;
					F.InnerConeDeg = Row.InnerConeDeg;
					F.OuterConeDeg = Row.OuterConeDeg;
					F.TemperatureK = Row.TemperatureK;
					F.SourceRadiusM = Row.SourceRadiusM;
					F.LensLuminanceNits = Row.LensLuminanceNits;
					F.AttenuationRadiusM = 2.0 * std::max(CornerReach, Row.HeightM + Row.OffsetY);
					Fixtures.push_back(F);
				}
			}
		}

		// Nearest the centre first: those shadows land where the players are most of the time.
		// Fixtures at the same distance are a mirror group, taken whole or not at all, so both
		// halves (and both sides) get the same shadows.
		std::vector<size_t> Order;
		for (size_t I = 0; I < Fixtures.size(); ++I)
		{
			if (Fixtures[I].Role == EFixtureRole::Court)
			{
				Order.push_back(I);
			}
		}
		const auto Reach = [&Fixtures](size_t I)
		{
			return std::hypot(Fixtures[I].Position.X, Fixtures[I].Position.Y);
		};
		std::stable_sort(Order.begin(), Order.end(), [&Reach](size_t A, size_t B) { return Reach(A) < Reach(B) - 1e-6; });
		const size_t Budget = static_cast<size_t>(std::max(0, Preset.MaxShadowedFixtures));
		size_t Shadowed = 0;
		for (size_t Start = 0; Start < Order.size();)
		{
			size_t End = Start + 1;
			while (End < Order.size() && std::abs(Reach(Order[End]) - Reach(Order[Start])) <= 1e-6)
			{
				++End;
			}
			if (Shadowed + (End - Start) > Budget)
			{
				break;
			}
			for (size_t I = Start; I < End; ++I)
			{
				Fixtures[Order[I]].bCastShadows = true;
			}
			Shadowed += End - Start;
			Start = End;
		}
		return Fixtures;
	}

	double ConeFalloff(const FFixture& Fixture, const FVec3& Point)
	{
		// Same curve as the engine's SpotAttenuation: a squared smooth ramp between the cones.
		const double CosOuter = std::cos(Radians(Fixture.OuterConeDeg));
		const double CosInner = std::cos(Radians(std::min(Fixture.InnerConeDeg, Fixture.OuterConeDeg - 0.01)));
		const double CosAngle = Dot(Normalised(Sub(Point, Fixture.Position)), Fixture.Direction);
		const double Ramp = std::clamp((CosAngle - CosOuter) / (CosInner - CosOuter), 0.0, 1.0);
		return Ramp * Ramp;
	}

	double IlluminanceLux(const FFixture& Fixture, const FVec3& Point, const FVec3& Normal)
	{
		const FVec3 ToLight = Sub(Fixture.Position, Point);
		const double DistanceSq = Dot(ToLight, ToLight);
		if (DistanceSq <= 1e-9 || DistanceSq >= Fixture.AttenuationRadiusM * Fixture.AttenuationRadiusM)
		{
			return 0.0;
		}
		const double CosIncidence = Dot(Normalised(ToLight), Normal);
		if (CosIncidence <= 0.0)
		{
			return 0.0;
		}
		return Fixture.IntensityCd * ConeFalloff(Fixture, Point) * CosIncidence / DistanceSq;
	}

	FIlluminanceReport MeasureCourt(const std::vector<FFixture>& Fixtures, double GridSpacingM)
	{
		FIlluminanceReport Report;
		const double Spacing = std::max(GridSpacingM, 0.05);
		const int CountX = static_cast<int>(std::round(Court::Length / Spacing));
		const int CountY = static_cast<int>(std::round(Court::Width / Spacing));
		double Sum = 0.0;
		Report.MinLux = 1e300;
		for (int IX = 0; IX < CountX; ++IX)
		{
			for (int IY = 0; IY < CountY; ++IY)
			{
				// Cell centres, as in EN 12193 grids.
				const FVec3 Point{ -Court::HalfLength + (IX + 0.5) * Spacing, -Court::HalfWidth + (IY + 0.5) * Spacing, 0.0 };
				double Lux = 0.0;
				for (const FFixture& Fixture : Fixtures)
				{
					Lux += IlluminanceLux(Fixture, Point, FVec3{ 0.0, 0.0, 1.0 });
				}
				Sum += Lux;
				Report.MinLux = std::min(Report.MinLux, Lux);
				Report.MaxLux = std::max(Report.MaxLux, Lux);
				++Report.Samples;
			}
		}
		if (Report.Samples == 0)
		{
			Report.MinLux = 0.0;
			return Report;
		}
		Report.AverageLux = Sum / Report.Samples;
		Report.Uniformity = Report.AverageLux > 0.0 ? Report.MinLux / Report.AverageLux : 0.0;
		return Report;
	}

	double MeasureCourtVertical(const std::vector<FFixture>& Fixtures, int FacingX, double HeightM, double GridSpacingM)
	{
		const double Spacing = std::max(GridSpacingM, 0.05);
		const int CountX = static_cast<int>(std::round(Court::Length / Spacing));
		const int CountY = static_cast<int>(std::round(Court::Width / Spacing));
		const FVec3 Normal{ FacingX >= 0 ? 1.0 : -1.0, 0.0, 0.0 };
		double Sum = 0.0;
		for (int IX = 0; IX < CountX; ++IX)
		{
			for (int IY = 0; IY < CountY; ++IY)
			{
				const FVec3 Point{ -Court::HalfLength + (IX + 0.5) * Spacing, -Court::HalfWidth + (IY + 0.5) * Spacing, HeightM };
				for (const FFixture& Fixture : Fixtures)
				{
					Sum += IlluminanceLux(Fixture, Point, Normal);
				}
			}
		}
		return CountX * CountY > 0 ? Sum / (CountX * CountY) : 0.0;
	}

	FIlluminanceReport MeasureStands(const std::vector<FFixture>& Fixtures, const FStandsArea& Stands, double GridSpacingM)
	{
		FIlluminanceReport Report;
		if (Stands.MaxX <= Stands.MinX || Stands.BackY <= Stands.FrontY)
		{
			return Report;
		}
		const double Spacing = std::max(GridSpacingM, 0.05);
		const int CountX = std::max(1, static_cast<int>(std::round((Stands.MaxX - Stands.MinX) / Spacing)));
		const int CountY = std::max(1, static_cast<int>(std::round((Stands.BackY - Stands.FrontY) / Spacing)));
		double Sum = 0.0;
		Report.MinLux = 1e300;
		for (int IX = 0; IX < CountX; ++IX)
		{
			for (int IY = 0; IY < CountY; ++IY)
			{
				const double T = (IY + 0.5) / CountY;
				const FVec3 Point{ Stands.MinX + (IX + 0.5) * (Stands.MaxX - Stands.MinX) / CountX,
					Stands.FrontY + T * (Stands.BackY - Stands.FrontY), Stands.FrontZ + T * (Stands.BackZ - Stands.FrontZ) };
				double Lux = 0.0;
				for (const FFixture& Fixture : Fixtures)
				{
					Lux += IlluminanceLux(Fixture, Point, FVec3{ 0.0, 0.0, 1.0 });
				}
				Sum += Lux;
				Report.MinLux = std::min(Report.MinLux, Lux);
				Report.MaxLux = std::max(Report.MaxLux, Lux);
				++Report.Samples;
			}
		}
		Report.AverageLux = Sum / Report.Samples;
		Report.Uniformity = Report.AverageLux > 0.0 ? Report.MinLux / Report.AverageLux : 0.0;
		return Report;
	}

	double NearestFixtureToViewAxisDeg(const std::vector<FFixture>& Fixtures, const FVec3& Camera, const FVec3& LookAt)
	{
		const FVec3 Axis = Normalised(Sub(LookAt, Camera));
		double Nearest = 180.0;
		for (const FFixture& Fixture : Fixtures)
		{
			const double CosAngle = std::clamp(Dot(Normalised(Sub(Fixture.Position, Camera)), Axis), -1.0, 1.0);
			Nearest = std::min(Nearest, Degrees(std::acos(CosAngle)));
		}
		return Nearest;
	}

	double BeamAngleToDeg(const FFixture& Fixture, const FVec3& Point)
	{
		const double CosAngle = std::clamp(Dot(Normalised(Sub(Point, Fixture.Position)), Fixture.Direction), -1.0, 1.0);
		return Degrees(std::acos(CosAngle));
	}

	double GlareMarginDeg(const std::vector<FFixture>& Fixtures, const FVec3& Point)
	{
		double Margin = 180.0;
		for (const FFixture& Fixture : Fixtures)
		{
			Margin = std::min(Margin, BeamAngleToDeg(Fixture, Point) - Fixture.OuterConeDeg);
		}
		return Margin;
	}

	double DaylightLux(const FLightingPreset& Preset)
	{
		const double Sun = Preset.Sun.bEnabled
			? Preset.Sun.IlluminanceLux * std::max(0.0, std::sin(Radians(Preset.Sun.ElevationDeg)))
			: 0.0;
		return Sun + Preset.Sky.SkyIlluminanceLux;
	}

	double MeteredEV100(double Lux)
	{
		return std::log2(std::max(Lux, 1e-6) * 100.0 / IncidentMeterC);
	}

	double ApertureForEV100(double ExposureEV100, double ShutterSpeed, double ISO)
	{
		// EV100 = log2(N^2 / t) - log2(ISO / 100), with t = 1 / ShutterSpeed.
		const double Seconds = 1.0 / std::max(ShutterSpeed, 1e-6);
		return std::sqrt(std::exp2(ExposureEV100) * Seconds * std::max(ISO, 1e-6) / 100.0);
	}

	FVec3 SunDirection(const FSunSettings& Sun)
	{
		const double Elevation = Radians(Sun.ElevationDeg);
		const double Azimuth = Radians(Sun.AzimuthDeg);
		// The sun sits at (cos el cos az, cos el sin az, sin el); light travels the other way.
		return { -std::cos(Elevation) * std::cos(Azimuth), -std::cos(Elevation) * std::sin(Azimuth), -std::sin(Elevation) };
	}

	const char* VenueName(EVenue Venue)
	{
		return Venue == EVenue::Outdoor ? "Outdoor" : "Indoor";
	}
}
