#include "Animation/Model/RacketGrip.h"

#include "MiniJson.h"

#include <cmath>

namespace Padel::Animation
{
	namespace
	{
		constexpr double DegToRad = 3.14159265358979323846 / 180.0;

		void ReadNumber(const Json::FValue& Object, const char* Key, double& Out)
		{
			if (const Json::FValue* Value = Object.Find(Key); Value && Value->Type == Json::EType::Number)
			{
				Out = Value->Number;
			}
		}

		void ReadString(const Json::FValue& Object, const char* Key, std::string& Out)
		{
			if (const Json::FValue* Value = Object.Find(Key); Value && Value->Type == Json::EType::String)
			{
				Out = Value->String;
			}
		}

		// A with its component along unit B removed.
		Court::FVec3 Square(const Court::FVec3& A, const Court::FVec3& B)
		{
			return A - B * Court::Dot(A, B);
		}
	}

	bool ReadRacketSettings(const Json::FValue& Root, FRacketSettings& Out)
	{
		const Json::FValue* Racket = Root.Find("Racket");
		if (!Racket || Racket->Type != Json::EType::Object)
		{
			return false;
		}
		ReadString(*Racket, "Mesh", Out.Mesh);
		ReadNumber(*Racket, "FaceCentreM", Out.FaceCentreM);
		if (const Json::FValue* Grip = Racket->Find("Grip"); Grip && Grip->Type == Json::EType::Object)
		{
			ReadString(*Grip, "ForearmBone", Out.ForearmBone);
			ReadString(*Grip, "KnuckleBone", Out.KnuckleBone);
			ReadNumber(*Grip, "PalmShare", Out.PalmShare);
			ReadNumber(*Grip, "PalmLengthM", Out.PalmLengthM);
			ReadNumber(*Grip, "GripOutM", Out.GripOutM);
			ReadNumber(*Grip, "GripAlongM", Out.GripAlongM);
			ReadNumber(*Grip, "HeadTiltDeg", Out.HeadTiltDeg);
			ReadNumber(*Grip, "FaceRollDeg", Out.FaceRollDeg);
		}
		return true;
	}

	bool ParseRacketSettings(const std::string& Text, FRacketSettings& Out, std::string& Error)
	{
		Json::FValue Root;
		if (!Json::Parse(Text, Root, Error))
		{
			return false;
		}
		FRacketSettings Parsed = Out;
		if (!ReadRacketSettings(Root, Parsed))
		{
			Error = "no \"Racket\" object";
			return false;
		}
		Out = Parsed;
		return true;
	}

	FRacketGripFrame SolveRacketGrip(const FRacketGripInput& In, const FRacketSettings& Settings)
	{
		using namespace Court;
		FRacketGripFrame Out;
		const FVec3 ToKnuckle = In.Knuckle - In.Hand;
		const bool bKnuckle = In.bHasKnuckle && Size(ToKnuckle) > 1e-4;
		const FVec3 Along = bKnuckle ? ToKnuckle : In.Hand - In.Forearm;
		if (Size(Along) < 1e-4)
		{
			return Out;
		}
		const FVec3 F = SafeNormal(Along);
		Out.PalmCentre = bKnuckle ? In.Hand + ToKnuckle * Settings.PalmShare : In.Hand + F * Settings.PalmLengthM;

		// Palm down: the component's down, square to the hand. A hand pointing straight
		// down (never in a reference pose) falls back to the facing direction.
		FVec3 N = Square(-In.Up, F);
		if (Size(N) < 1e-3)
		{
			N = Square(-In.Forward, F);
		}
		N = SafeNormal(N);
		// Thumb side: across the palm, on the side the character faces.
		FVec3 T = SafeNormal(Cross(F, N));
		if (Dot(T, In.Forward) < 0.0)
		{
			T = -T;
		}
		Out.PalmNormal = N;

		const double Tilt = Settings.HeadTiltDeg * DegToRad;
		Out.AxisZ = SafeNormal(T * std::cos(Tilt) + F * std::sin(Tilt));
		// N is square to both F and T, so to the handle too: roll it about the handle.
		Out.AxisY = SafeNormal(RotateAround(N, Out.AxisZ, Settings.FaceRollDeg * DegToRad));
		Out.AxisX = Cross(Out.AxisY, Out.AxisZ);
		Out.Origin = Out.PalmCentre + N * Settings.GripOutM + Out.AxisZ * Settings.GripAlongM;
		Out.bValid = true;
		return Out;
	}
}
