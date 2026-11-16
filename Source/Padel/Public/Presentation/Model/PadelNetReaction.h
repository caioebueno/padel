// How the net cloth moves when the ball hits it (GDD 2 net, GDD 3.4 ball_bounced on "net").
//
// Plain C++20 with no Unreal headers, so the native tests check it on every PR.
// UPadelNetReactionSubsystem feeds it the match's net contacts and copies PackShaderParams()
// into the material parameter collection MPC_NetReaction each frame. The net materials
// (M_PadelNet, M_PadelNetSolid, built by Tools/AssetPipeline/unreal/import_assets.py) run
// the same formula as Displacement() on the GPU, in Tools/AssetPipeline/unreal/NetReaction.hlsl,
// so any SM_CourtNet placed in any level reacts with no CPU mesh updates. Keep the two in step.
//
// The model is cosmetic: the ball's bounce off the net is PadelCore's (BallSim::CollideNet).
// The shape of one hit, at a point (Y, Z) on the net and a time Age since the hit:
//
//   bulge   = exp(-((Y - Y0)^2 + (Z - Z0)^2) / (2 BulgeRadius^2))       the pocket the ball makes
//   ripple  = travelling pulses along the cable, Y0 +- WaveSpeed * Age  the shiver along the net
//   edges   = 0 at the posts and the lacing, 1 a little way in           the net is laced to the posts
//   cable   = TapeShare at the tape, 1 lower down                        the steel cable is tight
//   ground  = 0 on the floor, 1 a little way up                          the bottom cord rests on the turf
//   time    = exp(-Damping Age) sin(2 pi Frequency Age), faded out by Duration
//   X offset = Amplitude * time * edges * cable * ground * (bulge + RippleShare * ripple)
//
// Amplitude is signed along X: the net gives way in the direction the ball was travelling.

#pragma once

#include <array>
#include <string>

namespace Padel::Json
{
	struct FValue;
}

namespace Padel::Presentation
{
	// Loaded from Data/Net.json ("Reaction"). Defaults match that file. Metres and seconds.
	struct FNetReactionSettings
	{
		// Pocket depth per m/s of ball speed, and its cap.
		double DisplacementPerMps = 0.006;
		double MaxDisplacementM = 0.16;
		// Contacts slower than this don't move the net visibly.
		double MinSpeedMps = 1.5;
		// A ball that clips the tape shakes the net much less than one that goes in.
		double TapeClipScale = 0.35;
		double BulgeRadiusM = 0.22;
		double WaveSpeedMps = 7.0;
		double WaveRadiusM = 0.30;
		double RippleShare = 0.45;
		double FrequencyHz = 3.0;
		double DampingPerSecond = 2.8;
		double DurationSeconds = 2.2;
		// How much the tape (on its tight steel cable) moves compared with the mesh below.
		double TapeShare = 0.30;
		// The net stops this far in from each side (the post and the lacing gap): nothing
		// moves outside it. Then the motion fades in over PostFadeM.
		double PostInsetM = 0.11;
		// Width of the fade to zero at each post, and above the ground.
		double PostFadeM = 0.35;
		double GroundFadeM = 0.12;
		// Height of the band under the tape over which the motion eases to TapeShare.
		double CableFadeM = 0.15;
		// Ball speed assumed when no ball_hit has been seen yet.
		double DefaultSpeedMps = 12.0;
	};

	// Reads a "Reaction" object like Data/Net.json's. Missing keys keep their defaults.
	// Returns false (and leaves Out at defaults) when Root has no "Reaction" object.
	bool ReadNetReactionSettings(const Json::FValue& Root, FNetReactionSettings& Out);

	// Parses the text of Data/Net.json. On failure Out keeps its defaults and Error says why.
	bool ParseNetReactionSettings(const std::string& Text, FNetReactionSettings& Out, std::string& Error);

	struct FNetImpact
	{
		double Y = 0.0;
		double Z = 0.0;
		// Signed pocket depth along X, metres. Zero means the slot is free.
		double Amplitude = 0.0;
		double Age = 0.0;

		bool IsActive() const { return Amplitude != 0.0; }
	};

	// Values for MPC_NetReaction, in Unreal units (cm) where they are lengths. The
	// material reads them by these names (see NetReaction.hlsl).
	struct FNetShaderParams
	{
		// Per slot: (Y cm, Z cm, Amplitude cm, Age s).
		std::array<std::array<double, 4>, 2> Impacts{};
		// (BulgeRadius cm, WaveSpeed cm/s, WaveRadius cm, RippleShare)
		std::array<double, 4> Shape{};
		// (Frequency Hz, Damping 1/s, Duration s, TapeShare)
		std::array<double, 4> Motion{};
		// (HalfWidth cm, NetHeightCentre cm, NetHeightPosts cm, PostFade cm)
		std::array<double, 4> Net{};
		// (GroundFade cm, CableFade cm, MaxDisplacement cm, PostInset cm)
		std::array<double, 4> Extra{};
	};

	class FNetReaction
	{
	public:
		// Two hits at once is plenty: a third replaces the older one.
		static constexpr int MaxImpacts = 2;

		explicit FNetReaction(const FNetReactionSettings& InSettings = FNetReactionSettings());

		const FNetReactionSettings& GetSettings() const { return Settings; }

		// The last ball_hit speed, used to size the next net contact (ball_bounced carries
		// no speed).
		void SetLastShotSpeed(double SpeedMps) { LastShotSpeedMps = SpeedMps; }

		// A ball_bounced on the net at (X, Y, Z) metres. The simulation puts a ball that
		// goes into the net a ball radius in front of it, so the sign of X is the side it
		// came from and the net gives way the other way. A contact on the tape is a clip.
		// Returns the slot used, or -1 when the hit is too soft to show.
		int OnNetContact(double X, double Y, double Z);

		// Adds a hit directly. Speed is the ball's speed into the net; Direction is +1 or -1 along X.
		int AddImpact(double Y, double Z, double SpeedMps, double Direction, bool bTapeClip);

		void Advance(double Seconds);

		// X offset of the rest-shape point (Y, Z), metres.
		double Displacement(double Y, double Z) const;

		// True while any hit is still moving the net.
		bool IsActive() const;

		const std::array<FNetImpact, MaxImpacts>& GetImpacts() const { return Impacts; }

		FNetShaderParams PackShaderParams() const;

	private:
		double ImpactDisplacement(const FNetImpact& Impact, double Y, double Z) const;

		FNetReactionSettings Settings;
		std::array<FNetImpact, MaxImpacts> Impacts{};
		double LastShotSpeedMps = -1.0;
	};
}
