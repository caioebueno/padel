#include "Animation/Model/PlayerAnimTuning.h"

#include "CourtSpec.h"

#include <utility>

namespace Padel::Animation
{
	namespace PlayerAnimTuningDetail
	{
		// Shots that are played on both sides, so they need backhand clips too (GDD 4:
		// forehand or backhand by the ball's side; RB + Y on the backhand is a bandeja).
		bool HasBackhand(EShotType Shot)
		{
			switch (Shot)
			{
			case EShotType::Drive:
			case EShotType::Volley:
			case EShotType::Lob:
			case EShotType::Bandeja:
			case EShotType::Chiquita:
				return true;
			default:
				return false;
			}
		}

		void Positive(std::vector<std::string>& Out, const std::string& Where, const char* Field, double Value)
		{
			if (!(Value > 0.0))
			{
				Out.push_back(Where + ": " + Field + " must be > 0");
			}
		}
	}

	std::string FAnimClip::MakeName(EShotType Shot, ESwingSide Side, ESwingVariant Variant)
	{
		return std::string(ShotTypeName(Shot)) + "_" + SwingSideName(Side) + "_" + SwingVariantName(Variant);
	}

	std::vector<std::string> FAnimSettings::Validate() const
	{
		using PlayerAnimTuningDetail::Positive;
		std::vector<std::string> Out;
		const std::string W = "Animation.json";
		Positive(Out, W, "WalkMinMps", WalkMinMps);
		Positive(Out, W, "SprintMinMps", SprintMinMps);
		Positive(Out, W, "RunClipSpeedMps", RunClipSpeedMps);
		Positive(Out, W, "SprintClipSpeedMps", SprintClipSpeedMps);
		Positive(Out, W, "StrideRateMin", StrideRateMin);
		Positive(Out, W, "PlantedMaxSpeedMps", PlantedMaxSpeedMps);
		Positive(Out, W, "FullBodyBlendPerS", FullBodyBlendPerS);
		Positive(Out, W, "SwingBlendInS", SwingBlendInS);
		Positive(Out, W, "ClipCrossfadeS", ClipCrossfadeS);
		Positive(Out, W, "CelebrationS", CelebrationS);
		Positive(Out, W, "CelebrationBlendS", CelebrationBlendS);
		Positive(Out, W, "FacingTurnRateDegPerS", FacingTurnRateDegPerS);
		for (const auto& [Field, Value] : { std::pair<const char*, double>{ "FacingHalfLifeS", FacingHalfLifeS },
			{ "FacingChestHalfLifeS", FacingChestHalfLifeS }, { "FacingHeadHalfLifeS", FacingHeadHalfLifeS },
			{ "LeanHalfLifeS", LeanHalfLifeS }, { "PelvisHalfLifeS", PelvisHalfLifeS }, { "ReadyBounceHalfLifeS", ReadyBounceHalfLifeS } })
		{
			Positive(Out, W, Field, Value);
		}
		for (const auto& [Field, Value] : { std::pair<const char*, double>{ "FacingMaxChestTwistDeg", FacingMaxChestTwistDeg },
			{ "FacingMaxHeadTwistDeg", FacingMaxHeadTwistDeg }, { "LeanDegPerMps2", LeanDegPerMps2 }, { "LeanMaxDeg", LeanMaxDeg },
			{ "PlantDropPerMps2", PlantDropPerMps2 }, { "PlantDropMaxM", PlantDropMaxM }, { "SplitHopHeightM", SplitHopHeightM },
			{ "SplitLandDropM", SplitLandDropM }, { "SplitLandS", SplitLandS }, { "ReadyBounceM", ReadyBounceM }, { "ReadyBounceHz", ReadyBounceHz } })
		{
			if (Value < 0.0)
			{
				Out.push_back(W + ": " + Field + " must not be negative");
			}
		}
		if (FacingChestHalfLifeS > FacingHalfLifeS || FacingHeadHalfLifeS > FacingChestHalfLifeS)
		{
			Out.push_back(W + ": the head turns first, then the chest, then the pelvis (FacingHeadHalfLifeS <= FacingChestHalfLifeS <= FacingHalfLifeS)");
		}
		if (LeanMaxDeg > 30.0 || FacingMaxChestTwistDeg > 60.0 || FacingMaxHeadTwistDeg > 80.0)
		{
			Out.push_back(W + ": LeanMaxDeg <= 30, FacingMaxChestTwistDeg <= 60 and FacingMaxHeadTwistDeg <= 80");
		}
		if (GlassBackTurnDeg < 0.0 || GlassBackTurnDeg > 180.0 || GlassSideTurnDeg < 0.0 || GlassSideTurnDeg > 180.0)
		{
			Out.push_back(W + ": GlassBackTurnDeg and GlassSideTurnDeg must be 0 to 180");
		}
		for (int I = 0; I < NumStances; ++I)
		{
			const FStancePose& P = Stances[static_cast<std::size_t>(I)];
			const std::string Name = W + ": stance " + StanceName(static_cast<EStance>(I));
			if (P.CrouchAlpha < 0.0 || P.CrouchAlpha > 1.0 || P.ArmAlpha < 0.0 || P.ArmAlpha > 1.0)
			{
				Out.push_back(Name + ": CrouchAlpha and ArmAlpha must be 0 to 1");
			}
			if (P.WidenM < 0.0 || P.WidenM > 0.5 || P.DropM < 0.0 || P.DropM > 0.2 || P.TrunkDeg < -10.0 || P.TrunkDeg > 30.0)
			{
				Out.push_back(Name + ": WidenM 0 to 0.5, DropM 0 to 0.2, TrunkDeg -10 to 30");
			}
			if (P.Racket.UpM < 0.3 || P.Racket.UpM > 2.0 || P.Racket.ForwardM < -0.2 || P.Racket.ForwardM > 0.8)
			{
				Out.push_back(Name + ": the racket is carried 0.3 to 2 m up and -0.2 to 0.8 m forward");
			}
		}
		if (StanceNetUntilM <= 0.0 || StanceBackFromM <= StanceNetUntilM || StanceBackFromM > Court::HalfLength)
		{
			Out.push_back(W + ": 0 < StanceNetUntilM < StanceBackFromM <= 10");
		}
		for (const auto& [Field, Value] : { std::pair<const char*, double>{ "StanceHalfLifeS", StanceHalfLifeS }, { "HeadLookHalfLifeS", HeadLookHalfLifeS },
			{ "HeadLookEyeHeightM", HeadLookEyeHeightM } })
		{
			Positive(Out, W, Field, Value);
		}
		if (HeadLookMaxYawDeg < 0.0 || HeadLookMaxYawDeg > 80.0 || HeadLookMinPitchDeg > 0.0 || HeadLookMinPitchDeg < -60.0
			|| HeadLookMaxPitchDeg < 0.0 || HeadLookMaxPitchDeg > 70.0)
		{
			Out.push_back(W + ": HeadLookMaxYawDeg 0 to 80, HeadLookMinPitchDeg -60 to 0, HeadLookMaxPitchDeg 0 to 70");
		}
		if (StrideRateMax < StrideRateMin)
		{
			Out.push_back(W + ": StrideRateMax must be >= StrideRateMin");
		}
		if (SprintMinMps <= WalkMinMps)
		{
			Out.push_back(W + ": SprintMinMps must be above WalkMinMps");
		}
		if (CelebrationBlendS * 2.0 > CelebrationS)
		{
			Out.push_back(W + ": CelebrationBlendS must fit twice in CelebrationS");
		}
		if (TrajectoryTimesS.empty())
		{
			Out.push_back(W + ": TrajectoryTimesS is empty");
		}
		for (std::size_t I = 1; I < TrajectoryTimesS.size(); ++I)
		{
			if (TrajectoryTimesS[I] <= TrajectoryTimesS[I - 1])
			{
				Out.push_back(W + ": TrajectoryTimesS must increase");
			}
		}
		if (CelebrateAssets.empty() || DejectedAssets.empty())
		{
			Out.push_back(W + ": CelebrateAssets and DejectedAssets need at least one clip each");
		}
		return Out;
	}

