#include "BallPhysicsParams.h"

#include "MiniJson.h"

namespace Padel
{
	const FSurfaceParams& FBallPhysicsParams::Surface(Court::ESurface Which) const
	{
		switch (Which)
		{
		case Court::ESurface::Glass: return Glass;
		case Court::ESurface::Mesh: return Mesh;
		case Court::ESurface::Net: return Net;
		case Court::ESurface::Ceiling: return Ceiling;
		default: return Ground;
		}
	}

	bool FBallPhysicsParams::Validate(std::string& Error) const
	{
		auto Check = [&Error](bool bOk, const char* Field)
		{
			if (!bOk && Error.empty())
			{
				Error = std::string("invalid physics value: ") + Field;
			}
			return bOk;
		};
		bool bOk = true;
		bOk &= Check(Gravity > 0.0, "Gravity");
		bOk &= Check(DragCoefficient >= 0.0, "DragCoefficient");
		bOk &= Check(InertiaFactor > 0.0, "InertiaFactor");
		bOk &= Check(SubStepsPerTick >= 1 && SubStepsPerTick <= 64, "SubStepsPerTick");
		bOk &= Check(StuckSeconds > 0.0, "StuckSeconds");
		bOk &= Check(MaxFlightSeconds > 0.0, "MaxFlightSeconds");
		const FSurfaceParams* Surfaces[] = { &Ground, &Glass, &Mesh, &Net, &Ceiling };
		for (const FSurfaceParams* S : Surfaces)
		{
			bOk &= Check(S->Restitution >= 0.0 && S->Restitution <= 1.0, "Restitution");
			bOk &= Check(S->Friction >= 0.0, "Friction");
		}
		return bOk;
	}

	bool ParseBallPhysicsJson(const std::string& Text, FBallPhysicsParams& Out, std::string& Error)
	{
		Json::FValue Root;
		if (!Json::Parse(Text, Root, Error))
		{
			return false;
		}
		const Json::FValue* Row = &Root;
		if (Root.Type == Json::EType::Array)
		{
			Row = nullptr;
			for (const Json::FValue& Item : Root.Items)
			{
				const Json::FValue* Name = Item.Find("Name");
				if (Name && Name->Type == Json::EType::String && Name->String == "Default")
				{
					Row = &Item;
					break;
				}
			}
			if (!Row && !Root.Items.empty())
			{
				Row = &Root.Items.front();
			}
		}
		if (!Row || Row->Type != Json::EType::Object)
		{
			Error = "Physics.json has no row object";
			return false;
		}

		bool bOk = true;
		auto Read = [&](const char* Key, double& Value)
		{
			const Json::FValue* Field = Row->Find(Key);
			if (!Field || Field->Type != Json::EType::Number)
			{
				if (Error.empty())
				{
					Error = std::string("Physics.json is missing number ") + Key;
				}
				bOk = false;
				return;
			}
			Value = Field->Number;
		};
		auto ReadSurface = [&](const std::string& Prefix, FSurfaceParams& S)
		{
			Read((Prefix + "Restitution").c_str(), S.Restitution);
			Read((Prefix + "Friction").c_str(), S.Friction);
			Read((Prefix + "DeflectionMax").c_str(), S.DeflectionMax);
			Read((Prefix + "SpeedVariation").c_str(), S.SpeedVariation);
		};

		FBallPhysicsParams P;
		double SubSteps = 0.0;
		Read("Gravity", P.Gravity);
		Read("DragCoefficient", P.DragCoefficient);
		Read("MagnusCoefficient", P.MagnusCoefficient);
		Read("SpinDecayPerSecond", P.SpinDecayPerSecond);
		Read("InertiaFactor", P.InertiaFactor);
		Read("SubStepsPerTick", SubSteps);
		Read("RollingThreshold", P.RollingThreshold);
		Read("RollingDeceleration", P.RollingDeceleration);
		Read("NetTapeSpeedKeep", P.NetTapeSpeedKeep);
		Read("StuckSpeed", P.StuckSpeed);
		Read("StuckSeconds", P.StuckSeconds);
		Read("MaxFlightSeconds", P.MaxFlightSeconds);
		ReadSurface("Ground", P.Ground);
		ReadSurface("Glass", P.Glass);
		ReadSurface("Mesh", P.Mesh);
		ReadSurface("Net", P.Net);
		ReadSurface("Ceiling", P.Ceiling);
		P.SubStepsPerTick = static_cast<int>(SubSteps);
		if (!bOk || !P.Validate(Error))
		{
			return false;
		}
		Out = P;
		return true;
	}
}
