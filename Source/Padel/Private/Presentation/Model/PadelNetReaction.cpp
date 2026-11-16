#include "Presentation/Model/PadelNetReaction.h"

#include "CourtSpec.h"
#include "MiniJson.h"

#include <algorithm>
#include <cmath>

namespace Padel::Presentation
{
	namespace
	{
		constexpr double NetPi = 3.14159265358979323846;

		// HLSL's smoothstep, so the CPU model and NetReaction.hlsl agree exactly.
		double SmoothStep(double Edge0, double Edge1, double X)
		{
			if (Edge1 <= Edge0)
			{
				return X < Edge0 ? 0.0 : 1.0;
			}
			const double T = std::clamp((X - Edge0) / (Edge1 - Edge0), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}

		double Gauss(double D, double Radius)
		{
			return std::exp(-(D * D) / (2.0 * Radius * Radius));
		}

		void ReadNumber(const Json::FValue& Object, const char* Key, double& Out)
		{
			if (const Json::FValue* Value = Object.Find(Key); Value && Value->Type == Json::EType::Number)
			{
				Out = Value->Number;
			}
		}
	}

	bool ReadNetReactionSettings(const Json::FValue& Root, FNetReactionSettings& Out)
	{
		const Json::FValue* Reaction = Root.Find("Reaction");
		if (!Reaction || Reaction->Type != Json::EType::Object)
		{
			return false;
		}
		ReadNumber(*Reaction, "DisplacementPerMps", Out.DisplacementPerMps);
		ReadNumber(*Reaction, "MaxDisplacementM", Out.MaxDisplacementM);
		ReadNumber(*Reaction, "MinSpeedMps", Out.MinSpeedMps);
		ReadNumber(*Reaction, "TapeClipScale", Out.TapeClipScale);
		ReadNumber(*Reaction, "BulgeRadiusM", Out.BulgeRadiusM);
		ReadNumber(*Reaction, "WaveSpeedMps", Out.WaveSpeedMps);
		ReadNumber(*Reaction, "WaveRadiusM", Out.WaveRadiusM);
		ReadNumber(*Reaction, "RippleShare", Out.RippleShare);
		ReadNumber(*Reaction, "FrequencyHz", Out.FrequencyHz);
		ReadNumber(*Reaction, "DampingPerSecond", Out.DampingPerSecond);
		ReadNumber(*Reaction, "DurationSeconds", Out.DurationSeconds);
		ReadNumber(*Reaction, "TapeShare", Out.TapeShare);
		ReadNumber(*Reaction, "PostInsetM", Out.PostInsetM);
		ReadNumber(*Reaction, "PostFadeM", Out.PostFadeM);
		ReadNumber(*Reaction, "GroundFadeM", Out.GroundFadeM);
		ReadNumber(*Reaction, "CableFadeM", Out.CableFadeM);
		ReadNumber(*Reaction, "DefaultSpeedMps", Out.DefaultSpeedMps);
		return true;
	}

	bool ParseNetReactionSettings(const std::string& Text, FNetReactionSettings& Out, std::string& Error)
	{
		Json::FValue Root;
		if (!Json::Parse(Text, Root, Error))
		{
			return false;
		}
		FNetReactionSettings Parsed;
		if (!ReadNetReactionSettings(Root, Parsed))
		{
			Error = "no \"Reaction\" object";
			return false;
		}
		Out = Parsed;
		return true;
	}

	FNetReaction::FNetReaction(const FNetReactionSettings& InSettings)
		: Settings(InSettings)
	{
	}

	int FNetReaction::OnNetContact(double X, double Y, double Z)
	{
		// A clip of the tape is reported at the tape, a ball into the net below it.
		const bool bTapeClip = Z >= Court::NetHeightAt(Y) - 1e-3;
		// Into the net: X is a ball radius on the side it came from, so it pushes the other way.
		// On the tape X is ~0 and the side is unknown; the ball carries on over, which pulls
		// the tape forward a little either way, so any sign looks right.
		const double Direction = X > 0.0 ? -1.0 : 1.0;
		const double Speed = LastShotSpeedMps >= 0.0 ? LastShotSpeedMps : Settings.DefaultSpeedMps;
		return AddImpact(Y, Z, Speed, Direction, bTapeClip);
	}