	const char* StanceName(EStance Stance)
	{
		switch (Stance)
		{
		case EStance::Relaxed: return "Relaxed";
		case EStance::Serve: return "Serve";
		case EStance::Receive: return "Receive";
		case EStance::Net: return "Net";
		case EStance::Back: return "Back";
		default: return "?";
		}
	}

	bool ParseStance(const std::string& Text, EStance& Out)
	{
		for (int I = 0; I < NumStances; ++I)
		{
			if (Text == StanceName(static_cast<EStance>(I)))
			{
				Out = static_cast<EStance>(I);
				return true;
			}
		}
		return false;
	}

	const FAnimClip* FAnimClipLibrary::FindExact(EShotType Shot, ESwingSide Side, ESwingVariant Variant) const
	{
		for (const FAnimClip& Clip : Clips)
		{
			if (Clip.Shot == Shot && Clip.Side == Side && Clip.Variant == Variant)
			{
				return &Clip;
			}
		}
		return nullptr;
	}

	const FAnimClip* FAnimClipLibrary::Find(EShotType Shot, ESwingSide Side, ESwingVariant Variant) const
	{
		if (const FAnimClip* Clip = FindExact(Shot, Side, Variant))
		{
			return Clip;
		}
		if (const FAnimClip* Clip = FindExact(Shot, Side, ESwingVariant::Perfect))
		{
			return Clip;
		}
		if (const FAnimClip* Clip = FindExact(Shot, ESwingSide::Fore, Variant))
		{
			return Clip;
		}
		return FindExact(Shot, ESwingSide::Fore, ESwingVariant::Perfect);
	}

