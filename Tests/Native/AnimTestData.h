// Loads the real Data/Animation.json and Data/AnimClips.json into the animation model's
// structs for the native tests. Field names match FPadelAnimSettingsRow / FPadelAnimClipRow.
#pragma once

#include "Animation/Model/PlayerAnimTuning.h"
#include "ShotTestData.h"

namespace AnimTestData
{
	using namespace Padel::Animation;
	using JsonLite::FValue;
	using ShotTestData::DataPath;
	using ShotTestData::Num;

	inline FPlayerFrameVec Vec3(const FValue& R, const std::string& Prefix)
	{
		return { R[Prefix + "RightM"].Number, R[Prefix + "ForwardM"].Number, R[Prefix + "UpM"].Number };
	}

	inline std::vector<std::string> Strings(const FValue& V)
	{
		std::vector<std::string> Out;
		for (const FValue& Item : V.Array)
		{
			Out.push_back(Item.String);
		}
		return Out;
	}

	inline FAnimSettings LoadSettings()
	{
		const FValue R = ShotTestData::Row(JsonLite::ParseFile(DataPath("Animation.json")), "default");
		FAnimSettings S;
		S.WalkMinMps = Num(R, "WalkMinMps");
		S.SprintMinMps = Num(R, "SprintMinMps");
		S.RunClipSpeedMps = Num(R, "RunClipSpeedMps");
		S.SprintClipSpeedMps = Num(R, "SprintClipSpeedMps");
		S.StrideRateMin = Num(R, "StrideRateMin");
		S.StrideRateMax = Num(R, "StrideRateMax");
		S.ReadyBallWindowS = Num(R, "ReadyBallWindowS");
		S.PlantedMaxSpeedMps = Num(R, "PlantedMaxSpeedMps");
		S.FullBodyBlendPerS = Num(R, "FullBodyBlendPerS");
		S.SwingBlendInS = Num(R, "SwingBlendInS");
		S.ClipCrossfadeS = Num(R, "ClipCrossfadeS");
		S.CelebrationS = Num(R, "CelebrationS");
		S.CelebrationBlendS = Num(R, "CelebrationBlendS");
		S.ReadyRacket = Vec3(R, "ReadyRacket");
		S.FacingTurnRateDegPerS = Num(R, "FacingTurnRateDegPerS");
		S.GlassBackTurnDeg = Num(R, "GlassBackTurnDeg");
		S.GlassSideTurnDeg = Num(R, "GlassSideTurnDeg");
		S.ReadyCrouchAlphaNet = Num(R, "ReadyCrouchAlphaNet");
		S.ReadyCrouchAlphaBack = Num(R, "ReadyCrouchAlphaBack");
		for (const FValue& T : R["TrajectoryTimesS"].Array)
		{
			S.TrajectoryTimesS.push_back(T.Number);
		}
		S.CelebrateAssets = Strings(R["CelebrateAssets"]);
		S.DejectedAssets = Strings(R["DejectedAssets"]);
		return S;
	}

	inline FAnimClipLibrary LoadClips()
	{
		FAnimClipLibrary Lib;
		for (const FValue& R : JsonLite::ParseFile(DataPath("AnimClips.json")).Array)
		{
			FAnimClip C;
			C.Name = R["Name"].String;
			if (!FAnimClipLibrary::ParseShot(R["Shot"].String, C.Shot) || !FAnimClipLibrary::ParseSide(R["Side"].String, C.Side)
				|| !FAnimClipLibrary::ParseVariant(R["Variant"].String, C.Variant))
			{
				throw std::runtime_error("bad Shot/Side/Variant in AnimClips.json row " + C.Name);
			}
			C.Asset = R["Asset"].String;
			C.WindupS = Num(R, "WindupS");
			C.ContactS = Num(R, "ContactS");
			C.EndS = Num(R, "EndS");
			C.Backswing = Vec3(R, "Backswing");
			C.Follow = Vec3(R, "Follow");
			C.Contact = Vec3(R, "Contact");
			Lib.Add(C);
		}
		return Lib;
	}
}
