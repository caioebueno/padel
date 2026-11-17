// Animation tuning (docs/decisions/0005-player-animation.md): Data/Animation.json (one
// "default" row) and Data/AnimClips.json (one row per swing clip). Plain C++ with no
// Unreal headers, so the native tests check the real files on every PR. The game reads
// the same files through DataTables (FPadelAnimData) and converts them to these structs.

#pragma once

#include "PadelTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Padel::Animation
{
	// Forehand or backhand, relative to the player's handedness.
	enum class ESwingSide : std::uint8_t { Fore, Back };

	// GDD 5.1 grades collapse to three animation versions: Perfect and Good share one.
	enum class ESwingVariant : std::uint8_t { Early, Perfect, Late };

	inline constexpr const char* SwingSideName(ESwingSide Side) { return Side == ESwingSide::Fore ? "fore" : "back"; }
	inline constexpr const char* SwingVariantName(ESwingVariant Variant)
	{
		return Variant == ESwingVariant::Early ? "early" : (Variant == ESwingVariant::Late ? "late" : "perfect");
	}

	// A small vector in the player's own frame: Right, Forward (toward the net), Up
	// (height above the ground), metres. Authored for a right-hander's forehand and
	// mirrored across for the other side.
	struct FPlayerFrameVec
	{
		double RightM = 0.0;
		double ForwardM = 0.0;
		double UpM = 0.0;
	};

	// One swing clip. Times are in the clip's own seconds: the backswing is full at
	// WindupS, the racket meets the ball at ContactS, the follow-through ends at EndS.
	struct FAnimClip
	{
		// Row name: <shot>_<side>_<variant>, e.g. "drive_fore_perfect".
		std::string Name;
		EShotType Shot = EShotType::Drive;
		ESwingSide Side = ESwingSide::Fore;
		ESwingVariant Variant = ESwingVariant::Perfect;
		// Unreal asset path of the UAnimSequence; may not exist yet (placeholder racket only).
		std::string Asset;
		double WindupS = 0.0;
		double ContactS = 0.0;
		double EndS = 0.0;
		// Placeholder racket keys (the racket face centre) for the procedural swing that
		// runs until clips exist. The contact key is not here: it is the ball.
		FPlayerFrameVec Backswing;
		FPlayerFrameVec Follow;
		// The racket sweet spot at ContactS in the authored clip, from the root, facing the
		// net (right, forward, up). All zero = not measured: no contact warp for this clip
		// (BIOMECHANICS_SHOTS 5).
		FPlayerFrameVec Contact;

		bool HasContactPoint() const { return Contact.RightM != 0.0 || Contact.ForwardM != 0.0 || Contact.UpM != 0.0; }

		static std::string MakeName(EShotType Shot, ESwingSide Side, ESwingVariant Variant);
	};

	// P1 (docs/animation/MOVEMENT_FLUENCY.md): where the player is and what the point is
	// doing decides how they stand. Partners of the server and receiver take Net and Back.
	enum class EStance : std::uint8_t
	{
		// Between points, or nothing to play: upright, racket down.
		Relaxed,
		// The server before the serve: side-on behind the service line, ball in hand.
		Serve,
		// The receiver before the serve: the widest, lowest stance in the game.
		Receive,
		// In a rally near the net: taller, racket head up at the chest, elbows in front.
		Net,
		// In a rally at the back: lower and wider, racket at the waist.
		Back,
		Count,
	};

	inline constexpr int NumStances = static_cast<int>(EStance::Count);
	const char* StanceName(EStance Stance);
	bool ParseStance(const std::string& Text, EStance& Out);

	// One stance, as the pose holds it while standing (Data/Animation.json "Stances").
	struct FStancePose
	{
		// Weight of the crouched ready clips against the upright ones, 0 to 1.
		double CrouchAlpha = 0.0;
		// Where the racket face centre is carried, in the player's frame (Up is the height
		// above the ground), for a right-hander's forehand side.
		FPlayerFrameVec Racket;
		// How firmly the arm holds the racket there over the clip's arm, 0 to 1.
		double ArmAlpha = 0.0;
		// Feet this much further apart than the clips put them, metres.
		double WidenM = 0.0;
		// Knees bent this much more (pelvis lower, feet planted), metres.
		double DropM = 0.0;
		// Trunk tipped forward over the knees, degrees.
		double TrunkDeg = 0.0;
	};

	struct FAnimSettings
	{
		// Below this planar speed the legs stand (idle or ready stance).
		double WalkMinMps = 0.0;
		// Sprint state when the sprint is held and the player is at least this fast.
		double SprintMinMps = 0.0;
		// Ground speed the run and sprint cycles were authored at, for stride matching.
		double RunClipSpeedMps = 0.0;
		double SprintClipSpeedMps = 0.0;
		double StrideRateMin = 0.0;
		double StrideRateMax = 0.0;
		// Ready stance (knees bent) when a ball is coming within this many seconds.
		double ReadyBallWindowS = 0.0;
		// A swing owns the legs only while the player moves slower than this; above it
		// the swing plays on the upper body over the locomotion, so feet never slide.
		double PlantedMaxSpeedMps = 0.0;
		// How fast the full-body weight follows its target, per second.
		double FullBodyBlendPerS = 0.0;
		// Swing layer blend-in at the press.
		double SwingBlendInS = 0.0;
		// Crossfade when the clip changes (side switch in the windup, or the played shot
		// replacing the family clip at contact). Both clips share the swing progress, so
		// each shows its own contact frame on the contact tick.
		double ClipCrossfadeS = 0.0;
		double CelebrationS = 0.0;
		double CelebrationBlendS = 0.0;
		// N1 (movement doc 3.3, 5.3): how fast the body turns to its facing, degrees a second.
		// Square to the net by default; toward travel in Crossover, TurnAndRun and Run.
		double FacingTurnRateDegPerS = 0.0;
		// N1c (movement doc 2.10): reading a ball off the glass, the body turns side-on, away
		// from square to the net by this many degrees. Back glass: toward the side the ball
		// comes on (forehand or backhand), so the racket shoulder is toward the glass. Side
		// glass: toward that wall.
		double GlassBackTurnDeg = 0.0;
		double GlassSideTurnDeg = 0.0;
		// P1b: running back with a ball to watch, the body turns side-on (racket shoulder back)
		// by this much from square to the net, instead of all the way round.
		double BackRunTurnDeg = 0.0;
		// P1 stances (movement doc 2.1, the motion catalogue): one pose per EStance. In a rally
		// the stance is Net up to StanceNetUntilM from the net, Back from StanceBackFromM, and a
		// blend between; every change is followed on StanceHalfLifeS.
		std::array<FStancePose, NumStances> Stances{};
		double StanceNetUntilM = 0.0;
		double StanceBackFromM = 0.0;
		double StanceHalfLifeS = 0.0;
		// P1: the head follows the ball (in a rally, and the receivers watch the server's ball),
		// on HeadLookHalfLifeS, at most HeadLookMaxYawDeg either side of the chest and between
		// HeadLookMinPitchDeg (down) and HeadLookMaxPitchDeg (up), measured from the eyes.
		double HeadLookHalfLifeS = 0.0;
		double HeadLookMaxYawDeg = 0.0;
		double HeadLookMinPitchDeg = 0.0;
		double HeadLookMaxPitchDeg = 0.0;
		double HeadLookEyeHeightM = 0.0;
		// F1 body dynamics (docs/animation/MOVEMENT_FLUENCY.md). Springs are critically damped
		// and set by half-life: the time to close half the gap.
		// Facing: the pelvis follows the facing target on FacingHalfLifeS (at most
		// FacingTurnRateDegPerS), the chest and head on their own shorter half-lives, so a turn
		// starts at the eyes and ends at the hips. The twists are capped against the body below.
		double FacingHalfLifeS = 0.0;
		double FacingChestHalfLifeS = 0.0;
		double FacingHeadHalfLifeS = 0.0;
		double FacingMaxChestTwistDeg = 0.0;
		double FacingMaxHeadTwistDeg = 0.0;
		// Lean into the movement's acceleration (forward on a start, back on a brake, into a
		// turn), degrees per m/s^2 of the acceleration smoothed over LeanHalfLifeS, up to LeanMaxDeg.
		double LeanDegPerMps2 = 0.0;
		double LeanMaxDeg = 0.0;
		double LeanHalfLifeS = 0.0;
		// Braking sinks the pelvis (knees take the load; feet stay planted): metres per m/s^2
		// of deceleration, up to PlantDropMaxM, followed on PelvisHalfLifeS.
		double PlantDropPerMps2 = 0.0;
		double PlantDropMaxM = 0.0;
		double PelvisHalfLifeS = 0.0;
		// The split step shows as a hop this high over FPlayerMotion's hop, then a landing
		// that loads the legs by SplitLandDropM for SplitLandS.
		double SplitHopHeightM = 0.0;
		double SplitLandDropM = 0.0;
		double SplitLandS = 0.0;
		// Standing in the ready stance in a rally, a light bounce on the toes: a dip this deep,
		// this many times a second, faded in and out on ReadyBounceHalfLifeS.
		double ReadyBounceM = 0.0;
		double ReadyBounceHz = 0.0;
		double ReadyBounceHalfLifeS = 0.0;
		// Motion Matching trajectory sample times (negative = past), seconds.
		std::vector<double> TrajectoryTimesS;
		std::vector<std::string> CelebrateAssets;
		std::vector<std::string> DejectedAssets;

		std::vector<std::string> Validate() const;
	};

	class FAnimClipLibrary
	{
	public:
		void Add(const FAnimClip& Clip) { Clips.push_back(Clip); }
		const std::vector<FAnimClip>& All() const { return Clips; }

		// Exact clip, else the perfect version on that side, else the forehand.
		const FAnimClip* Find(EShotType Shot, ESwingSide Side, ESwingVariant Variant) const;
		const FAnimClip* FindExact(EShotType Shot, ESwingSide Side, ESwingVariant Variant) const;

		// Rows that are broken, or slice shots without all three versions on the forehand.
		std::vector<std::string> Validate() const;

		// Parses "drive" / "fore" / "early"; false when the text is not a known name.
		static bool ParseShot(const std::string& Text, EShotType& Out);
		static bool ParseSide(const std::string& Text, ESwingSide& Out);
		static bool ParseVariant(const std::string& Text, ESwingVariant& Out);

	private:
		std::vector<FAnimClip> Clips;
	};
}