	std::vector<std::string> FAnimClipLibrary::Validate() const
	{
		using PlayerAnimTuningDetail::Positive;
		std::vector<std::string> Out;
		for (const FAnimClip& Clip : Clips)
		{
			const std::string W = "AnimClips.json " + Clip.Name;
			if (Clip.Name != FAnimClip::MakeName(Clip.Shot, Clip.Side, Clip.Variant))
			{
				Out.push_back(W + ": Name must be " + FAnimClip::MakeName(Clip.Shot, Clip.Side, Clip.Variant));
			}
			if (Clip.Shot == EShotType::Mishit || Clip.Shot == EShotType::Count)
			{
				Out.push_back(W + ": a mishit plays its button's family clip, it has no clip of its own");
			}
			Positive(Out, W, "WindupS", Clip.WindupS);
			if (!(Clip.ContactS > Clip.WindupS) || !(Clip.EndS > Clip.ContactS))
			{
				Out.push_back(W + ": times must be WindupS < ContactS < EndS");
			}
			if (Clip.Asset.rfind("/Game/Padel/Animations/", 0) != 0)
			{
				Out.push_back(W + ": Asset must be under /Game/Padel/Animations/");
			}
			if (FindExact(Clip.Shot, Clip.Side, Clip.Variant) != &Clip)
			{
				Out.push_back(W + ": duplicate row");
			}
		}
		// Every slice shot in early, perfect and late versions (the Agent 6 brief), on both
		// sides for shots played on both sides.
		for (int Index = 0; Index < static_cast<int>(EShotType::Mishit); ++Index)
		{
			const EShotType Shot = static_cast<EShotType>(Index);
			for (const ESwingSide Side : { ESwingSide::Fore, ESwingSide::Back })
			{
				if (Side == ESwingSide::Back && !PlayerAnimTuningDetail::HasBackhand(Shot))
				{
					continue;
				}
				for (const ESwingVariant Variant : { ESwingVariant::Early, ESwingVariant::Perfect, ESwingVariant::Late })
				{
					if (!FindExact(Shot, Side, Variant))
					{
						Out.push_back("AnimClips.json: missing " + FAnimClip::MakeName(Shot, Side, Variant));
					}
				}
			}
		}
		return Out;
	}

	bool FAnimClipLibrary::ParseShot(const std::string& Text, EShotType& Out)
	{
		for (int Index = 0; Index < static_cast<int>(EShotType::Count); ++Index)
		{
			if (Text == ShotTypeName(static_cast<EShotType>(Index)))
			{
				Out = static_cast<EShotType>(Index);
				return true;
			}
		}
		return false;
	}

	bool FAnimClipLibrary::ParseSide(const std::string& Text, ESwingSide& Out)
	{
		for (const ESwingSide Side : { ESwingSide::Fore, ESwingSide::Back })
		{
			if (Text == SwingSideName(Side))
			{
				Out = Side;
				return true;
			}
		}
		return false;
	}

	bool FAnimClipLibrary::ParseVariant(const std::string& Text, ESwingVariant& Out)
	{
		for (const ESwingVariant Variant : { ESwingVariant::Early, ESwingVariant::Perfect, ESwingVariant::Late })
		{
			if (Text == SwingVariantName(Variant))
			{
				Out = Variant;
				return true;
			}
		}
		return false;
	}
}