	int FNetReaction::AddImpact(double Y, double Z, double SpeedMps, double Direction, bool bTapeClip)
	{
		if (SpeedMps < Settings.MinSpeedMps)
		{
			return -1;
		}
		double Depth = std::min(SpeedMps * Settings.DisplacementPerMps, Settings.MaxDisplacementM);
		if (bTapeClip)
		{
			Depth *= Settings.TapeClipScale;
		}
		if (Depth <= 0.0)
		{
			return -1;
		}
		// A free slot, or else the oldest hit.
		int Slot = 0;
		for (int Index = 0; Index < MaxImpacts; ++Index)
		{
			if (!Impacts[Index].IsActive())
			{
				Slot = Index;
				break;
			}
			if (Impacts[Index].Age > Impacts[Slot].Age)
			{
				Slot = Index;
			}
		}
		FNetImpact& Impact = Impacts[Slot];
		Impact.Y = std::clamp(Y, -Court::HalfWidth, Court::HalfWidth);
		Impact.Z = std::clamp(Z, 0.0, Court::NetHeightAt(Y));
		Impact.Amplitude = Direction < 0.0 ? -Depth : Depth;
		Impact.Age = 0.0;
		return Slot;
	}

	void FNetReaction::Advance(double Seconds)
	{
		if (Seconds <= 0.0)
		{
			return;
		}
		for (FNetImpact& Impact : Impacts)
		{
			if (!Impact.IsActive())
			{
				continue;
			}
			Impact.Age += Seconds;
			if (Impact.Age >= Settings.DurationSeconds)
			{
				Impact = FNetImpact();
			}
		}
	}

	bool FNetReaction::IsActive() const
	{
		return std::any_of(Impacts.begin(), Impacts.end(), [](const FNetImpact& Impact) { return Impact.IsActive(); });
	}

	double FNetReaction::ImpactDisplacement(const FNetImpact& Impact, double Y, double Z) const
	{
		const FNetReactionSettings& S = Settings;
		const double Age = Impact.Age;
		if (!Impact.IsActive() || Age >= S.DurationSeconds)
		{
			return 0.0;
		}
		const double Time = std::exp(-S.DampingPerSecond * Age) * std::sin(2.0 * NetPi * S.FrequencyHz * Age)
			* (1.0 - SmoothStep(0.75 * S.DurationSeconds, S.DurationSeconds, Age));

		const double DY = Y - Impact.Y;
		const double DZ = Z - Impact.Z;
		const double Bulge = Gauss(DY, S.BulgeRadiusM) * Gauss(DZ, S.BulgeRadiusM);
		const double Travel = S.WaveSpeedMps * Age;
		const double Ripple = 0.5 * (Gauss(DY - Travel, S.WaveRadiusM) + Gauss(DY + Travel, S.WaveRadiusM)) * Gauss(DZ, S.WaveRadiusM);

		const double Edges = SmoothStep(S.PostInsetM, S.PostInsetM + S.PostFadeM, Court::HalfWidth - std::abs(Y));
		const double Ground = SmoothStep(0.0, S.GroundFadeM, Z);
		const double Top = Court::NetHeightAt(Y);
		const double Cable = 1.0 + (S.TapeShare - 1.0) * SmoothStep(Top - S.CableFadeM, Top, Z);

		const double Offset = Impact.Amplitude * Time * Edges * Ground * Cable * (Bulge + S.RippleShare * Ripple);
		return std::clamp(Offset, -S.MaxDisplacementM, S.MaxDisplacementM);
	}

	double FNetReaction::Displacement(double Y, double Z) const
	{
		double Sum = 0.0;
		for (const FNetImpact& Impact : Impacts)
		{
			Sum += ImpactDisplacement(Impact, Y, Z);
		}
		return std::clamp(Sum, -Settings.MaxDisplacementM, Settings.MaxDisplacementM);
	}

	FNetShaderParams FNetReaction::PackShaderParams() const
	{
		const double Cm = Court::UnrealUnitsPerMetre;
		const FNetReactionSettings& S = Settings;
		FNetShaderParams Out;
		for (int Index = 0; Index < MaxImpacts; ++Index)
		{
			const FNetImpact& Impact = Impacts[Index];
			Out.Impacts[Index] = { Impact.Y * Cm, Impact.Z * Cm, Impact.Amplitude * Cm, Impact.Age };
		}
		Out.Shape = { S.BulgeRadiusM * Cm, S.WaveSpeedMps * Cm, S.WaveRadiusM * Cm, S.RippleShare };
		Out.Motion = { S.FrequencyHz, S.DampingPerSecond, S.DurationSeconds, S.TapeShare };
		Out.Net = { Court::HalfWidth * Cm, Court::NetHeightCentre * Cm, Court::NetHeightPosts * Cm, S.PostFadeM * Cm };
		Out.Extra = { S.GroundFadeM * Cm, S.CableFadeM * Cm, S.MaxDisplacementM * Cm, S.PostInsetM * Cm };
		return Out;
	}
}
