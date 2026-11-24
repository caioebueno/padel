#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Animation/Model/PlayerAnimTuning.h"
#include "PadelAnimSettingsRow.generated.h"

/** P1: one stance in Animation.json "Stances". Meanings on Padel::Animation::FStancePose. */
USTRUCT(BlueprintType)
struct PADEL_API FPadelStanceRow
{
	GENERATED_BODY()

	/** Relaxed, Serve, Receive, Net or Back (Padel::Animation::EStance). */
	UPROPERTY(EditAnywhere, Category = "Stance") FString Name;
	UPROPERTY(EditAnywhere, Category = "Stance") double CrouchAlpha = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double RacketRightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double RacketForwardM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double RacketUpM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double ArmAlpha = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double WidenM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double DropM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double TrunkDeg = 0.0;
};

/**
 * The single row ("default") of Data/Animation.json (docs/decisions/0005-player-animation.md).
 * Field meanings are documented on Padel::Animation::FAnimSettings.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelAnimSettingsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Locomotion") double WalkMinMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double SprintMinMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double RunClipSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double SprintClipSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double StrideRateMin = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double StrideRateMax = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double ReadyBallWindowS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double PlantedMaxSpeedMps = 0.0;
	/** N1: how fast the body turns to its facing (square to the net, or toward travel in Crossover, TurnAndRun and Run), degrees a second. */
	UPROPERTY(EditAnywhere, Category = "Locomotion") double FacingTurnRateDegPerS = 0.0;
	/** N1c: degrees the body turns side-on from square to the net while it waits on a ball off the back glass, and off a side glass. */
	UPROPERTY(EditAnywhere, Category = "Locomotion") double GlassBackTurnDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double GlassSideTurnDeg = 0.0;
	/** P1b: degrees the body turns from square to the net running back with a ball to watch (side-on, racket shoulder back). */
	UPROPERTY(EditAnywhere, Category = "Locomotion") double BackRunTurnDeg = 0.0;
	/** P1 stances (one per Padel::Animation::EStance), the rally's net and back zones, and the stance spring. */
	UPROPERTY(EditAnywhere, Category = "Stance") TArray<FPadelStanceRow> Stances;
	UPROPERTY(EditAnywhere, Category = "Stance") double StanceNetUntilM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double StanceBackFromM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double StanceHalfLifeS = 0.0;
	/** P1: the head follows the ball. Meanings on Padel::Animation::FAnimSettings. */
	UPROPERTY(EditAnywhere, Category = "Stance") double HeadLookHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double HeadLookMaxYawDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double HeadLookMinPitchDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double HeadLookMaxPitchDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "Stance") double HeadLookEyeHeightM = 0.0;
	/** F1 body dynamics: facing springs, lean, plant drop, split-step hop and ready bounce. Meanings on Padel::Animation::FAnimSettings. */
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingChestHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingHeadHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingMaxChestTwistDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double FacingMaxHeadTwistDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double LeanDegPerMps2 = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double LeanMaxDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double LeanHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double PlantDropPerMps2 = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double PlantDropMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double PelvisHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double SplitHopHeightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double SplitLandDropM = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double SplitLandS = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double ReadyBounceM = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double ReadyBounceHz = 0.0;
	UPROPERTY(EditAnywhere, Category = "BodyDynamics") double ReadyBounceHalfLifeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double FullBodyBlendPerS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double SwingBlendInS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double ClipCrossfadeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Celebration") double CelebrationS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Celebration") double CelebrationBlendS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<double> TrajectoryTimesS;
	UPROPERTY(EditAnywhere, Category = "Celebration") TArray<FString> CelebrateAssets;
	UPROPERTY(EditAnywhere, Category = "Celebration") TArray<FString> DejectedAssets;

	/** Skeletal mesh per team (index 0 Team A, 1 Team B) and the thin AnimBP class (ABP_PadelPlayer). */
	UPROPERTY(EditAnywhere, Category = "Assets") TArray<FString> TeamMeshes;
	UPROPERTY(EditAnywhere, Category = "Assets") FString AnimClass;
	/** Bone the racket attaches to once a skeletal body is shown. */
	UPROPERTY(EditAnywhere, Category = "Assets") FString RacketBone;

	/**
	 * MetaHuman players (docs/decisions/0006-metahuman-players.md): the IK Retargeter from the
	 * placeholder skeleton to the MetaHuman body, the first LOD the MetaHumans may draw (their
	 * LOD 0 face is far more than a broadcast-distance player needs at 60 fps) and the last LOD
	 * that runs the retargeter's IK pass (-1 = every LOD).
	 */
	UPROPERTY(EditAnywhere, Category = "MetaHuman") FString MetaHumanRetargeter;
	UPROPERTY(EditAnywhere, Category = "MetaHuman") int32 MetaHumanMinLod = 0;
	UPROPERTY(EditAnywhere, Category = "MetaHuman") int32 MetaHumanIkLodThreshold = -1;
	/** The MetaHuman a player shows while their own (PlayerBodies.json) isn't on disk; empty for the placeholder. */
	UPROPERTY(EditAnywhere, Category = "MetaHuman") FString MetaHumanFallbackClass;
	/** Padel shoe with its sock, attached to each MetaHuman's feet (the 5.8 wardrobe has no footwear),
	 *  and the shoe's authored ankle-to-ball length, which is scaled to each MetaHuman's foot. */
	UPROPERTY(EditAnywhere, Category = "MetaHuman") FString MetaHumanShoeMesh;
	UPROPERTY(EditAnywhere, Category = "MetaHuman") double ShoeAnkleToBallM = 0.0;

	/**
	 * The C++ pose used when ABP_PadelPlayer isn't there (0006): idle, walk and jog cycles
	 * (forward, back, left, right) on the placeholder skeleton, the walk cycle's ground speed
	 * (the jog's is RunClipSpeedMps). How firmly the arm carries the racket is per stance.
	 */
	UPROPERTY(EditAnywhere, Category = "Locomotion") FString IdleAsset;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<FString> WalkAssets;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<FString> JogAssets;
	/** N1: the crouched ready idle and the upright walk cycles (forward, back, left, right) the stance mixes with by its CrouchAlpha. */
	UPROPERTY(EditAnywhere, Category = "Locomotion") FString ReadyAsset;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<FString> WalkUprightAssets;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double WalkClipSpeedMps = 0.0;

	/**
	 * M1 motion matching (docs/animation/MOVEMENT_FLUENCY.md): the Pose Search databases the
	 * C++ pose searches for its locomotion (built by Tools/AssetPipeline/unreal/animation_setup.py
	 * from the Game Animation Sample's Dense Stand set). Missing databases leave the loop mix above.
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") TArray<FString> MotionMatchingDatabases;
	/**
	 * Databases searched alongside MotionMatchingDatabases when they're built, skipped (one log
	 * line) when they aren't: the padel footwork from video (docs/decisions/0012-video-footwork.md)
	 * is made only on a machine that has the WHAM results and its SMPL licence, so a clone without
	 * it keeps the full set above instead of dropping to the loop mix. The chooser lists below may
	 * name them too.
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") TArray<FString> MotionMatchingExtraDatabases;
	/**
	 * The chooser (the sample's chooser table, as data): below MotionMatchingStillBelowMps and not
	 * speeding up, the search uses MotionMatchingStillDatabases (idles), the stop ones when the
	 * trajectory's past travelled MotionMatchingStopAfterM, and the turn-in-place ones once the
	 * body has turned MotionMatchingTurnInPlaceDeg over it; otherwise every other database
	 * (starts, loops, pivots).
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") TArray<FString> MotionMatchingStillDatabases;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") TArray<FString> MotionMatchingStopDatabases;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingStopAfterM = 0.2;
	/**
	 * Gaits, moving: MotionMatchingWalkDatabases up to MotionMatchingRunFromMps, the rest (run)
	 * from there, MotionMatchingSprintDatabases from MotionMatchingSprintFromMps, by the current
	 * speed; each band reaches MotionMatchingGaitOverlap (a share) past its edges.
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") TArray<FString> MotionMatchingWalkDatabases;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") TArray<FString> MotionMatchingSprintDatabases;
	/**
	 * Padel footwork (docs/decisions/0012-video-footwork.md): searched alone first while
	 * shuffling, in a point (any locomotion state but the between-points Idle), in the walk band,
	 * with the sideways share of the coming travel at least MotionMatchingShuffleSideShare. Its
	 * best pose plays when it costs at most MotionMatchingReadyMaxCost; otherwise the sample's
	 * databases are searched as anywhere else. Never searched outside the shuffle.
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") TArray<FString> MotionMatchingReadyDatabases;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingShuffleSideShare = 0.6;
	/**
	 * Moving (at least MotionMatchingStillBelowMps), a playing clip whose own root carries under
	 * this share of the player's speed is stalled, and if the chooser left its database out it
	 * may not carry on as the continuing pose (a stop's still tail under a walking player). 0: off.
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingStalledShare = 0.0;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingReadyMaxCost = 0.0;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingRunFromMps = 2.5;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingSprintFromMps = 5.0;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingGaitOverlap = 0.2;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") TArray<FString> MotionMatchingTurnInPlaceDatabases;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingStillBelowMps = 1.0;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingTurnInPlaceDeg = 30.0;
	/** Seconds between searches (0: every update). */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingSearchThrottleS = 0.0;
	/** Crossfade from one selected clip to the next. */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingBlendTimeS = 0.2;
	/** A result in the clip already playing closer than this to its playhead doesn't start a new blend. */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingPoseJumpThresholdS = 0.25;
	/** Play rate range that matches a clip's root speed to the simulation's speed (stride matching). */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingPlayRateMin = 0.8;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingPlayRateMax = 1.3;
	/** Clips blending at once; the oldest drops out past this. */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") int32 MotionMatchingMaxBlends = 4;
	/** A new pick waits until the latest is this far (0..1) through its crossfade. */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingRepickAfterBlend = 0.5;
	/** The pose history the search reads (its pose features): poses kept and seconds between them. */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") int32 MotionMatchingHistoryPoses = 10;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingHistorySamplingS = 0.04;
	/**
	 * M2: a switch by inertialization: the new clip plays alone at once, the difference from the
	 * pose that was showing decaying on a spring of this half-life. 0 keeps the crossfade above.
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingInertialHalfLifeS = 0.0;
	/** M2: a new candidate must beat the continuing pose's search cost by this much to switch. */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MotionMatchingSwitchCostMargin = 0.0;
	/**
	 * With mocap legs (motion matching on), the procedural leg layers that fight the clips fade
	 * with speed: the stance's crouch and feet widen, the pelvis drops (plant, bounce). Fully on
	 * up to MocapLegFadeFromMps, down to MocapLegKeep of themselves from MocapLegFadeToMps.
	 * The trunk lean keeps MocapLeanKeep of itself at speed (the clips lean on their own).
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MocapLegFadeFromMps = 0.3;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MocapLegFadeToMps = 1.5;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MocapLegKeep = 0.0;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MocapLeanKeep = 0.5;
	/**
	 * M2: at speed the clips own the trunk. The chest's facing lead keeps MocapChestTwistKeep of
	 * itself (its share of the look handed to the head), and the head turns at most
	 * MocapHeadMaxTwistDeg from the chest. Same fade as the legs.
	 */
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MocapChestTwistKeep = 1.0;
	UPROPERTY(EditAnywhere, Category = "MotionMatching") double MocapHeadMaxTwistDeg = 180.0;
	/**
	 * M2 locomotion warping (PadelLocomotionWarp.h): the hips turn toward the simulation's travel
	 * from the clip's by up to OrientationWarpMaxDeg (0: off) on OrientationWarpHalfLifeS, the
	 * spine turning OrientationWarpSpineCounter of it back; both moving at least OrientationWarpMinMps.
	 */
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double OrientationWarpMaxDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double OrientationWarpHalfLifeS = 0.08;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double OrientationWarpMinMps = 0.5;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double OrientationWarpSpineCounter = 1.0;
	/** Each foot's reach along the travel scales by the simulation's speed over the clip's, within StrideWarpMin..Max (both 1: off). */
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double StrideWarpMin = 1.0;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double StrideWarpMax = 1.0;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double StrideWarpHalfLifeS = 0.08;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double StrideWarpMinMps = 0.5;
	/**
	 * A clip leg longer than this share of the thigh and calf together is nearly straight: the
	 * stride warp fades out on it toward fully straight, where the knee's bend has no steady way
	 * and a small change in reach swings it round. 1: never fades.
	 */
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double StrideWarpStraightLegShare = 1.0;
	/**
	 * Foot locking: an ankle within FootLockHeightM of its standing height (0: off) and slower than
	 * FootLockSpeedMps in the clip's own world stays where it landed; it lets go above
	 * FootLockReleaseHeightM or FootLockReleaseSpeedMps, or FootLockMaxDriftM from the clip's
	 * foot, blending back over FootLockBlendOutS.
	 */
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double FootLockHeightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double FootLockSpeedMps = 0.3;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double FootLockReleaseHeightM = 0.06;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double FootLockReleaseSpeedMps = 0.6;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double FootLockMaxDriftM = 0.2;
	UPROPERTY(EditAnywhere, Category = "LocomotionWarp") double FootLockBlendOutS = 0.12;

	/**
	 * Contact warp (movement plan N3, shots doc 4.2, PadelContactWarp.h): how the reach for the
	 * ball splits across the body. Metres and degrees here; Unreal units once, at load.
	 */
	/** Of a target below the face, how much the pelvis drops (knee bend; the feet stay planted). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpPelvisDownShare = 0.8;
	/** The deepest knee bend. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxPelvisDropM = 0.5;
	/** Of the reach ahead, how much the pelvis shifts forward. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpPelvisForwardShare = 0.25;
	/** The longest forward pelvis shift. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxPelvisForwardM = 0.15;
	/** Of the sideways reach, how much the spine side-bends. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpSpineSideShare = 0.45;
	/** Of the reach ahead, how much the spine flexes forward. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpSpineForwardShare = 0.25;
	/** Of what's still below after the knees, how much the spine flexes. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpSpineDownShare = 0.75;
	/** The most the spine bends, both passes together. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxSpineDeg = 55.0;
	/** Of the angle from the racket shoulder to the ball, how much the trunk turns about the vertical. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpTrunkTurnShare = 0.7;
	/** The most the trunk turns. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxTrunkTurnDeg = 50.0;
	/** How far in front of the knee the leg IK aims it (knees bend forward). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpKneeBendM = 0.2;
	/** How far out the arm IK aims the elbow (it keeps the current pose's bend). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpElbowBendM = 0.2;
	/**
	 * How firmly the elbow keeps a steady down-and-out bend when the animated arm is nearly
	 * straight (a running arm swing): added to the pose's own elbow offset before the arm IK
	 * reads its bend from it, so a straight arm can't flip the elbow from frame to frame.
	 */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpElbowPoleSteadyM = 0.1;
	/** Of the arm's length, the furthest the hand goes before the racket turns along the arm (never a locked elbow). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxReachShare = 0.98;
	/** Of the arm's length, the closest the hand comes to the shoulder; nearer, the racket turns onto the target instead (a pivot past the carried racket). */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMinReachShare = 0.45;
	/** Of the hips' dip below standing (a clip's stop), how much the carried racket sinks with them while moving. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpCarryFollowShare = 1.0;
	/** How far above the shoulder a target has to be for the full overhead arm (long arm, elbow up and out); blended in from the shoulder. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpOverheadBlendM = 0.40;
	/** How far the forearm keeps from the spine (pelvis up to the top spine bone) through a swing: the torso half-depth plus a margin. 0 turns the guard off. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpTorsoClearanceM = 0.14;
	/** An overhead past the arm's reach: how high the player rises onto the toes, and how far the trunk leans towards the ball. */
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxToeRiseM = 0.12;
	UPROPERTY(EditAnywhere, Category = "ContactWarp") double WarpMaxOverheadLeanDeg = 30.0;

	Padel::Animation::FAnimSettings ToCore() const;
};
